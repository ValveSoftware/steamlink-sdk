/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/dom/AXObjectCache.h"
#include "core/layout/BidiRunForLine.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutRubyRun.h"
#include "core/layout/LayoutView.h"
#include "core/layout/VerticalPositionCache.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/line/BreakingContextInlineHeaders.h"
#include "core/layout/line/GlyphOverflow.h"
#include "core/layout/line/LayoutTextInfo.h"
#include "core/layout/line/LineLayoutState.h"
#include "core/layout/line/LineWidth.h"
#include "core/layout/line/WordMeasurement.h"
#include "core/layout/svg/line/SVGRootInlineBox.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/Character.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

using namespace WTF::Unicode;

class ExpansionOpportunities {
public:
    ExpansionOpportunities()
    : m_totalOpportunities(0)
    { }

    void addRunWithExpansions(BidiRun& run, bool& isAfterExpansion, TextJustify textJustify)
    {
        LineLayoutText text = LineLayoutText(run.m_lineLayoutItem);
        unsigned opportunitiesInRun;
        if (text.is8Bit()) {
            opportunitiesInRun = Character::expansionOpportunityCount(text.characters8() + run.m_start,
                run.m_stop - run.m_start, run.m_box->direction(), isAfterExpansion, textJustify);
        } else {
            opportunitiesInRun = Character::expansionOpportunityCount(text.characters16() + run.m_start,
                run.m_stop - run.m_start, run.m_box->direction(), isAfterExpansion, textJustify);
        }
        m_runsWithExpansions.append(opportunitiesInRun);
        m_totalOpportunities += opportunitiesInRun;
    }
    void removeTrailingExpansion()
    {
        if (!m_totalOpportunities || !m_runsWithExpansions.last())
            return;
        m_runsWithExpansions.last()--;
        m_totalOpportunities--;
    }

    unsigned count() { return m_totalOpportunities; }

    unsigned opportunitiesInRun(size_t run) { return m_runsWithExpansions[run]; }

    void computeExpansionsForJustifiedText(BidiRun* firstRun, BidiRun* trailingSpaceRun, LayoutUnit& totalLogicalWidth, LayoutUnit availableLogicalWidth)
    {
        if (!m_totalOpportunities || availableLogicalWidth <= totalLogicalWidth)
            return;

        size_t i = 0;
        for (BidiRun* r = firstRun; r; r = r->next()) {
            if (!r->m_box || r == trailingSpaceRun)
                continue;

            if (r->m_lineLayoutItem.isText()) {
                unsigned opportunitiesInRun = m_runsWithExpansions[i++];

                RELEASE_ASSERT(opportunitiesInRun <= m_totalOpportunities);

                // Don't justify for white-space: pre.
                if (r->m_lineLayoutItem.style()->whiteSpace() != PRE) {
                    InlineTextBox* textBox = toInlineTextBox(r->m_box);
                    RELEASE_ASSERT(m_totalOpportunities);
                    int expansion = (availableLogicalWidth - totalLogicalWidth) * opportunitiesInRun / m_totalOpportunities;
                    textBox->setExpansion(expansion);
                    totalLogicalWidth += expansion;
                }
                m_totalOpportunities -= opportunitiesInRun;
                if (!m_totalOpportunities)
                    break;
            }
        }
    }
private:
    Vector<unsigned, 16> m_runsWithExpansions;
    unsigned m_totalOpportunities;
};

static inline InlineBox* createInlineBoxForLayoutObject(LineLayoutItem lineLayoutItem, bool isRootLineBox, bool isOnlyRun = false)
{
    // Callers should handle text themselves.
    ASSERT(!lineLayoutItem.isText());

    if (isRootLineBox)
        return LineLayoutBlockFlow(lineLayoutItem).createAndAppendRootInlineBox();

    if (lineLayoutItem.isBox())
        return LineLayoutBox(lineLayoutItem).createInlineBox();

    return LineLayoutInline(lineLayoutItem).createAndAppendInlineFlowBox();
}

static inline InlineTextBox* createInlineBoxForText(BidiRun& run, bool isOnlyRun)
{
    ASSERT(run.m_lineLayoutItem.isText());
    LineLayoutText text = LineLayoutText(run.m_lineLayoutItem);
    InlineTextBox* textBox = text.createInlineTextBox(run.m_start, run.m_stop - run.m_start);
    // We only treat a box as text for a <br> if we are on a line by ourself or in strict mode
    // (Note the use of strict mode.  In "almost strict" mode, we don't treat the box for <br> as text.)
    if (text.isBR())
        textBox->setIsText(isOnlyRun || text.document().inNoQuirksMode());
    textBox->setDirOverride(run.dirOverride(text.style()->rtlOrdering() == VisualOrder));
    if (run.m_hasHyphen)
        textBox->setHasHyphen(true);
    return textBox;
}

static inline void dirtyLineBoxesForObject(LayoutObject* o, bool fullLayout)
{
    if (o->isText()) {
        LayoutText* layoutText = toLayoutText(o);
        layoutText->dirtyOrDeleteLineBoxesIfNeeded(fullLayout);
    } else {
        toLayoutInline(o)->dirtyLineBoxes(fullLayout);
    }
}

static bool parentIsConstructedOrHaveNext(InlineFlowBox* parentBox)
{
    do {
        if (parentBox->isConstructed() || parentBox->nextOnLine())
            return true;
        parentBox = parentBox->parent();
    } while (parentBox);
    return false;
}

InlineFlowBox* LayoutBlockFlow::createLineBoxes(LineLayoutItem lineLayoutItem, const LineInfo& lineInfo, InlineBox* childBox)
{
    // See if we have an unconstructed line box for this object that is also
    // the last item on the line.
    unsigned lineDepth = 1;
    InlineFlowBox* parentBox = nullptr;
    InlineFlowBox* result = nullptr;
    do {
        ASSERT_WITH_SECURITY_IMPLICATION(lineLayoutItem.isLayoutInline() || lineLayoutItem.isEqual(this));

        LineLayoutInline inlineFlow(!lineLayoutItem.isEqual(this) ? lineLayoutItem : nullptr);

        // Get the last box we made for this layout object.
        parentBox = inlineFlow ? inlineFlow.lastLineBox() : LineLayoutBlockFlow(lineLayoutItem).lastLineBox();

        // If this box or its ancestor is constructed then it is from a previous line, and we need
        // to make a new box for our line.  If this box or its ancestor is unconstructed but it has
        // something following it on the line, then we know we have to make a new box
        // as well.  In this situation our inline has actually been split in two on
        // the same line (this can happen with very fancy language mixtures).
        bool constructedNewBox = false;
        bool allowedToConstructNewBox = !inlineFlow || inlineFlow.alwaysCreateLineBoxes();
        bool canUseExistingParentBox = parentBox && !parentIsConstructedOrHaveNext(parentBox);
        if (allowedToConstructNewBox && !canUseExistingParentBox) {
            // We need to make a new box for this layout object.  Once
            // made, we need to place it at the end of the current line.
            InlineBox* newBox = createInlineBoxForLayoutObject(LineLayoutItem(lineLayoutItem), lineLayoutItem.isEqual(this));
            ASSERT_WITH_SECURITY_IMPLICATION(newBox->isInlineFlowBox());
            parentBox = toInlineFlowBox(newBox);
            parentBox->setFirstLineStyleBit(lineInfo.isFirstLine());
            parentBox->setIsHorizontal(isHorizontalWritingMode());
            constructedNewBox = true;
        }

        if (constructedNewBox || canUseExistingParentBox) {
            if (!result)
                result = parentBox;

            // If we have hit the block itself, then |box| represents the root
            // inline box for the line, and it doesn't have to be appended to any parent
            // inline.
            if (childBox)
                parentBox->addToLine(childBox);

            if (!constructedNewBox || lineLayoutItem.isEqual(this))
                break;

            childBox = parentBox;
        }

        // If we've exceeded our line depth, then jump straight to the root and skip all the remaining
        // intermediate inline flows.
        lineLayoutItem = (++lineDepth >= cMaxLineDepth) ? LineLayoutItem(this) : lineLayoutItem.parent();

    } while (true);

    return result;
}

template <typename CharacterType>
static inline bool endsWithASCIISpaces(const CharacterType* characters, unsigned pos, unsigned end)
{
    while (isASCIISpace(characters[pos])) {
        pos++;
        if (pos >= end)
            return true;
    }
    return false;
}

static bool reachedEndOfTextRun(const BidiRunList<BidiRun>& bidiRuns)
{
    BidiRun* run = bidiRuns.logicallyLastRun();
    if (!run)
        return true;
    unsigned pos = run->stop();
    LineLayoutItem r = run->m_lineLayoutItem;
    if (!r.isText() || r.isBR())
        return false;
    LineLayoutText layoutText(r);
    unsigned length = layoutText.textLength();
    if (pos >= length)
        return true;

    if (layoutText.is8Bit())
        return endsWithASCIISpaces(layoutText.characters8(), pos, length);
    return endsWithASCIISpaces(layoutText.characters16(), pos, length);
}

RootInlineBox* LayoutBlockFlow::constructLine(BidiRunList<BidiRun>& bidiRuns, const LineInfo& lineInfo)
{
    ASSERT(bidiRuns.firstRun());

    bool rootHasSelectedChildren = false;
    InlineFlowBox* parentBox = nullptr;
    int runCount = bidiRuns.runCount() - lineInfo.runsFromLeadingWhitespace();
    for (BidiRun* r = bidiRuns.firstRun(); r; r = r->next()) {
        // Create a box for our object.
        bool isOnlyRun = (runCount == 1);
        if (runCount == 2 && !r->m_lineLayoutItem.isListMarker())
            isOnlyRun = (!style()->isLeftToRightDirection() ? bidiRuns.lastRun() : bidiRuns.firstRun())->m_lineLayoutItem.isListMarker();

        if (lineInfo.isEmpty())
            continue;

        InlineBox* box;
        if (r->m_lineLayoutItem.isText())
            box = createInlineBoxForText(*r, isOnlyRun);
        else
            box = createInlineBoxForLayoutObject(r->m_lineLayoutItem, false, isOnlyRun);
        r->m_box = box;

        ASSERT(box);
        if (!box)
            continue;

        if (!rootHasSelectedChildren && box->getLineLayoutItem().getSelectionState() != SelectionNone)
            rootHasSelectedChildren = true;

        // If we have no parent box yet, or if the run is not simply a sibling,
        // then we need to construct inline boxes as necessary to properly enclose the
        // run's inline box. Segments can only be siblings at the root level, as
        // they are positioned separately.
        if (!parentBox || (parentBox->getLineLayoutItem() != r->m_lineLayoutItem.parent())) {
            // Create new inline boxes all the way back to the appropriate insertion point.
            parentBox = createLineBoxes(r->m_lineLayoutItem.parent(), lineInfo, box);
        } else {
            // Append the inline box to this line.
            parentBox->addToLine(box);
        }

        box->setBidiLevel(r->level());

        if (box->isInlineTextBox()) {
            if (AXObjectCache* cache = document().existingAXObjectCache())
                cache->inlineTextBoxesUpdated(r->m_lineLayoutItem);
        }
    }

    // We should have a root inline box.  It should be unconstructed and
    // be the last continuation of our line list.
    ASSERT(lastLineBox() && !lastLineBox()->isConstructed());

    // Set the m_selectedChildren flag on the root inline box if one of the leaf inline box
    // from the bidi runs walk above has a selection state.
    if (rootHasSelectedChildren)
        lastLineBox()->root().setHasSelectedChildren(true);

    // Set bits on our inline flow boxes that indicate which sides should
    // paint borders/margins/padding.  This knowledge will ultimately be used when
    // we determine the horizontal positions and widths of all the inline boxes on
    // the line.
    bool isLogicallyLastRunWrapped = bidiRuns.logicallyLastRun()->m_lineLayoutItem && bidiRuns.logicallyLastRun()->m_lineLayoutItem.isText() ? !reachedEndOfTextRun(bidiRuns) : true;
    lastLineBox()->determineSpacingForFlowBoxes(lineInfo.isLastLine(), isLogicallyLastRunWrapped, bidiRuns.logicallyLastRun()->m_lineLayoutItem);

    // Now mark the line boxes as being constructed.
    lastLineBox()->setConstructed();

    // Return the last line.
    return lastRootBox();
}

ETextAlign LayoutBlockFlow::textAlignmentForLine(bool endsWithSoftBreak) const
{
    ETextAlign alignment = style()->textAlign();
    if (endsWithSoftBreak)
        return alignment;

    TextAlignLast alignmentLast = style()->getTextAlignLast();
    switch (alignmentLast) {
    case TextAlignLastStart:
        return TASTART;
    case TextAlignLastEnd:
        return TAEND;
    case TextAlignLastLeft:
        return LEFT;
    case TextAlignLastRight:
        return RIGHT;
    case TextAlignLastCenter:
        return CENTER;
    case TextAlignLastJustify:
        return JUSTIFY;
    case TextAlignLastAuto:
        if (alignment == JUSTIFY)
            return TASTART;
        return alignment;
    }

    return alignment;
}

static void updateLogicalWidthForLeftAlignedBlock(bool isLeftToRightDirection, BidiRun* trailingSpaceRun, LayoutUnit& logicalLeft, LayoutUnit totalLogicalWidth, LayoutUnit availableLogicalWidth)
{
    // The direction of the block should determine what happens with wide lines.
    // In particular with RTL blocks, wide lines should still spill out to the left.
    if (isLeftToRightDirection) {
        if (totalLogicalWidth > availableLogicalWidth && trailingSpaceRun)
            trailingSpaceRun->m_box->setLogicalWidth(std::max(LayoutUnit(), trailingSpaceRun->m_box->logicalWidth() - totalLogicalWidth + availableLogicalWidth));
        return;
    }

    if (trailingSpaceRun)
        trailingSpaceRun->m_box->setLogicalWidth(LayoutUnit());
    else if (totalLogicalWidth > availableLogicalWidth)
        logicalLeft -= (totalLogicalWidth - availableLogicalWidth);
}

static void updateLogicalWidthForRightAlignedBlock(bool isLeftToRightDirection, BidiRun* trailingSpaceRun, LayoutUnit& logicalLeft, LayoutUnit& totalLogicalWidth, LayoutUnit availableLogicalWidth)
{
    // Wide lines spill out of the block based off direction.
    // So even if text-align is right, if direction is LTR, wide lines should overflow out of the right
    // side of the block.
    if (isLeftToRightDirection) {
        if (trailingSpaceRun) {
            totalLogicalWidth -= trailingSpaceRun->m_box->logicalWidth();
            trailingSpaceRun->m_box->setLogicalWidth(LayoutUnit());
        }
        if (totalLogicalWidth < availableLogicalWidth)
            logicalLeft += availableLogicalWidth - totalLogicalWidth;
        return;
    }

    if (totalLogicalWidth > availableLogicalWidth && trailingSpaceRun) {
        trailingSpaceRun->m_box->setLogicalWidth(std::max(LayoutUnit(), trailingSpaceRun->m_box->logicalWidth() - totalLogicalWidth + availableLogicalWidth));
        totalLogicalWidth -= trailingSpaceRun->m_box->logicalWidth();
    } else {
        logicalLeft += availableLogicalWidth - totalLogicalWidth;
    }
}

static void updateLogicalWidthForCenterAlignedBlock(bool isLeftToRightDirection, BidiRun* trailingSpaceRun, LayoutUnit& logicalLeft, LayoutUnit& totalLogicalWidth, LayoutUnit availableLogicalWidth)
{
    LayoutUnit trailingSpaceWidth;
    if (trailingSpaceRun) {
        totalLogicalWidth -= trailingSpaceRun->m_box->logicalWidth();
        trailingSpaceWidth = std::min(trailingSpaceRun->m_box->logicalWidth(), (availableLogicalWidth - totalLogicalWidth + 1) / 2);
        trailingSpaceRun->m_box->setLogicalWidth(std::max(LayoutUnit(), trailingSpaceWidth));
    }
    if (isLeftToRightDirection)
        logicalLeft += std::max((availableLogicalWidth - totalLogicalWidth) / 2, LayoutUnit());
    else
        logicalLeft += totalLogicalWidth > availableLogicalWidth ? (availableLogicalWidth - totalLogicalWidth) : (availableLogicalWidth - totalLogicalWidth) / 2 - trailingSpaceWidth;
}

void LayoutBlockFlow::setMarginsForRubyRun(BidiRun* run, LayoutRubyRun* layoutRubyRun, LayoutObject* previousObject, const LineInfo& lineInfo)
{
    int startOverhang;
    int endOverhang;
    LayoutObject* nextObject = nullptr;
    for (BidiRun* runWithNextObject = run->next(); runWithNextObject; runWithNextObject = runWithNextObject->next()) {
        if (!runWithNextObject->m_lineLayoutItem.isOutOfFlowPositioned() && !runWithNextObject->m_box->isLineBreak()) {
            nextObject = runWithNextObject->m_lineLayoutItem.layoutObject();
            break;
        }
    }
    layoutRubyRun->getOverhang(lineInfo.isFirstLine(), layoutRubyRun->style()->isLeftToRightDirection() ? previousObject : nextObject, layoutRubyRun->style()->isLeftToRightDirection() ? nextObject : previousObject, startOverhang, endOverhang);
    setMarginStartForChild(*layoutRubyRun, LayoutUnit(-startOverhang));
    setMarginEndForChild(*layoutRubyRun, LayoutUnit(-endOverhang));
}

static inline void setLogicalWidthForTextRun(RootInlineBox* lineBox, BidiRun* run, LineLayoutText layoutText, LayoutUnit xPos, const LineInfo& lineInfo,
    GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache, WordMeasurements& wordMeasurements)
{
    HashSet<const SimpleFontData*> fallbackFonts;
    GlyphOverflow glyphOverflow;

    const Font& font = layoutText.style(lineInfo.isFirstLine())->font();

    LayoutUnit hyphenWidth;
    if (toInlineTextBox(run->m_box)->hasHyphen())
        hyphenWidth = LayoutUnit(layoutText.hyphenWidth(font, run->direction()));

    float measuredWidth = 0;
    FloatRect glyphBounds;

    bool kerningIsEnabled = font.getFontDescription().getTypesettingFeatures() & Kerning;

#if OS(MACOSX)
    // FIXME: Having any font feature settings enabled can lead to selection gaps on
    // Chromium-mac. https://bugs.webkit.org/show_bug.cgi?id=113418
    bool canUseCachedWordMeasurements = font.canShapeWordByWord() && !font.getFontDescription().featureSettings();
#else
    bool canUseCachedWordMeasurements = font.canShapeWordByWord();
#endif

    if (canUseCachedWordMeasurements) {
        int lastEndOffset = run->m_start;
        for (size_t i = 0, size = wordMeasurements.size(); i < size && lastEndOffset < run->m_stop; ++i) {
            const WordMeasurement& wordMeasurement = wordMeasurements[i];
            if (wordMeasurement.startOffset == wordMeasurement.endOffset)
                continue;
            if (wordMeasurement.layoutText != layoutText || wordMeasurement.startOffset != lastEndOffset || wordMeasurement.endOffset > run->m_stop)
                continue;

            lastEndOffset = wordMeasurement.endOffset;
            if (kerningIsEnabled && lastEndOffset == run->m_stop) {
                int wordLength = lastEndOffset - wordMeasurement.startOffset;
                measuredWidth += layoutText.width(wordMeasurement.startOffset, wordLength, xPos, run->direction(), lineInfo.isFirstLine());
                if (i > 0 && wordLength == 1 && layoutText.characterAt(wordMeasurement.startOffset) == ' ')
                    measuredWidth += layoutText.style()->wordSpacing();
            } else {
                FloatRect wordGlyphBounds = wordMeasurement.glyphBounds;
                wordGlyphBounds.move(measuredWidth, 0);
                glyphBounds.unite(wordGlyphBounds);
                measuredWidth += wordMeasurement.width;
            }
            if (!wordMeasurement.fallbackFonts.isEmpty()) {
                HashSet<const SimpleFontData*>::const_iterator end = wordMeasurement.fallbackFonts.end();
                for (HashSet<const SimpleFontData*>::const_iterator it = wordMeasurement.fallbackFonts.begin(); it != end; ++it)
                    fallbackFonts.add(*it);
            }
        }
        if (lastEndOffset != run->m_stop) {
            // If we don't have enough cached data, we'll measure the run again.
            canUseCachedWordMeasurements = false;
            fallbackFonts.clear();
        }
    }

    // Don't put this into 'else' part of the above 'if' because canUseCachedWordMeasurements may be modified in the 'if' block.
    if (!canUseCachedWordMeasurements)
        measuredWidth = layoutText.width(run->m_start, run->m_stop - run->m_start, xPos, run->direction(), lineInfo.isFirstLine(), &fallbackFonts, &glyphBounds);

    // Negative word-spacing and/or letter-spacing may cause some glyphs to overflow the left boundary and result
    // negative measured width. Reset measured width to 0 and adjust glyph bounds accordingly to cover the overflow.
    if (measuredWidth < 0) {
        if (measuredWidth < glyphBounds.x()) {
            glyphBounds.expand(glyphBounds.x() - measuredWidth, 0);
            glyphBounds.setX(measuredWidth);
        }
        measuredWidth = 0;
    }

    glyphOverflow.setFromBounds(glyphBounds, font.getFontMetrics().floatAscent(), font.getFontMetrics().floatDescent(), measuredWidth);

    run->m_box->setLogicalWidth(LayoutUnit(measuredWidth) + hyphenWidth);
    if (!fallbackFonts.isEmpty()) {
        ASSERT(run->m_box->isText());
        GlyphOverflowAndFallbackFontsMap::ValueType* it = textBoxDataMap.add(toInlineTextBox(run->m_box), std::make_pair(Vector<const SimpleFontData*>(), GlyphOverflow())).storedValue;
        ASSERT(it->value.first.isEmpty());
        copyToVector(fallbackFonts, it->value.first);
        run->m_box->parent()->clearDescendantsHaveSameLineHeightAndBaseline();
    }
    if (!glyphOverflow.isApproximatelyZero()) {
        ASSERT(run->m_box->isText());
        GlyphOverflowAndFallbackFontsMap::ValueType* it = textBoxDataMap.add(toInlineTextBox(run->m_box), std::make_pair(Vector<const SimpleFontData*>(), GlyphOverflow())).storedValue;
        it->value.second = glyphOverflow;
        run->m_box->clearKnownToHaveNoOverflow();
    }
}

void LayoutBlockFlow::updateLogicalWidthForAlignment(const ETextAlign& textAlign, const RootInlineBox* rootInlineBox, BidiRun* trailingSpaceRun, LayoutUnit& logicalLeft, LayoutUnit& totalLogicalWidth, LayoutUnit& availableLogicalWidth, unsigned expansionOpportunityCount)
{
    TextDirection direction;
    if (rootInlineBox && rootInlineBox->getLineLayoutItem().style()->unicodeBidi() == Plaintext)
        direction = rootInlineBox->direction();
    else
        direction = style()->direction();

    // Armed with the total width of the line (without justification),
    // we now examine our text-align property in order to determine where to position the
    // objects horizontally. The total width of the line can be increased if we end up
    // justifying text.
    switch (textAlign) {
    case LEFT:
    case WEBKIT_LEFT:
        updateLogicalWidthForLeftAlignedBlock(style()->isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case RIGHT:
    case WEBKIT_RIGHT:
        updateLogicalWidthForRightAlignedBlock(style()->isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case CENTER:
    case WEBKIT_CENTER:
        updateLogicalWidthForCenterAlignedBlock(style()->isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case JUSTIFY:
        adjustInlineDirectionLineBounds(expansionOpportunityCount, logicalLeft, availableLogicalWidth);
        if (expansionOpportunityCount) {
            if (trailingSpaceRun) {
                totalLogicalWidth -= trailingSpaceRun->m_box->logicalWidth();
                trailingSpaceRun->m_box->setLogicalWidth(LayoutUnit());
            }
            break;
        }
        // Fall through
    case TASTART:
        if (direction == LTR)
            updateLogicalWidthForLeftAlignedBlock(style()->isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        else
            updateLogicalWidthForRightAlignedBlock(style()->isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case TAEND:
        if (direction == LTR)
            updateLogicalWidthForRightAlignedBlock(style()->isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        else
            updateLogicalWidthForLeftAlignedBlock(style()->isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    }
    if (shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        logicalLeft += verticalScrollbarWidth();
}

static void updateLogicalInlinePositions(LayoutBlockFlow* block, LayoutUnit& lineLogicalLeft, LayoutUnit& lineLogicalRight, LayoutUnit& availableLogicalWidth, bool firstLine, IndentTextOrNot indentText, LayoutUnit boxLogicalHeight)
{
    LayoutUnit lineLogicalHeight = block->minLineHeightForReplacedObject(firstLine, boxLogicalHeight);
    lineLogicalLeft = block->logicalLeftOffsetForLine(block->logicalHeight(), indentText, lineLogicalHeight);
    lineLogicalRight = block->logicalRightOffsetForLine(block->logicalHeight(), indentText, lineLogicalHeight);
    availableLogicalWidth = lineLogicalRight - lineLogicalLeft;
}

void LayoutBlockFlow::computeInlineDirectionPositionsForLine(RootInlineBox* lineBox, const LineInfo& lineInfo, BidiRun* firstRun, BidiRun* trailingSpaceRun, bool reachedEnd,
    GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache, WordMeasurements& wordMeasurements)
{
    ETextAlign textAlign = textAlignmentForLine(!reachedEnd && !lineBox->endsWithBreak());

    // CSS 2.1: "'Text-indent' only affects a line if it is the first formatted line of an element. For example, the first line of an anonymous block
    // box is only affected if it is the first child of its parent element."
    // CSS3 "text-indent", "each-line" affects the first line of the block container as well as each line after a forced line break,
    // but does not affect lines after a soft wrap break.
    bool isFirstLine = lineInfo.isFirstLine() && !(isAnonymousBlock() && parent()->slowFirstChild() != this);
    bool isAfterHardLineBreak = lineBox->prevRootBox() && lineBox->prevRootBox()->endsWithBreak();
    IndentTextOrNot indentText = requiresIndent(isFirstLine, isAfterHardLineBreak, styleRef());
    LayoutUnit lineLogicalLeft;
    LayoutUnit lineLogicalRight;
    LayoutUnit availableLogicalWidth;
    updateLogicalInlinePositions(this, lineLogicalLeft, lineLogicalRight, availableLogicalWidth, isFirstLine, indentText, LayoutUnit());
    bool needsWordSpacing;

    if (firstRun && firstRun->m_lineLayoutItem.isAtomicInlineLevel()) {
        LineLayoutBox layoutBox(firstRun->m_lineLayoutItem);
        updateLogicalInlinePositions(this, lineLogicalLeft, lineLogicalRight, availableLogicalWidth, isFirstLine, indentText, layoutBox.logicalHeight());
    }

    computeInlineDirectionPositionsForSegment(lineBox, lineInfo, textAlign, lineLogicalLeft, availableLogicalWidth, firstRun, trailingSpaceRun, textBoxDataMap, verticalPositionCache, wordMeasurements);
    // The widths of all runs are now known. We can now place every inline box (and
    // compute accurate widths for the inline flow boxes).
    needsWordSpacing = lineBox->isLeftToRightDirection() ? false: true;
    lineBox->placeBoxesInInlineDirection(lineLogicalLeft, needsWordSpacing);
}

BidiRun* LayoutBlockFlow::computeInlineDirectionPositionsForSegment(RootInlineBox* lineBox, const LineInfo& lineInfo, ETextAlign textAlign, LayoutUnit& logicalLeft,
    LayoutUnit& availableLogicalWidth, BidiRun* firstRun, BidiRun* trailingSpaceRun, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache,
    WordMeasurements& wordMeasurements)
{
    bool needsWordSpacing = true;
    LayoutUnit totalLogicalWidth = lineBox->getFlowSpacingLogicalWidth();
    bool isAfterExpansion = true;
    ExpansionOpportunities expansions;
    LayoutObject* previousObject = nullptr;
    TextJustify textJustify = style()->getTextJustify();

    BidiRun* r = firstRun;
    for (; r; r = r->next()) {
        if (!r->m_box || r->m_lineLayoutItem.isOutOfFlowPositioned() || r->m_box->isLineBreak()) {
            continue; // Positioned objects are only participating to figure out their
                // correct static x position.  They have no effect on the width.
                // Similarly, line break boxes have no effect on the width.
        }
        if (r->m_lineLayoutItem.isText()) {
            LineLayoutText rt(r->m_lineLayoutItem);
            if (textAlign == JUSTIFY && r != trailingSpaceRun && textJustify != TextJustifyNone) {
                if (!isAfterExpansion)
                    toInlineTextBox(r->m_box)->setCanHaveLeadingExpansion(true);
                expansions.addRunWithExpansions(*r, isAfterExpansion, textJustify);
            }

            if (rt.textLength()) {
                if (!r->m_start && needsWordSpacing && isSpaceOrNewline(rt.characterAt(r->m_start)))
                    totalLogicalWidth += rt.style(lineInfo.isFirstLine())->font().getFontDescription().wordSpacing();
                needsWordSpacing = !isSpaceOrNewline(rt.characterAt(r->m_stop - 1));
            }

            setLogicalWidthForTextRun(lineBox, r, rt, totalLogicalWidth, lineInfo, textBoxDataMap, verticalPositionCache, wordMeasurements);
        } else {
            isAfterExpansion = false;
            if (!r->m_lineLayoutItem.isLayoutInline()) {
                LayoutBox* layoutBox = toLayoutBox(r->m_lineLayoutItem.layoutObject());
                if (layoutBox->isRubyRun())
                    setMarginsForRubyRun(r, toLayoutRubyRun(layoutBox), previousObject, lineInfo);
                r->m_box->setLogicalWidth(logicalWidthForChild(*layoutBox));
                totalLogicalWidth += marginStartForChild(*layoutBox) + marginEndForChild(*layoutBox);
                needsWordSpacing = true;
            }
        }

        totalLogicalWidth += r->m_box->logicalWidth();
        previousObject = r->m_lineLayoutItem.layoutObject();
    }

    if (isAfterExpansion)
        expansions.removeTrailingExpansion();

    updateLogicalWidthForAlignment(textAlign, lineBox, trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth, expansions.count());

    expansions.computeExpansionsForJustifiedText(firstRun, trailingSpaceRun, totalLogicalWidth, availableLogicalWidth);

    return r;
}

void LayoutBlockFlow::computeBlockDirectionPositionsForLine(RootInlineBox* lineBox, BidiRun* firstRun, GlyphOverflowAndFallbackFontsMap& textBoxDataMap,
    VerticalPositionCache& verticalPositionCache)
{
    setLogicalHeight(lineBox->alignBoxesInBlockDirection(logicalHeight(), textBoxDataMap, verticalPositionCache));

    // Now make sure we place replaced layout objects correctly.
    for (BidiRun* r = firstRun; r; r = r->next()) {
        ASSERT(r->m_box);
        if (!r->m_box)
            continue; // Skip runs with no line boxes.

        // Align positioned boxes with the top of the line box.  This is
        // a reasonable approximation of an appropriate y position.
        if (r->m_lineLayoutItem.isOutOfFlowPositioned())
            r->m_box->setLogicalTop(logicalHeight());

        // Position is used to properly position both replaced elements and
        // to update the static normal flow x/y of positioned elements.
        if (r->m_lineLayoutItem.isText())
            toLayoutText(r->m_lineLayoutItem.layoutObject())->positionLineBox(r->m_box);
        else if (r->m_lineLayoutItem.isBox())
            toLayoutBox(r->m_lineLayoutItem.layoutObject())->positionLineBox(r->m_box);
    }
}

void LayoutBlockFlow::appendFloatingObjectToLastLine(FloatingObject& floatingObject)
{
    ASSERT(!floatingObject.originatingLine());
    floatingObject.setOriginatingLine(lastRootBox());
    lastRootBox()->appendFloat(floatingObject.layoutObject());
}

// This function constructs line boxes for all of the text runs in the resolver and computes their position.
RootInlineBox* LayoutBlockFlow::createLineBoxesFromBidiRuns(unsigned bidiLevel, BidiRunList<BidiRun>& bidiRuns, const InlineIterator& end, LineInfo& lineInfo, VerticalPositionCache& verticalPositionCache, BidiRun* trailingSpaceRun, WordMeasurements& wordMeasurements)
{
    if (!bidiRuns.runCount())
        return nullptr;

    // FIXME: Why is this only done when we had runs?
    lineInfo.setLastLine(!end.getLineLayoutItem());

    RootInlineBox* lineBox = constructLine(bidiRuns, lineInfo);
    if (!lineBox)
        return nullptr;

    lineBox->setBidiLevel(bidiLevel);
    lineBox->setEndsWithBreak(lineInfo.previousLineBrokeCleanly());

    bool isSVGRootInlineBox = lineBox->isSVGRootInlineBox();

    GlyphOverflowAndFallbackFontsMap textBoxDataMap;

    // Now we position all of our text runs horizontally.
    if (!isSVGRootInlineBox)
        computeInlineDirectionPositionsForLine(lineBox, lineInfo, bidiRuns.firstRun(), trailingSpaceRun, end.atEnd(), textBoxDataMap, verticalPositionCache, wordMeasurements);

    // Now position our text runs vertically.
    computeBlockDirectionPositionsForLine(lineBox, bidiRuns.firstRun(), textBoxDataMap, verticalPositionCache);

    // SVG text layout code computes vertical & horizontal positions on its own.
    // Note that we still need to execute computeVerticalPositionsForLine() as
    // it calls InlineTextBox::positionLineBox(), which tracks whether the box
    // contains reversed text or not. If we wouldn't do that editing and thus
    // text selection in RTL boxes would not work as expected.
    if (isSVGRootInlineBox) {
        ASSERT(isSVGText());
        toSVGRootInlineBox(lineBox)->computePerCharacterLayoutInformation();
    }

    // Compute our overflow now.
    lineBox->computeOverflow(lineBox->lineTop(), lineBox->lineBottom(), textBoxDataMap);

    return lineBox;
}

static void deleteLineRange(LineLayoutState& layoutState, RootInlineBox* startLine, RootInlineBox* stopLine = 0)
{
    RootInlineBox* boxToDelete = startLine;
    while (boxToDelete && boxToDelete != stopLine) {
        // Note: deleteLineRange(firstRootBox()) is not identical to deleteLineBoxTree().
        // deleteLineBoxTree uses nextLineBox() instead of nextRootBox() when traversing.
        RootInlineBox* next = boxToDelete->nextRootBox();
        boxToDelete->deleteLine();
        boxToDelete = next;
    }
}

void LayoutBlockFlow::layoutRunsAndFloats(LineLayoutState& layoutState)
{
    // We want to skip ahead to the first dirty line
    InlineBidiResolver resolver;
    RootInlineBox* startLine = determineStartPosition(layoutState, resolver);

    if (containsFloats())
        layoutState.setLastFloat(m_floatingObjects->set().last().get());

    // We also find the first clean line and extract these lines.  We will add them back
    // if we determine that we're able to synchronize after handling all our dirty lines.
    InlineIterator cleanLineStart;
    BidiStatus cleanLineBidiStatus;
    if (!layoutState.isFullLayout() && startLine)
        determineEndPosition(layoutState, startLine, cleanLineStart, cleanLineBidiStatus);

    if (startLine)
        deleteLineRange(layoutState, startLine);

    layoutRunsAndFloatsInRange(layoutState, resolver, cleanLineStart, cleanLineBidiStatus);
    linkToEndLineIfNeeded(layoutState);
    markDirtyFloatsForPaintInvalidation(layoutState.floats());
}

// Before restarting the layout loop with a new logicalHeight, remove all floats that were added and reset the resolver.
inline const InlineIterator& LayoutBlockFlow::restartLayoutRunsAndFloatsInRange(LayoutUnit oldLogicalHeight, LayoutUnit newLogicalHeight,  FloatingObject* lastFloatFromPreviousLine, InlineBidiResolver& resolver,  const InlineIterator& oldEnd)
{
    removeFloatingObjectsBelow(lastFloatFromPreviousLine, oldLogicalHeight);
    setLogicalHeight(newLogicalHeight);
    resolver.setPositionIgnoringNestedIsolates(oldEnd);
    return oldEnd;
}

void LayoutBlockFlow::appendFloatsToLastLine(LineLayoutState& layoutState, const InlineIterator& cleanLineStart, const InlineBidiResolver& resolver, const BidiStatus& cleanLineBidiStatus)
{
    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    FloatingObjectSetIterator it = floatingObjectSet.begin();
    FloatingObjectSetIterator end = floatingObjectSet.end();
    if (layoutState.lastFloat()) {
        FloatingObjectSetIterator lastFloatIterator = floatingObjectSet.find(layoutState.lastFloat());
        ASSERT(lastFloatIterator != end);
        ++lastFloatIterator;
        it = lastFloatIterator;
    }
    for (; it != end; ++it) {
        FloatingObject& floatingObject = *it->get();
        // If we've reached the start of clean lines any remaining floating children belong to them.
        if (cleanLineStart.getLineLayoutItem().isEqual(floatingObject.layoutObject()) && layoutState.endLine()) {
            layoutState.setEndLineMatched(layoutState.endLineMatched() || matchedEndLine(layoutState, resolver, cleanLineStart, cleanLineBidiStatus));
            if (layoutState.endLineMatched()) {
                layoutState.setLastFloat(&floatingObject);
                return;
            }
        }
        appendFloatingObjectToLastLine(floatingObject);
        ASSERT(floatingObject.layoutObject() == layoutState.floats()[layoutState.floatIndex()].object);
        // If a float's geometry has changed, give up on syncing with clean lines.
        if (layoutState.floats()[layoutState.floatIndex()].rect != floatingObject.frameRect()) {
            // Delete all the remaining lines.
            deleteLineRange(layoutState, layoutState.endLine());
            layoutState.setEndLine(nullptr);
        }
        layoutState.setFloatIndex(layoutState.floatIndex() + 1);
    }
    layoutState.setLastFloat(!floatingObjectSet.isEmpty() ? floatingObjectSet.last().get() : 0);
}

void LayoutBlockFlow::layoutRunsAndFloatsInRange(LineLayoutState& layoutState,
    InlineBidiResolver& resolver, const InlineIterator& cleanLineStart,
    const BidiStatus& cleanLineBidiStatus)
{
    const ComputedStyle& styleToUse = styleRef();
    bool paginated = view()->layoutState() && view()->layoutState()->isPaginated();
    LineMidpointState& lineMidpointState = resolver.midpointState();
    InlineIterator endOfLine = resolver.position();
    LayoutTextInfo layoutTextInfo;
    VerticalPositionCache verticalPositionCache;

    // Pagination may require us to delete and re-create a line due to floats. When this happens,
    // we need to store the pagination strut in the meantime.
    LayoutUnit paginationStrutFromDeletedLine;

    LineBreaker lineBreaker(LineLayoutBlockFlow(this));

    while (!endOfLine.atEnd()) {
        // The runs from the previous line should have been cleaned up.
        ASSERT(!resolver.runs().runCount());

        // FIXME: Is this check necessary before the first iteration or can it be moved to the end?
        if (layoutState.endLine()) {
            layoutState.setEndLineMatched(layoutState.endLineMatched() || matchedEndLine(layoutState, resolver, cleanLineStart, cleanLineBidiStatus));
            if (layoutState.endLineMatched()) {
                resolver.setPosition(InlineIterator(resolver.position().root(), 0, 0), 0);
                break;
            }
        }

        lineMidpointState.reset();

        layoutState.lineInfo().setEmpty(true);
        layoutState.lineInfo().resetRunsFromLeadingWhitespace();

        const InlineIterator previousEndofLine = endOfLine;
        bool isNewUBAParagraph = layoutState.lineInfo().previousLineBrokeCleanly();
        FloatingObject* lastFloatFromPreviousLine = (containsFloats()) ? m_floatingObjects->set().last().get() : 0;

        WordMeasurements wordMeasurements;
        endOfLine = lineBreaker.nextLineBreak(resolver, layoutState.lineInfo(), layoutTextInfo, wordMeasurements);
        layoutTextInfo.m_lineBreakIterator.resetPriorContext();
        if (resolver.position().atEnd()) {
            // FIXME: We shouldn't be creating any runs in nextLineBreak to begin with!
            // Once BidiRunList is separated from BidiResolver this will not be needed.
            resolver.runs().deleteRuns();
            resolver.markCurrentRunEmpty(); // FIXME: This can probably be replaced by an ASSERT (or just removed).
            resolver.setPosition(InlineIterator(resolver.position().root(), 0, 0), 0);
            break;
        }

        ASSERT(endOfLine != resolver.position());

        // This is a short-cut for empty lines.
        if (layoutState.lineInfo().isEmpty()) {
            ASSERT(!paginationStrutFromDeletedLine);
            if (lastRootBox())
                lastRootBox()->setLineBreakInfo(endOfLine.getLineLayoutItem(), endOfLine.offset(), resolver.status());
            resolver.runs().deleteRuns();
        } else {
            VisualDirectionOverride override = (styleToUse.rtlOrdering() == VisualOrder ? (styleToUse.direction() == LTR ? VisualLeftToRightOverride : VisualRightToLeftOverride) : NoVisualOverride);
            if (isNewUBAParagraph && styleToUse.unicodeBidi() == Plaintext && !resolver.context()->parent()) {
                TextDirection direction = determinePlaintextDirectionality(resolver.position().root(), resolver.position().getLineLayoutItem(), resolver.position().offset());
                resolver.setStatus(BidiStatus(direction, isOverride(styleToUse.unicodeBidi())));
            }
            // FIXME: This ownership is reversed. We should own the BidiRunList and pass it to createBidiRunsForLine.
            BidiRunList<BidiRun>& bidiRuns = resolver.runs();
            constructBidiRunsForLine(resolver, bidiRuns, endOfLine, override, layoutState.lineInfo().previousLineBrokeCleanly(), isNewUBAParagraph);
            ASSERT(resolver.position() == endOfLine);

            BidiRun* trailingSpaceRun = resolver.trailingSpaceRun();

            if (bidiRuns.runCount() && lineBreaker.lineWasHyphenated())
                bidiRuns.logicallyLastRun()->m_hasHyphen = true;

            // Now that the runs have been ordered, we create the line boxes.
            // At the same time we figure out where border/padding/margin should be applied for
            // inline flow boxes.

            LayoutUnit oldLogicalHeight = logicalHeight();
            RootInlineBox* lineBox = createLineBoxesFromBidiRuns(resolver.status().context->level(), bidiRuns, endOfLine, layoutState.lineInfo(), verticalPositionCache, trailingSpaceRun, wordMeasurements);

            bidiRuns.deleteRuns();
            resolver.markCurrentRunEmpty(); // FIXME: This can probably be replaced by an ASSERT (or just removed).

            // If we decided to re-create the line due to pagination, we better have a new line now.
            ASSERT(lineBox || !paginationStrutFromDeletedLine);

            if (lineBox) {
                lineBox->setLineBreakInfo(endOfLine.getLineLayoutItem(), endOfLine.offset(), resolver.status());
                if (paginated) {
                    if (paginationStrutFromDeletedLine) {
                        // This is a line that got re-created because it got pushed to the next fragmentainer, and there
                        // were floats in the vicinity that affected the available width. Restore the pagination info
                        // for this line.
                        lineBox->setIsFirstAfterPageBreak(true);
                        lineBox->setPaginationStrut(paginationStrutFromDeletedLine);
                        paginationStrutFromDeletedLine = LayoutUnit();
                    } else {
                        LayoutUnit adjustment;
                        adjustLinePositionForPagination(*lineBox, adjustment);
                        if (adjustment) {
                            LayoutUnit oldLineWidth = availableLogicalWidthForLine(oldLogicalHeight, layoutState.lineInfo().isFirstLine() ? IndentText : DoNotIndentText);
                            lineBox->moveInBlockDirection(adjustment);
                            if (availableLogicalWidthForLine(oldLogicalHeight + adjustment, layoutState.lineInfo().isFirstLine() ? IndentText: DoNotIndentText) != oldLineWidth) {
                                // We have to delete this line, remove all floats that got added, and let line layout
                                // re-run. We had just calculated the pagination strut for this line, and we need to
                                // stow it away, so that we can re-apply it when the new line has been created.
                                paginationStrutFromDeletedLine = lineBox->paginationStrut();
                                ASSERT(paginationStrutFromDeletedLine);
                                // We're also going to assume that we're right after a page break when re-creating this
                                // line, so it better be so.
                                ASSERT(lineBox->isFirstAfterPageBreak());
                                lineBox->deleteLine();
                                endOfLine = restartLayoutRunsAndFloatsInRange(oldLogicalHeight, oldLogicalHeight + adjustment, lastFloatFromPreviousLine, resolver, previousEndofLine);
                            } else {
                                setLogicalHeight(lineBox->lineBottomWithLeading());
                            }
                        }
                    }
                }
            }
        }

        if (!paginationStrutFromDeletedLine) {
            for (size_t i = 0; i < lineBreaker.positionedObjects().size(); ++i)
                setStaticPositions(LineLayoutBlockFlow(this), LineLayoutBox(lineBreaker.positionedObjects()[i]), DoNotIndentText);

            if (!layoutState.lineInfo().isEmpty())
                layoutState.lineInfo().setFirstLine(false);
            clearFloats(lineBreaker.clear());

            if (m_floatingObjects && lastRootBox()) {
                InlineBidiResolver endOfLineResolver;
                endOfLineResolver.setPosition(endOfLine, numberOfIsolateAncestors(endOfLine));
                endOfLineResolver.setStatus(resolver.status());
                appendFloatsToLastLine(layoutState, cleanLineStart, endOfLineResolver, cleanLineBidiStatus);
            }
        }

        lineMidpointState.reset();
        resolver.setPosition(endOfLine, numberOfIsolateAncestors(endOfLine));
    }

    // The resolver runs should have been cleared, otherwise they're leaking.
    ASSERT(!resolver.runs().runCount());

    // In case we already adjusted the line positions during this layout to avoid widows
    // then we need to ignore the possibility of having a new widows situation.
    // Otherwise, we risk leaving empty containers which is against the block fragmentation principles.
    if (paginated && style()->widows() > 1 && !didBreakAtLineToAvoidWidow()) {
        // Check the line boxes to make sure we didn't create unacceptable widows.
        // However, we'll prioritize orphans - so nothing we do here should create
        // a new orphan.

        RootInlineBox* lineBox = lastRootBox();

        // Count from the end of the block backwards, to see how many hanging
        // lines we have.
        RootInlineBox* firstLineInBlock = firstRootBox();
        int numLinesHanging = 1;
        while (lineBox && lineBox != firstLineInBlock && !lineBox->isFirstAfterPageBreak()) {
            ++numLinesHanging;
            lineBox = lineBox->prevRootBox();
        }

        // If there were no breaks in the block, we didn't create any widows.
        if (!lineBox || !lineBox->isFirstAfterPageBreak() || lineBox == firstLineInBlock)
            return;

        if (numLinesHanging < style()->widows()) {
            // We have detected a widow. Now we need to work out how many
            // lines there are on the previous page, and how many we need
            // to steal.
            int numLinesNeeded = style()->widows() - numLinesHanging;
            RootInlineBox* currentFirstLineOfNewPage = lineBox;

            // Count the number of lines in the previous page.
            lineBox = lineBox->prevRootBox();
            int numLinesInPreviousPage = 1;
            while (lineBox && lineBox != firstLineInBlock && !lineBox->isFirstAfterPageBreak()) {
                ++numLinesInPreviousPage;
                lineBox = lineBox->prevRootBox();
            }

            // If there was an explicit value for orphans, respect that. If not, we still
            // shouldn't create a situation where we make an orphan bigger than the initial value.
            // This means that setting widows implies we also care about orphans, but given
            // the specification says the initial orphan value is non-zero, this is ok. The
            // author is always free to set orphans explicitly as well.
            int orphans = style()->orphans();
            int numLinesAvailable = numLinesInPreviousPage - orphans;
            if (numLinesAvailable <= 0)
                return;

            int numLinesToTake = std::min(numLinesAvailable, numLinesNeeded);
            // Wind back from our first widowed line.
            lineBox = currentFirstLineOfNewPage;
            for (int i = 0; i < numLinesToTake; ++i)
                lineBox = lineBox->prevRootBox();

            // We now want to break at this line. Remember for next layout and trigger relayout.
            setBreakAtLineToAvoidWidow(lineCount(lineBox));
            markLinesDirtyInBlockRange(lastRootBox()->lineBottomWithLeading(), lineBox->lineBottomWithLeading(), lineBox);
        }
    }

    clearDidBreakAtLineToAvoidWidow();
}

void LayoutBlockFlow::linkToEndLineIfNeeded(LineLayoutState& layoutState)
{
    if (layoutState.endLine()) {
        if (layoutState.endLineMatched()) {
            bool paginated = view()->layoutState() && view()->layoutState()->isPaginated();
            // Attach all the remaining lines, and then adjust their y-positions as needed.
            LayoutUnit delta = logicalHeight() - layoutState.endLineLogicalTop();
            for (RootInlineBox* line = layoutState.endLine(); line; line = line->nextRootBox()) {
                line->attachLine();
                if (paginated) {
                    delta -= line->paginationStrut();
                    adjustLinePositionForPagination(*line, delta);
                }
                if (delta)
                    line->moveInBlockDirection(delta);
                if (Vector<LayoutBox*>* cleanLineFloats = line->floatsPtr()) {
                    for (auto* box : *cleanLineFloats) {
                        FloatingObject* floatingObject = insertFloatingObject(*box);
                        ASSERT(!floatingObject->originatingLine());
                        floatingObject->setOriginatingLine(line);
                        setLogicalHeight(logicalTopForChild(*box) - marginBeforeForChild(*box) + delta);
                        positionNewFloats();
                    }
                }
            }
            setLogicalHeight(lastRootBox()->lineBottomWithLeading());
        } else {
            // Delete all the remaining lines.
            deleteLineRange(layoutState, layoutState.endLine());
        }
    }

    // In case we have a float on the last line, it might not be positioned up to now.
    // This has to be done before adding in the bottom border/padding, or the float will
    // include the padding incorrectly. -dwh
    if (positionNewFloats() && lastRootBox())
        appendFloatsToLastLine(layoutState, InlineIterator(), InlineBidiResolver(), BidiStatus());
}

void LayoutBlockFlow::markDirtyFloatsForPaintInvalidation(Vector<FloatWithRect>& floats)
{
    size_t floatCount = floats.size();
    // Floats that did not have layout did not paint invalidations when we laid them out. They would have
    // painted by now if they had moved, but if they stayed at (0, 0), they still need to be
    // painted.
    for (size_t i = 0; i < floatCount; ++i) {
        LayoutBox* f = floats[i].object;
        if (!floats[i].everHadLayout) {
            if (!f->location().x() && !f->location().y())
                f->setShouldDoFullPaintInvalidation();
        }
        insertFloatingObject(*f);
    }
    positionNewFloats();
}

struct InlineMinMaxIterator {
/* InlineMinMaxIterator is a class that will iterate over all layout objects that contribute to
   inline min/max width calculations.  Note the following about the way it walks:
   (1) Positioned content is skipped (since it does not contribute to min/max width of a block)
   (2) We do not drill into the children of floats or replaced elements, since you can't break
       in the middle of such an element.
   (3) Inline flows (e.g., <a>, <span>, <i>) are walked twice, since each side can have
       distinct borders/margin/padding that contribute to the min/max width.
*/
    LayoutObject* parent;
    LayoutObject* current;
    bool endOfInline;

    InlineMinMaxIterator(LayoutObject* p, bool end = false)
        : parent(p), current(p), endOfInline(end)
    {

    }

    LayoutObject* next();
};

LayoutObject* InlineMinMaxIterator::next()
{
    LayoutObject* result = nullptr;
    bool oldEndOfInline = endOfInline;
    endOfInline = false;
    while (current || current == parent) {
        if (!oldEndOfInline && (current == parent || (!current->isFloating() && !current->isAtomicInlineLevel() && !current->isOutOfFlowPositioned())))
            result = current->slowFirstChild();

        if (!result) {
            // We hit the end of our inline. (It was empty, e.g., <span></span>.)
            if (!oldEndOfInline && current->isLayoutInline()) {
                result = current;
                endOfInline = true;
                break;
            }

            while (current && current != parent) {
                result = current->nextSibling();
                if (result)
                    break;
                current = current->parent();
                if (current && current != parent && current->isLayoutInline()) {
                    result = current;
                    endOfInline = true;
                    break;
                }
            }
        }

        if (!result)
            break;

        if (!result->isOutOfFlowPositioned() && (result->isText() || result->isFloating() || result->isAtomicInlineLevel() || result->isLayoutInline()))
            break;

        current = result;
        result = nullptr;
    }

    // Update our position.
    current = result;
    return current;
}

static LayoutUnit getBPMWidth(LayoutUnit childValue, Length cssUnit)
{
    if (cssUnit.type() != Auto)
        return (cssUnit.isFixed() ? static_cast<LayoutUnit>(cssUnit.value()) : childValue);
    return LayoutUnit();
}

static LayoutUnit getBorderPaddingMargin(const LayoutBoxModelObject& child, bool endOfInline)
{
    const ComputedStyle& childStyle = child.styleRef();
    if (endOfInline) {
        return getBPMWidth(child.marginEnd(), childStyle.marginEnd()) +
            getBPMWidth(child.paddingEnd(), childStyle.paddingEnd()) +
            child.borderEnd();
    }
    return getBPMWidth(child.marginStart(), childStyle.marginStart()) +
        getBPMWidth(child.paddingStart(), childStyle.paddingStart()) +
        child.borderStart();
}

static inline void stripTrailingSpace(LayoutUnit& inlineMax, LayoutUnit& inlineMin,
    LayoutObject* trailingSpaceChild)
{
    if (trailingSpaceChild && trailingSpaceChild->isText()) {
        // Collapse away the trailing space at the end of a block by finding
        // the first white-space character and subtracting its width. Subsequent
        // white-space characters have been collapsed into the first one (which
        // can be either a space or a tab character).
        LayoutText* text = toLayoutText(trailingSpaceChild);
        UChar trailingWhitespaceChar = ' ';
        for (unsigned i = text->textLength(); i > 0; i--) {
            UChar c = text->characterAt(i - 1);
            if (!Character::treatAsSpace(c))
                break;
            trailingWhitespaceChar = c;
        }

        // FIXME: This ignores first-line.
        const Font& font = text->style()->font();
        TextRun run = constructTextRun(font, &trailingWhitespaceChar, 1,
            text->styleRef(), text->style()->direction());
        float spaceWidth = font.width(run);
        inlineMax -= LayoutUnit::fromFloatCeil(spaceWidth + font.getFontDescription().wordSpacing());
        if (inlineMin > inlineMax)
            inlineMin = inlineMax;
    }
}

// When converting between floating point and LayoutUnits we risk losing precision
// with each conversion. When this occurs while accumulating our preferred widths,
// we can wind up with a line width that's larger than our maxPreferredWidth due to
// pure float accumulation.
static inline LayoutUnit adjustFloatForSubPixelLayout(float value)
{
    return LayoutUnit::fromFloatCeil(value);
}

static inline void adjustMinMaxForInlineFlow(LayoutObject* child,
    bool endOfInline, LayoutUnit& childMin, LayoutUnit& childMax)
{
    // Add in padding/border/margin from the appropriate side of
    // the element.
    LayoutUnit bpm = getBorderPaddingMargin(toLayoutInline(*child),
        endOfInline);
    childMin += bpm;
    childMax += bpm;
}

static inline void adjustMarginForInlineReplaced(LayoutObject* child,
    LayoutUnit& childMin, LayoutUnit& childMax)
{
    // Inline replaced elts add in their margins to their min/max values.
    const ComputedStyle& childStyle = child->styleRef();
    Length startMargin = childStyle.marginStart();
    Length endMargin = childStyle.marginEnd();
    LayoutUnit margins;
    if (startMargin.isFixed())
        margins += adjustFloatForSubPixelLayout(startMargin.value());
    if (endMargin.isFixed())
        margins += adjustFloatForSubPixelLayout(endMargin.value());
    childMin += margins;
    childMax += margins;
}

// FIXME: This function should be broken into something less monolithic.
// FIXME: The main loop here is very similar to LineBreaker::nextSegmentBreak. They can probably reuse code.
void LayoutBlockFlow::computeInlinePreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth)
{
    LayoutUnit inlineMax;
    LayoutUnit inlineMin;

    const ComputedStyle& styleToUse = styleRef();
    LayoutBlock* containingBlock = this->containingBlock();
    LayoutUnit cw = containingBlock ? containingBlock->contentLogicalWidth() : LayoutUnit();

    // If we are at the start of a line, we want to ignore all white-space.
    // Also strip spaces if we previously had text that ended in a trailing space.
    bool stripFrontSpaces = true;
    LayoutObject* trailingSpaceChild = nullptr;

    // Firefox and Opera will allow a table cell to grow to fit an image inside it under
    // very specific cirucumstances (in order to match common WinIE layouts).
    // Not supporting the quirk has caused us to mis-layout some real sites. (See Bugzilla 10517.)
    bool allowImagesToBreak = !document().inQuirksMode() || !isTableCell() || !styleToUse.logicalWidth().isIntrinsicOrAuto();

    bool autoWrap, oldAutoWrap;
    autoWrap = oldAutoWrap = styleToUse.autoWrap();

    InlineMinMaxIterator childIterator(this);

    // Only gets added to the max preffered width once.
    bool addedTextIndent = false;
    // Signals the text indent was more negative than the min preferred width
    bool hasRemainingNegativeTextIndent = false;

    LayoutUnit textIndent = minimumValueForLength(styleToUse.textIndent(), cw);
    LayoutObject* prevFloat = nullptr;
    bool isPrevChildInlineFlow = false;
    bool shouldBreakLineAfterText = false;
    while (LayoutObject* child = childIterator.next()) {
        autoWrap = child->isAtomicInlineLevel() ? child->parent()->style()->autoWrap() :
            child->style()->autoWrap();

        if (!child->isBR()) {
            // Step One: determine whether or not we need to go ahead and
            // terminate our current line. Each discrete chunk can become
            // the new min-width, if it is the widest chunk seen so far, and
            // it can also become the max-width.

            // Children fall into three categories:
            // (1) An inline flow object. These objects always have a min/max of 0,
            // and are included in the iteration solely so that their margins can
            // be added in.
            //
            // (2) An inline non-text non-flow object, e.g., an inline replaced element.
            // These objects can always be on a line by themselves, so in this situation
            // we need to go ahead and break the current line, and then add in our own
            // margins and min/max width on its own line, and then terminate the line.
            //
            // (3) A text object. Text runs can have breakable characters at the start,
            // the middle or the end. They may also lose whitespace off the front if
            // we're already ignoring whitespace. In order to compute accurate min-width
            // information, we need three pieces of information.
            // (a) the min-width of the first non-breakable run. Should be 0 if the text string
            // starts with whitespace.
            // (b) the min-width of the last non-breakable run. Should be 0 if the text string
            // ends with whitespace.
            // (c) the min/max width of the string (trimmed for whitespace).
            //
            // If the text string starts with whitespace, then we need to go ahead and
            // terminate our current line (unless we're already in a whitespace stripping
            // mode.
            //
            // If the text string has a breakable character in the middle, but didn't start
            // with whitespace, then we add the width of the first non-breakable run and
            // then end the current line. We then need to use the intermediate min/max width
            // values (if any of them are larger than our current min/max). We then look at
            // the width of the last non-breakable run and use that to start a new line
            // (unless we end in whitespace).
            LayoutUnit childMin;
            LayoutUnit childMax;

            if (!child->isText()) {
                // Case (1) and (2). Inline replaced and inline flow elements.
                if (child->isLayoutInline()) {
                    adjustMinMaxForInlineFlow(child, childIterator.endOfInline,
                        childMin, childMax);
                    inlineMin += childMin;
                    inlineMax += childMax;
                    child->clearPreferredLogicalWidthsDirty();
                } else {
                    adjustMarginForInlineReplaced(child, childMin, childMax);
                }
            }

            if (!child->isLayoutInline() && !child->isText()) {
                // Case (2). Inline replaced elements and floats.
                // Go ahead and terminate the current line as far as
                // minwidth is concerned.
                LayoutUnit childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth;
                computeChildPreferredLogicalWidths(*child, childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth);
                childMin += childMinPreferredLogicalWidth;
                childMax += childMaxPreferredLogicalWidth;

                bool clearPreviousFloat;
                if (child->isFloating()) {
                    const ComputedStyle& childStyle = child->styleRef();
                    clearPreviousFloat = (prevFloat
                        && ((prevFloat->styleRef().floating() == LeftFloat && (childStyle.clear() & ClearLeft))
                            || (prevFloat->styleRef().floating() == RightFloat && (childStyle.clear() & ClearRight))));
                    prevFloat = child;
                } else {
                    clearPreviousFloat = false;
                }

                bool canBreakReplacedElement = !child->isImage() || allowImagesToBreak;
                if ((canBreakReplacedElement && (autoWrap || oldAutoWrap) && (!isPrevChildInlineFlow || shouldBreakLineAfterText)) || clearPreviousFloat) {
                    minLogicalWidth = std::max(minLogicalWidth, inlineMin);
                    inlineMin = LayoutUnit();
                }

                // If we're supposed to clear the previous float, then terminate maxwidth as well.
                if (clearPreviousFloat) {
                    maxLogicalWidth = std::max(maxLogicalWidth, inlineMax);
                    inlineMax = LayoutUnit();
                }

                // Add in text-indent. This is added in only once.
                if (!addedTextIndent && !child->isFloating()) {
                    childMin += textIndent;
                    childMax += textIndent;

                    if (childMin < LayoutUnit())
                        textIndent = childMin;
                    else
                        addedTextIndent = true;
                }

                // Add our width to the max.
                inlineMax += std::max(LayoutUnit(), childMax);

                if (!autoWrap || !canBreakReplacedElement || (isPrevChildInlineFlow && !shouldBreakLineAfterText)) {
                    if (child->isFloating())
                        minLogicalWidth = std::max(minLogicalWidth, childMin);
                    else
                        inlineMin += childMin;
                } else {
                    // Now check our line.
                    minLogicalWidth = std::max(minLogicalWidth, childMin);

                    // Now start a new line.
                    inlineMin = LayoutUnit();
                }

                if (autoWrap && canBreakReplacedElement && isPrevChildInlineFlow) {
                    minLogicalWidth = std::max(minLogicalWidth, inlineMin);
                    inlineMin = LayoutUnit();
                }

                // We are no longer stripping whitespace at the start of
                // a line.
                if (!child->isFloating()) {
                    stripFrontSpaces = false;
                    trailingSpaceChild = nullptr;
                }
            } else if (child->isText()) {
                // Case (3). Text.
                LayoutText* t = toLayoutText(child);

                if (t->isWordBreak()) {
                    minLogicalWidth = std::max(minLogicalWidth, inlineMin);
                    inlineMin = LayoutUnit();
                    continue;
                }

                // Determine if we have a breakable character. Pass in
                // whether or not we should ignore any spaces at the front
                // of the string. If those are going to be stripped out,
                // then they shouldn't be considered in the breakable char
                // check.
                bool hasBreakableChar, hasBreak;
                LayoutUnit firstLineMinWidth, lastLineMinWidth;
                bool hasBreakableStart, hasBreakableEnd;
                LayoutUnit firstLineMaxWidth, lastLineMaxWidth;
                t->trimmedPrefWidths(inlineMax,
                    firstLineMinWidth, hasBreakableStart, lastLineMinWidth, hasBreakableEnd,
                    hasBreakableChar, hasBreak, firstLineMaxWidth, lastLineMaxWidth,
                    childMin, childMax, stripFrontSpaces, styleToUse.direction());

                // This text object will not be laid out, but it may still provide a breaking opportunity.
                if (!hasBreak && !childMax) {
                    if (autoWrap && (hasBreakableStart || hasBreakableEnd)) {
                        minLogicalWidth = std::max(minLogicalWidth, inlineMin);
                        inlineMin = LayoutUnit();
                    }
                    continue;
                }

                if (stripFrontSpaces)
                    trailingSpaceChild = child;
                else
                    trailingSpaceChild = nullptr;

                // Add in text-indent. This is added in only once.
                LayoutUnit ti;
                if (!addedTextIndent || hasRemainingNegativeTextIndent) {
                    ti = textIndent;
                    childMin += ti;
                    firstLineMinWidth += ti;

                    // It the text indent negative and larger than the child minimum, we re-use the remainder
                    // in future minimum calculations, but using the negative value again on the maximum
                    // will lead to under-counting the max pref width.
                    if (!addedTextIndent) {
                        childMax += ti;
                        firstLineMaxWidth += ti;
                        addedTextIndent = true;
                    }

                    if (childMin < LayoutUnit()) {
                        textIndent = childMin;
                        hasRemainingNegativeTextIndent = true;
                    }
                }

                // If we have no breakable characters at all,
                // then this is the easy case. We add ourselves to the current
                // min and max and continue.
                if (!hasBreakableChar) {
                    inlineMin += childMin;
                } else {
                    if (hasBreakableStart) {
                        minLogicalWidth = std::max(minLogicalWidth, inlineMin);
                    } else {
                        inlineMin += firstLineMinWidth;
                        minLogicalWidth = std::max(minLogicalWidth, inlineMin);
                        childMin -= ti;
                    }

                    inlineMin = childMin;

                    if (hasBreakableEnd) {
                        minLogicalWidth = std::max(minLogicalWidth, inlineMin);
                        inlineMin = LayoutUnit();
                        shouldBreakLineAfterText = false;
                    } else {
                        minLogicalWidth = std::max(minLogicalWidth, inlineMin);
                        inlineMin = lastLineMinWidth;
                        shouldBreakLineAfterText = true;
                    }
                }

                if (hasBreak) {
                    inlineMax += firstLineMaxWidth;
                    maxLogicalWidth = std::max(maxLogicalWidth, inlineMax);
                    maxLogicalWidth = std::max(maxLogicalWidth, childMax);
                    inlineMax = lastLineMaxWidth;
                    addedTextIndent = true;
                } else {
                    inlineMax += std::max(LayoutUnit(), childMax);
                }
            }

            // Ignore spaces after a list marker.
            if (child->isListMarker())
                stripFrontSpaces = true;
        } else {
            minLogicalWidth = std::max(minLogicalWidth, inlineMin);
            maxLogicalWidth = std::max(maxLogicalWidth, inlineMax);
            inlineMin = inlineMax = LayoutUnit();
            stripFrontSpaces = true;
            trailingSpaceChild = nullptr;
            addedTextIndent = true;
        }

        if (!child->isText() && child->isLayoutInline())
            isPrevChildInlineFlow = true;
        else
            isPrevChildInlineFlow = false;

        oldAutoWrap = autoWrap;
    }

    if (styleToUse.collapseWhiteSpace())
        stripTrailingSpace(inlineMax, inlineMin, trailingSpaceChild);

    minLogicalWidth = std::max(minLogicalWidth, inlineMin);
    maxLogicalWidth = std::max(maxLogicalWidth, inlineMax);
}

static bool isInlineWithOutlineAndContinuation(const LayoutObject& o)
{
    return o.isLayoutInline() && o.styleRef().hasOutline() && !o.isElementContinuation() && toLayoutInline(o).continuation();
}

static inline bool shouldTruncateOverflowingText(const LayoutBlockFlow* block)
{
    const LayoutObject* objectToCheck = block;
    if (block->isAnonymousBlock()) {
        const LayoutObject* parent = block->parent();
        if (!parent || !parent->behavesLikeBlockContainer())
            return false;
        objectToCheck = parent;
    }
    return objectToCheck->hasOverflowClip() && objectToCheck->style()->getTextOverflow();
}

void LayoutBlockFlow::layoutInlineChildren(bool relayoutChildren, LayoutUnit afterEdge)
{
    // Figure out if we should clear out our line boxes.
    // FIXME: Handle resize eventually!
    bool isFullLayout = !firstLineBox() || selfNeedsLayout() || relayoutChildren;
    LineLayoutState layoutState(isFullLayout);

    if (isFullLayout) {
        // Ensure the old line boxes will be erased.
        if (firstLineBox())
            setShouldDoFullPaintInvalidation();
        lineBoxes()->deleteLineBoxes();
    }

    // Text truncation kicks in if overflow isn't visible and text-overflow isn't 'clip'. If this is
    // an anonymous block, we have to examine the parent.
    // FIXME: CSS3 says that descendants that are clipped must also know how to truncate.  This is insanely
    // difficult to figure out in general (especially in the middle of doing layout), so we only handle the
    // simple case of an anonymous block truncating when its parent is clipped.
    bool hasTextOverflow = shouldTruncateOverflowingText(this);

    // Walk all the lines and delete our ellipsis line boxes if they exist.
    if (hasTextOverflow)
        deleteEllipsisLineBoxes();

    if (firstChild()) {
        for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.atEnd(); walker.advance()) {
            LayoutObject* o = walker.current().layoutObject();

            if (!layoutState.hasInlineChild() && o->isInline())
                layoutState.setHasInlineChild(true);

            if (o->isAtomicInlineLevel() || o->isFloating() || o->isOutOfFlowPositioned()) {
                LayoutBox* box = toLayoutBox(o);
                box->setMayNeedPaintInvalidation();

                updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, *box);

                if (o->isOutOfFlowPositioned()) {
                    o->containingBlock()->insertPositionedObject(box);
                } else if (o->isFloating()) {
                    layoutState.floats().append(FloatWithRect(box));
                    if (box->needsLayout()) {
                        box->layout();
                        // Dirty any lineboxes potentially affected by the float, but don't search outside this
                        // object as we are only interested in dirtying lineboxes to which we may attach the float.
                        dirtyLinesFromChangedChild(box, MarkOnlyThis);
                    }
                } else if (isFullLayout || o->needsLayout()) {
                    // Atomic inline.
                    box->dirtyLineBoxes(isFullLayout);
                    o->layoutIfNeeded();
                }
            } else if (o->isText() || (o->isLayoutInline() && !walker.atEndOfInline())) {
                if (!o->isText())
                    toLayoutInline(o)->updateAlwaysCreateLineBoxes(layoutState.isFullLayout());
                if (layoutState.isFullLayout() || o->selfNeedsLayout())
                    dirtyLineBoxesForObject(o, layoutState.isFullLayout());
                o->clearNeedsLayout();
            }

            if (isInlineWithOutlineAndContinuation(*o))
                setContainsInlineWithOutlineAndContinuation(true);
        }

        layoutRunsAndFloats(layoutState);
    }

    // Expand the last line to accommodate Ruby and emphasis marks.
    int lastLineAnnotationsAdjustment = 0;
    if (lastRootBox()) {
        LayoutUnit lowestAllowedPosition = std::max(lastRootBox()->lineBottom(), logicalHeight() + paddingAfter());
        if (!style()->isFlippedLinesWritingMode())
            lastLineAnnotationsAdjustment = lastRootBox()->computeUnderAnnotationAdjustment(lowestAllowedPosition);
        else
            lastLineAnnotationsAdjustment = lastRootBox()->computeOverAnnotationAdjustment(lowestAllowedPosition);
    }

    // Now add in the bottom border/padding.
    setLogicalHeight(logicalHeight() + lastLineAnnotationsAdjustment + afterEdge);

    if (!firstLineBox() && hasLineIfEmpty())
        setLogicalHeight(logicalHeight() + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));

    // See if we have any lines that spill out of our block.  If we do, then we will possibly need to
    // truncate text.
    if (hasTextOverflow)
        checkLinesForTextOverflow();

    // Ensure the new line boxes will be painted.
    if (isFullLayout && firstLineBox())
        setShouldDoFullPaintInvalidation();
}

RootInlineBox* LayoutBlockFlow::determineStartPosition(LineLayoutState& layoutState, InlineBidiResolver& resolver)
{
    RootInlineBox* curr = nullptr;
    RootInlineBox* last = nullptr;
    RootInlineBox* firstLineBoxWithBreakAndClearance = 0;

    // FIXME: This entire float-checking block needs to be broken into a new function.
    if (!layoutState.isFullLayout()) {
        // Paginate all of the clean lines.
        bool paginated = view()->layoutState() && view()->layoutState()->isPaginated();
        LayoutUnit paginationDelta;
        for (curr = firstRootBox(); curr && !curr->isDirty(); curr = curr->nextRootBox()) {
            if (paginated) {
                paginationDelta -= curr->paginationStrut();
                adjustLinePositionForPagination(*curr, paginationDelta);
                if (paginationDelta) {
                    if (containsFloats() || !layoutState.floats().isEmpty()) {
                        // FIXME: Do better eventually.  For now if we ever shift because of pagination and floats are present just go to a full layout.
                        layoutState.markForFullLayout();
                        break;
                    }
                    curr->moveInBlockDirection(paginationDelta);
                }
            }

            // If the linebox breaks cleanly and with clearance then dirty from at least this point onwards so that we can clear the correct floats without difficulty.
            if (!firstLineBoxWithBreakAndClearance && lineBoxHasBRWithClearance(curr))
                firstLineBoxWithBreakAndClearance = curr;

            if (layoutState.isFullLayout())
                break;
        }
    }

    if (layoutState.isFullLayout()) {
        // If we encountered a new float and have inline children, mark ourself to force us to issue paint invalidations.
        if (layoutState.hasInlineChild() && !selfNeedsLayout()) {
            setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::FloatDescendantChanged, MarkOnlyThis);
            setShouldDoFullPaintInvalidation();
        }

        deleteLineBoxTree();
        curr = nullptr;
        ASSERT(!firstLineBox() && !lastLineBox());
    } else {
        if (firstLineBoxWithBreakAndClearance)
            curr = firstLineBoxWithBreakAndClearance;
        if (curr) {
            // We have a dirty line.
            if (RootInlineBox* prevRootBox = curr->prevRootBox()) {
                // We have a previous line.
                if (!prevRootBox->endsWithBreak() || !prevRootBox->lineBreakObj() || (prevRootBox->lineBreakObj().isText() && prevRootBox->lineBreakPos() >= toLayoutText(prevRootBox->lineBreakObj().layoutObject())->textLength())) {
                    // The previous line didn't break cleanly or broke at a newline
                    // that has been deleted, so treat it as dirty too.
                    curr = prevRootBox;
                }
            }
        } else {
            // No dirty lines were found.
            // If the last line didn't break cleanly, treat it as dirty.
            if (lastRootBox() && !lastRootBox()->endsWithBreak())
                curr = lastRootBox();
        }

        // If we have no dirty lines, then last is just the last root box.
        last = curr ? curr->prevRootBox() : lastRootBox();
    }

    unsigned numCleanFloats = 0;
    if (!layoutState.floats().isEmpty()) {
        LayoutUnit savedLogicalHeight = logicalHeight();
        // Restore floats from clean lines.
        RootInlineBox* line = firstRootBox();
        while (line != curr) {
            if (Vector<LayoutBox*>* cleanLineFloats = line->floatsPtr()) {
                for (auto* box : *cleanLineFloats) {
                    FloatingObject* floatingObject = insertFloatingObject(*box);
                    ASSERT(!floatingObject->originatingLine());
                    floatingObject->setOriginatingLine(line);
                    setLogicalHeight(logicalTopForChild(*box) - marginBeforeForChild(*box));
                    positionNewFloats();
                    ASSERT(layoutState.floats()[numCleanFloats].object == box);
                    numCleanFloats++;
                }
            }
            line = line->nextRootBox();
        }
        setLogicalHeight(savedLogicalHeight);
    }
    layoutState.setFloatIndex(numCleanFloats);

    layoutState.lineInfo().setFirstLine(!last);
    layoutState.lineInfo().setPreviousLineBrokeCleanly(!last || last->endsWithBreak());

    if (last) {
        setLogicalHeight(last->lineBottomWithLeading());
        InlineIterator iter = InlineIterator(LineLayoutBlockFlow(this), LineLayoutItem(last->lineBreakObj()), last->lineBreakPos());
        resolver.setPosition(iter, numberOfIsolateAncestors(iter));
        resolver.setStatus(last->lineBreakBidiStatus());
    } else {
        TextDirection direction = style()->direction();
        if (style()->unicodeBidi() == Plaintext)
            direction = determinePlaintextDirectionality(LineLayoutItem(this));
        resolver.setStatus(BidiStatus(direction, isOverride(style()->unicodeBidi())));
        InlineIterator iter = InlineIterator(LineLayoutBlockFlow(this), bidiFirstSkippingEmptyInlines(LineLayoutBlockFlow(this), resolver.runs(), &resolver), 0);
        resolver.setPosition(iter, numberOfIsolateAncestors(iter));
    }
    return curr;
}


bool LayoutBlockFlow::lineBoxHasBRWithClearance(RootInlineBox* curr)
{
    // If the linebox breaks cleanly and with clearance then dirty from at least this point onwards so that we can clear the correct floats without difficulty.
    if (!curr->endsWithBreak())
        return false;
    InlineBox* lastBox = style()->isLeftToRightDirection() ? curr->lastLeafChild() : curr->firstLeafChild();
    return lastBox && lastBox->getLineLayoutItem().isBR() && lastBox->getLineLayoutItem().style()->clear() != ClearNone;
}

void LayoutBlockFlow::determineEndPosition(LineLayoutState& layoutState, RootInlineBox* startLine, InlineIterator& cleanLineStart, BidiStatus& cleanLineBidiStatus)
{
    ASSERT(!layoutState.endLine());
    RootInlineBox* last = nullptr;
    for (RootInlineBox* curr = startLine->nextRootBox(); curr; curr = curr->nextRootBox()) {
        if (!curr->isDirty() && lineBoxHasBRWithClearance(curr))
            return;

        if (curr->isDirty())
            last = nullptr;
        else if (!last)
            last = curr;
    }

    if (!last)
        return;

    // At this point, |last| is the first line in a run of clean lines that ends with the last line
    // in the block.

    RootInlineBox* prev = last->prevRootBox();
    cleanLineStart = InlineIterator(LineLayoutItem(this), LineLayoutItem(prev->lineBreakObj()), prev->lineBreakPos());
    cleanLineBidiStatus = prev->lineBreakBidiStatus();
    layoutState.setEndLineLogicalTop(prev->lineBottomWithLeading());

    for (RootInlineBox* line = last; line; line = line->nextRootBox())
        line->extractLine(); // Disconnect all line boxes from their layout objects while preserving
            // their connections to one another.

    layoutState.setEndLine(last);
}

bool LayoutBlockFlow::checkPaginationAndFloatsAtEndLine(LineLayoutState& layoutState)
{
    if (!m_floatingObjects || !layoutState.endLine())
        return true;

    LayoutUnit lineDelta = logicalHeight() - layoutState.endLineLogicalTop();

    bool paginated = view()->layoutState() && view()->layoutState()->isPaginated();
    if (paginated) {
        // Check all lines from here to the end, and see if the hypothetical new position for the lines will result
        // in a different available line width.
        for (RootInlineBox* lineBox = layoutState.endLine(); lineBox; lineBox = lineBox->nextRootBox()) {
            // This isn't the real move we're going to do, so don't update the line box's pagination
            // strut yet.
            LayoutUnit oldPaginationStrut = lineBox->paginationStrut();
            lineDelta -= oldPaginationStrut;
            adjustLinePositionForPagination(*lineBox, lineDelta);
            lineBox->setPaginationStrut(oldPaginationStrut);
        }
    }
    if (!lineDelta)
        return true;

    // See if any floats end in the range along which we want to shift the lines vertically.
    LayoutUnit logicalTop = std::min(logicalHeight(), layoutState.endLineLogicalTop());

    RootInlineBox* lastLine = layoutState.endLine();
    while (RootInlineBox* nextLine = lastLine->nextRootBox())
        lastLine = nextLine;

    LayoutUnit logicalBottom = lastLine->lineBottomWithLeading() + absoluteValue(lineDelta);

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    FloatingObjectSetIterator end = floatingObjectSet.end();
    for (FloatingObjectSetIterator it = floatingObjectSet.begin(); it != end; ++it) {
        const FloatingObject& floatingObject = *it->get();
        if (logicalBottomForFloat(floatingObject) >= logicalTop && logicalBottomForFloat(floatingObject) < logicalBottom)
            return false;
    }

    return true;
}

bool LayoutBlockFlow::matchedEndLine(LineLayoutState& layoutState, const InlineBidiResolver& resolver, const InlineIterator& endLineStart, const BidiStatus& endLineStatus)
{
    if (resolver.position() == endLineStart) {
        if (resolver.status() != endLineStatus)
            return false;

        return checkPaginationAndFloatsAtEndLine(layoutState);
    }

    // The first clean line doesn't match, but we can check a handful of following lines to try
    // to match back up.
    static int numLines = 8; // The # of lines we're willing to match against.
    RootInlineBox* originalEndLine = layoutState.endLine();
    RootInlineBox* line = originalEndLine;
    for (int i = 0; i < numLines && line; i++, line = line->nextRootBox()) {
        if (line->lineBreakObj() == resolver.position().getLineLayoutItem() && line->lineBreakPos() == resolver.position().offset()) {
            // We have a match.
            if (line->lineBreakBidiStatus() != resolver.status())
                return false; // ...but the bidi state doesn't match.

            bool matched = false;
            RootInlineBox* result = line->nextRootBox();
            layoutState.setEndLine(result);
            if (result) {
                layoutState.setEndLineLogicalTop(line->lineBottomWithLeading());
                matched = checkPaginationAndFloatsAtEndLine(layoutState);
            }

            // Now delete the lines that we failed to sync.
            deleteLineRange(layoutState, originalEndLine, result);
            return matched;
        }
    }

    return false;
}

bool LayoutBlockFlow::generatesLineBoxesForInlineChild(LayoutObject* inlineObj)

{
    ASSERT(inlineObj->parent() == this);

    InlineIterator it(LineLayoutBlockFlow(this), LineLayoutItem(inlineObj), 0);
    // FIXME: We should pass correct value for WhitespacePosition.
    while (!it.atEnd() && !requiresLineBox(it))
        it.increment();

    return !it.atEnd();
}


void LayoutBlockFlow::addOverflowFromInlineChildren()
{
    LayoutUnit endPadding = hasOverflowClip() ? paddingEnd() : LayoutUnit();
    // FIXME: Need to find another way to do this, since scrollbars could show when we don't want them to.
    if (hasOverflowClip() && !endPadding && node() && node()->isRootEditableElement() && style()->isLeftToRightDirection())
        endPadding = LayoutUnit(1);
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        addLayoutOverflow(curr->paddedLayoutOverflowRect(endPadding));
        LayoutRect visualOverflow = curr->visualOverflowRect(curr->lineTop(), curr->lineBottom());
        addContentsVisualOverflow(visualOverflow);
    }

    if (!containsInlineWithOutlineAndContinuation())
        return;

    // Add outline rects of continuations of descendant inlines into visual overflow of this block.
    LayoutRect outlineBoundsOfAllContinuations;
    for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.atEnd(); walker.advance()) {
        const LayoutObject& o = *walker.current().layoutObject();
        if (!isInlineWithOutlineAndContinuation(o))
            continue;

        Vector<LayoutRect> outlineRects;
        toLayoutInline(o).addOutlineRectsForContinuations(outlineRects, LayoutPoint(), o.outlineRectsShouldIncludeBlockVisualOverflow());
        if (!outlineRects.isEmpty()) {
            LayoutRect outlineBounds = unionRectEvenIfEmpty(outlineRects);
            outlineBounds.inflate(LayoutUnit(o.styleRef().outlineOutsetExtent()));
            outlineBoundsOfAllContinuations.unite(outlineBounds);
        }
    }
    addContentsVisualOverflow(outlineBoundsOfAllContinuations);
}

void LayoutBlockFlow::deleteEllipsisLineBoxes()
{
    ETextAlign textAlign = style()->textAlign();
    IndentTextOrNot indentText = IndentText;
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        if (curr->hasEllipsisBox()) {
            curr->clearTruncation();

            // Shift the line back where it belongs if we cannot accommodate an ellipsis.
            LayoutUnit logicalLeft = logicalLeftOffsetForLine(curr->lineTop(), indentText);
            LayoutUnit availableLogicalWidth = logicalRightOffsetForLine(curr->lineTop(), DoNotIndentText) - logicalLeft;
            LayoutUnit totalLogicalWidth = curr->logicalWidth();
            updateLogicalWidthForAlignment(textAlign, curr, 0, logicalLeft, totalLogicalWidth, availableLogicalWidth, 0);

            curr->moveInInlineDirection(logicalLeft - curr->logicalLeft());
        }
        indentText = DoNotIndentText;
    }
}

void LayoutBlockFlow::checkLinesForTextOverflow()
{
    // Determine the width of the ellipsis using the current font.
    const Font& font = style()->font();

    const size_t fullStopStringLength = 3;
    const UChar fullStopString[] = {fullstopCharacter, fullstopCharacter, fullstopCharacter};
    DEFINE_STATIC_LOCAL(AtomicString, fullstopCharacterStr, (fullStopString, fullStopStringLength));
    DEFINE_STATIC_LOCAL(AtomicString, ellipsisStr, (&horizontalEllipsisCharacter, 1));
    AtomicString& selectedEllipsisStr = ellipsisStr;

    const Font& firstLineFont = firstLineStyle()->font();
    // FIXME: We should probably not hard-code the direction here. https://crbug.com/333004
    TextDirection ellipsisDirection = LTR;
    float firstLineEllipsisWidth = 0;
    float ellipsisWidth = 0;

    // As per CSS3 http://www.w3.org/TR/2003/CR-css3-text-20030514/ sequence of three
    // Full Stops (002E) can be used.
    ASSERT(firstLineFont.primaryFont());
    if (firstLineFont.primaryFont()->glyphForCharacter(horizontalEllipsisCharacter)) {
        firstLineEllipsisWidth = firstLineFont.width(constructTextRun(firstLineFont, &horizontalEllipsisCharacter, 1, *firstLineStyle(), ellipsisDirection));
    } else {
        selectedEllipsisStr = fullstopCharacterStr;
        firstLineEllipsisWidth = firstLineFont.width(constructTextRun(firstLineFont, fullStopString, fullStopStringLength, *firstLineStyle(), ellipsisDirection));
    }
    ellipsisWidth = (font == firstLineFont) ? firstLineEllipsisWidth : 0;

    if (!ellipsisWidth) {
        ASSERT(font.primaryFont());
        if (font.primaryFont()->glyphForCharacter(horizontalEllipsisCharacter)) {
            selectedEllipsisStr = ellipsisStr;
            ellipsisWidth = font.width(constructTextRun(font, &horizontalEllipsisCharacter, 1, styleRef(), ellipsisDirection));
        } else {
            selectedEllipsisStr = fullstopCharacterStr;
            ellipsisWidth = font.width(constructTextRun(font, fullStopString, fullStopStringLength, styleRef(), ellipsisDirection));
        }
    }

    // For LTR text truncation, we want to get the right edge of our padding box, and then we want to see
    // if the right edge of a line box exceeds that.  For RTL, we use the left edge of the padding box and
    // check the left edge of the line box to see if it is less
    // Include the scrollbar for overflow blocks, which means we want to use "contentWidth()"
    bool ltr = style()->isLeftToRightDirection();
    ETextAlign textAlign = style()->textAlign();
    IndentTextOrNot indentText = IndentText;
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        LayoutUnit currLogicalLeft = curr->logicalLeft();
        LayoutUnit blockRightEdge = logicalRightOffsetForLine(curr->lineTop(), indentText);
        LayoutUnit blockLeftEdge = logicalLeftOffsetForLine(curr->lineTop(), indentText);
        LayoutUnit lineBoxEdge = ltr ? currLogicalLeft + curr->logicalWidth() : currLogicalLeft;
        if ((ltr && lineBoxEdge > blockRightEdge) || (!ltr && lineBoxEdge < blockLeftEdge)) {
            // This line spills out of our box in the appropriate direction.  Now we need to see if the line
            // can be truncated.  In order for truncation to be possible, the line must have sufficient space to
            // accommodate our truncation string, and no replaced elements (images, tables) can overlap the ellipsis
            // space.

            LayoutUnit width(indentText == IndentText ? firstLineEllipsisWidth : ellipsisWidth);
            LayoutUnit blockEdge = ltr ? blockRightEdge : blockLeftEdge;
            if (curr->lineCanAccommodateEllipsis(ltr, blockEdge, lineBoxEdge, width)) {
                LayoutUnit totalLogicalWidth = curr->placeEllipsis(selectedEllipsisStr, ltr, blockLeftEdge, blockRightEdge, width);
                LayoutUnit logicalLeft; // We are only interested in the delta from the base position.
                LayoutUnit availableLogicalWidth = blockRightEdge - blockLeftEdge;
                updateLogicalWidthForAlignment(textAlign, curr, 0, logicalLeft, totalLogicalWidth, availableLogicalWidth, 0);
                if (ltr)
                    curr->moveInInlineDirection(logicalLeft);
                else
                    curr->moveInInlineDirection(logicalLeft - (availableLogicalWidth - totalLogicalWidth));
            }
        }
        indentText = DoNotIndentText;
    }
}

void LayoutBlockFlow::markLinesDirtyInBlockRange(LayoutUnit logicalTop, LayoutUnit logicalBottom, RootInlineBox* highest)
{
    if (logicalTop >= logicalBottom)
        return;

    RootInlineBox* lowestDirtyLine = lastRootBox();
    RootInlineBox* afterLowest = lowestDirtyLine;
    while (lowestDirtyLine && lowestDirtyLine->lineBottomWithLeading() >= logicalBottom && logicalBottom < LayoutUnit::max()) {
        afterLowest = lowestDirtyLine;
        lowestDirtyLine = lowestDirtyLine->prevRootBox();
    }

    while (afterLowest && afterLowest != highest && (afterLowest->lineBottomWithLeading() >= logicalTop || afterLowest->lineBottomWithLeading() < LayoutUnit())) {
        afterLowest->markDirty();
        afterLowest = afterLowest->prevRootBox();
    }
}

LayoutUnit LayoutBlockFlow::startAlignedOffsetForLine(LayoutUnit position, IndentTextOrNot indentText)
{
    ETextAlign textAlign = style()->textAlign();

    bool applyIndentText;
    switch (textAlign) { // FIXME: Handle TAEND here
    case LEFT:
    case WEBKIT_LEFT:
        applyIndentText = style()->isLeftToRightDirection();
        break;
    case RIGHT:
    case WEBKIT_RIGHT:
        applyIndentText = !style()->isLeftToRightDirection();
        break;
    case TASTART:
        applyIndentText = true;
        break;
    default:
        applyIndentText = false;
    }

    if (applyIndentText)
        return startOffsetForLine(position, indentText);

    // updateLogicalWidthForAlignment() handles the direction of the block so no need to consider it here
    LayoutUnit totalLogicalWidth;
    LayoutUnit logicalLeft = logicalLeftOffsetForLine(logicalHeight(), DoNotIndentText);
    LayoutUnit availableLogicalWidth = logicalRightOffsetForLine(logicalHeight(), DoNotIndentText) - logicalLeft;
    updateLogicalWidthForAlignment(textAlign, 0, 0, logicalLeft, totalLogicalWidth, availableLogicalWidth, 0);

    if (!style()->isLeftToRightDirection())
        return logicalWidth() - logicalLeft;
    return logicalLeft;
}

void LayoutBlockFlow::setShouldDoFullPaintInvalidationForFirstLine()
{
    ASSERT(childrenInline());
    if (RootInlineBox* firstRootBox = this->firstRootBox())
        firstRootBox->setShouldDoFullPaintInvalidationRecursively();
}

PaintInvalidationReason LayoutBlockFlow::invalidatePaintIfNeeded(const PaintInvalidationState& paintInvalidationState)
{
    if (containsFloats())
        paintInvalidationState.paintingLayer().setNeedsPaintPhaseFloat();

    PaintInvalidationReason reason = LayoutBlock::invalidatePaintIfNeeded(paintInvalidationState);
    if (reason == PaintInvalidationNone)
        return reason;
    RootInlineBox* line = firstRootBox();
    if (!line || !line->isFirstLineStyle())
        return reason;
    // It's the RootInlineBox that paints the ::first-line background. Note that since it may be
    // expensive to figure out if the first line is affected by any ::first-line selectors at all,
    // we just invalidate it unconditionally, since that's typically cheaper.
    invalidateDisplayItemClient(*line, reason);
    return reason;
}

} // namespace blink
