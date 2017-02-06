/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated.
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

#ifndef BreakingContextInlineHeaders_h
#define BreakingContextInlineHeaders_h

#include "core/layout/TextRunConstructor.h"
#include "core/layout/api/LineLayoutBox.h"
#include "core/layout/api/LineLayoutListMarker.h"
#include "core/layout/api/LineLayoutRubyRun.h"
#include "core/layout/api/LineLayoutSVGInlineText.h"
#include "core/layout/api/LineLayoutText.h"
#include "core/layout/api/LineLayoutTextCombine.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/line/LayoutTextInfo.h"
#include "core/layout/line/LineBreaker.h"
#include "core/layout/line/LineInfo.h"
#include "core/layout/line/LineWidth.h"
#include "core/layout/line/TrailingObjects.h"
#include "core/layout/line/WordMeasurement.h"
#include "core/paint/PaintLayer.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/text/Hyphenation.h"
#include "platform/text/TextBreakIterator.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"

namespace blink {

// We don't let our line box tree for a single line get any deeper than this.
const unsigned cMaxLineDepth = 200;

class BreakingContext {
    STACK_ALLOCATED();
public:
    BreakingContext(InlineBidiResolver& resolver, LineInfo& inLineInfo, LineWidth& lineWidth, LayoutTextInfo& inLayoutTextInfo, bool appliedStartWidth, LineLayoutBlockFlow block)
        : m_resolver(resolver)
        , m_current(resolver.position())
        , m_lineBreak(resolver.position())
        , m_block(block)
        , m_lastObject(m_current.getLineLayoutItem())
        , m_nextObject(nullptr)
        , m_currentStyle(nullptr)
        , m_blockStyle(block.style())
        , m_lineInfo(inLineInfo)
        , m_layoutTextInfo(inLayoutTextInfo)
        , m_width(lineWidth)
        , m_currWS(NORMAL)
        , m_lastWS(NORMAL)
        , m_preservesNewline(false)
        , m_atStart(true)
        , m_ignoringSpaces(false)
        , m_currentCharacterIsSpace(false)
        , m_appliedStartWidth(appliedStartWidth)
        , m_includeEndWidth(true)
        , m_autoWrap(false)
        , m_autoWrapWasEverTrueOnLine(false)
        , m_floatsFitOnLine(true)
        , m_collapseWhiteSpace(false)
        , m_startingNewParagraph(m_lineInfo.previousLineBrokeCleanly())
        , m_allowImagesToBreak(!block.document().inQuirksMode() || !block.isTableCell() || !m_blockStyle->logicalWidth().isIntrinsicOrAuto())
        , m_atEnd(false)
        , m_lineMidpointState(resolver.midpointState())
    {
        m_lineInfo.setPreviousLineBrokeCleanly(false);
    }

    LineLayoutItem currentItem() { return m_current.getLineLayoutItem(); }
    InlineIterator lineBreak() { return m_lineBreak; }
    bool atEnd() { return m_atEnd; }

    void initializeForCurrentObject();

    void increment();

    void handleBR(EClear&);
    void handleOutOfFlowPositioned(Vector<LineLayoutBox>& positionedObjects);
    void handleFloat();
    void handleEmptyInline();
    void handleReplaced();
    bool handleText(WordMeasurements&, bool& hyphenated);
    void prepareForNextCharacter(const LineLayoutText&, bool& prohibitBreakInside, bool previousCharacterIsSpace);
    bool canBreakAtWhitespace(bool breakWords, WordMeasurement&, bool stoppedIgnoringSpaces, float charWidth, bool& hyphenated, bool disableSoftHyphen, float& hyphenWidth, bool betweenWords, bool midWordBreak, bool canBreakMidWord, bool previousCharacterIsSpace, float lastWidthMeasurement, const LineLayoutText&, const Font&, bool applyWordSpacing, float wordSpacing);
    bool trailingSpaceExceedsAvailableWidth(bool canBreakMidWord, const LineLayoutText&, WordMeasurement&, bool applyWordSpacing, bool wordSpacing, const Font&);
    WordMeasurement& calculateWordWidth(WordMeasurements&, LineLayoutText&, unsigned lastSpace, float& lastWidthMeasurement, float wordSpacingForWordMeasurement, const Font&, float wordTrailingSpaceWidth, UChar);
    void stopIgnoringSpaces(unsigned& lastSpace);
    void commitAndUpdateLineBreakIfNeeded();
    InlineIterator handleEndOfLine();

    void clearLineBreakIfFitsOnLine()
    {
        if (m_width.fitsOnLine() || m_lastWS == NOWRAP)
            m_lineBreak.clear();
    }

private:
    void skipTrailingWhitespace(InlineIterator&, const LineInfo&);
    bool shouldMidWordBreak(UChar, LineLayoutText, const Font&,
        float& charWidth, float& widthFromLastBreakingOpportunity,
        bool breakAll, int& nextBreakablePositionForBreakAll);
    bool rewindToMidWordBreak(WordMeasurement&, int end, float width);
    bool rewindToFirstMidWordBreak(LineLayoutText, const ComputedStyle&, const Font&, bool breakAll, WordMeasurement&);
    bool rewindToMidWordBreak(LineLayoutText, const ComputedStyle&, const Font&, bool breakAll, WordMeasurement&);
    bool hyphenate(LineLayoutText, const ComputedStyle&, const Font&, const Hyphenation&, float lastSpaceWordSpacing, WordMeasurement&);

    InlineBidiResolver& m_resolver;

    InlineIterator m_current;
    InlineIterator m_lineBreak;
    InlineIterator m_startOfIgnoredSpaces;

    LineLayoutBlockFlow m_block;
    LineLayoutItem m_lastObject;
    LineLayoutItem m_nextObject;

    const ComputedStyle* m_currentStyle;
    const ComputedStyle* m_blockStyle;

    LineInfo& m_lineInfo;

    LayoutTextInfo& m_layoutTextInfo;

    LineWidth m_width;

    EWhiteSpace m_currWS;
    EWhiteSpace m_lastWS;

    bool m_preservesNewline;
    bool m_atStart;
    bool m_ignoringSpaces;
    bool m_currentCharacterIsSpace;
    bool m_appliedStartWidth;
    bool m_includeEndWidth;
    bool m_autoWrap;
    bool m_autoWrapWasEverTrueOnLine;
    bool m_floatsFitOnLine;
    bool m_collapseWhiteSpace;
    bool m_startingNewParagraph;
    bool m_allowImagesToBreak;
    bool m_atEnd;

    LineMidpointState& m_lineMidpointState;

    TrailingObjects m_trailingObjects;
};

// When ignoring spaces, this needs to be called for objects that need line boxes such as LayoutInlines or
// hard line breaks to ensure that they're not ignored.
inline void ensureLineBoxInsideIgnoredSpaces(LineMidpointState* midpointState, LineLayoutItem item)
{
    InlineIterator midpoint(0, item, 0);
    midpointState->stopIgnoringSpaces(midpoint);
    midpointState->startIgnoringSpaces(midpoint);
}

inline bool shouldCollapseWhiteSpace(const ComputedStyle& style, const LineInfo& lineInfo, WhitespacePosition whitespacePosition)
{
    // CSS2 16.6.1
    // If a space (U+0020) at the beginning of a line has 'white-space' set to 'normal', 'nowrap', or 'pre-line', it is removed.
    // If a space (U+0020) at the end of a line has 'white-space' set to 'normal', 'nowrap', or 'pre-line', it is also removed.
    // If spaces (U+0020) or tabs (U+0009) at the end of a line have 'white-space' set to 'pre-wrap', UAs may visually collapse them.
    return style.collapseWhiteSpace()
        || (whitespacePosition == TrailingWhitespace && style.whiteSpace() == PRE_WRAP && (!lineInfo.isEmpty() || !lineInfo.previousLineBrokeCleanly()));
}

inline bool requiresLineBoxForContent(LineLayoutInline flow, const LineInfo& lineInfo)
{
    LineLayoutItem parent = flow.parent();
    if (flow.document().inNoQuirksMode()
        && (flow.style(lineInfo.isFirstLine())->lineHeight() != parent.style(lineInfo.isFirstLine())->lineHeight()
        || flow.style()->verticalAlign() != parent.style()->verticalAlign()
        || !parent.style()->font().getFontMetrics().hasIdenticalAscentDescentAndLineGap(flow.style()->font().getFontMetrics())))
        return true;
    return false;
}

inline bool alwaysRequiresLineBox(LineLayoutItem flow)
{
    // FIXME: Right now, we only allow line boxes for inlines that are truly empty.
    // We need to fix this, though, because at the very least, inlines containing only
    // ignorable whitespace should should also have line boxes.
    return isEmptyInline(flow) && LineLayoutInline(flow).hasInlineDirectionBordersPaddingOrMargin();
}

inline bool requiresLineBox(const InlineIterator& it, const LineInfo& lineInfo = LineInfo(), WhitespacePosition whitespacePosition = LeadingWhitespace)
{
    if (it.getLineLayoutItem().isFloatingOrOutOfFlowPositioned())
        return false;

    if (it.getLineLayoutItem().isLayoutInline() && !alwaysRequiresLineBox(it.getLineLayoutItem()) && !requiresLineBoxForContent(LineLayoutInline(it.getLineLayoutItem()), lineInfo))
        return false;

    if (!shouldCollapseWhiteSpace(it.getLineLayoutItem().styleRef(), lineInfo, whitespacePosition) || it.getLineLayoutItem().isBR())
        return true;

    UChar current = it.current();
    bool notJustWhitespace = current != spaceCharacter && current != tabulationCharacter && current != softHyphenCharacter && (current != newlineCharacter || it.getLineLayoutItem().preservesNewline());
    return notJustWhitespace || isEmptyInline(it.getLineLayoutItem());
}

inline void setStaticPositions(LineLayoutBlockFlow block, LineLayoutBox child, IndentTextOrNot indentText)
{
    ASSERT(child.isOutOfFlowPositioned());
    // FIXME: The math here is actually not really right. It's a best-guess approximation that
    // will work for the common cases
    LineLayoutItem containerBlock = child.container();
    LayoutUnit blockHeight = block.logicalHeight();
    if (containerBlock.isLayoutInline()) {
        // A relative positioned inline encloses us. In this case, we also have to determine our
        // position as though we were an inline. Set |staticInlinePosition| and |staticBlockPosition| on the relative positioned
        // inline so that we can obtain the value later.
        LineLayoutInline(containerBlock).layer()->setStaticInlinePosition(block.startAlignedOffsetForLine(blockHeight, indentText));
        LineLayoutInline(containerBlock).layer()->setStaticBlockPosition(blockHeight);

        // If |child| is a leading or trailing positioned object this is its only opportunity to ensure it moves with an inline
        // container changing width.
        child.moveWithEdgeOfInlineContainerIfNecessary(child.isHorizontalWritingMode());
    }
    block.updateStaticInlinePositionForChild(child, blockHeight, indentText);
    child.layer()->setStaticBlockPosition(blockHeight);
}

// FIXME: The entire concept of the skipTrailingWhitespace function is flawed, since we really need to be building
// line boxes even for containers that may ultimately collapse away. Otherwise we'll never get positioned
// elements quite right. In other words, we need to build this function's work into the normal line
// object iteration process.
// NB. this function will insert any floating elements that would otherwise
// be skipped but it will not position them.
inline void BreakingContext::skipTrailingWhitespace(InlineIterator& iterator, const LineInfo& lineInfo)
{
    while (!iterator.atEnd() && !requiresLineBox(iterator, lineInfo, TrailingWhitespace)) {
        LineLayoutItem item = iterator.getLineLayoutItem();
        if (item.isOutOfFlowPositioned())
            setStaticPositions(m_block, LineLayoutBox(item), DoNotIndentText);
        else if (item.isFloating())
            m_block.insertFloatingObject(LineLayoutBox(item));
        iterator.increment();
    }
}

inline void BreakingContext::initializeForCurrentObject()
{
    m_currentStyle = m_current.getLineLayoutItem().style();
    m_nextObject = bidiNextSkippingEmptyInlines(m_block, m_current.getLineLayoutItem());
    if (m_nextObject && m_nextObject.parent() && !m_nextObject.parent().isDescendantOf(m_current.getLineLayoutItem().parent()))
        m_includeEndWidth = true;

    m_currWS = m_current.getLineLayoutItem().isLayoutInline() ? m_currentStyle->whiteSpace() : m_current.getLineLayoutItem().parent().style()->whiteSpace();
    m_lastWS = m_lastObject.isLayoutInline() ? m_lastObject.style()->whiteSpace() : m_lastObject.parent().style()->whiteSpace();

    bool isSVGText = m_current.getLineLayoutItem().isSVGInlineText();
    m_autoWrap = !isSVGText && ComputedStyle::autoWrap(m_currWS);
    m_autoWrapWasEverTrueOnLine = m_autoWrapWasEverTrueOnLine || m_autoWrap;

    m_preservesNewline = !isSVGText && ComputedStyle::preserveNewline(m_currWS);

    m_collapseWhiteSpace = ComputedStyle::collapseWhiteSpace(m_currWS);

    // Ensure the whitespace in constructions like '<span style="white-space: pre-wrap">text <span><span> text</span>'
    // does not collapse.
    if (m_collapseWhiteSpace && !ComputedStyle::collapseWhiteSpace(m_lastWS))
        m_currentCharacterIsSpace = false;
}

inline void BreakingContext::increment()
{
    m_current.moveToStartOf(m_nextObject);

    // When the line box tree is created, this position in the line will be snapped to
    // LayoutUnit's, and those measurements will be used by the paint code.  Do the
    // equivalent snapping here, to get consistent line measurements.
    m_width.snapUncommittedWidth();

    m_atStart = false;
}

inline void BreakingContext::handleBR(EClear& clear)
{
    if (m_width.fitsOnLine()) {
        LineLayoutItem br = m_current.getLineLayoutItem();
        m_lineBreak.moveToStartOf(br);
        m_lineBreak.increment();

        // A <br> always breaks a line, so don't let the line be collapsed
        // away. Also, the space at the end of a line with a <br> does not
        // get collapsed away. It only does this if the previous line broke
        // cleanly. Otherwise the <br> has no effect on whether the line is
        // empty or not.
        if (m_startingNewParagraph)
            m_lineInfo.setEmpty(false);
        m_trailingObjects.clear();
        m_lineInfo.setPreviousLineBrokeCleanly(true);

        // A <br> with clearance always needs a linebox in case the lines below it get dirtied later and
        // need to check for floats to clear - so if we're ignoring spaces, stop ignoring them and add a
        // run for this object.
        if (m_ignoringSpaces && m_currentStyle->clear() != ClearNone)
            ensureLineBoxInsideIgnoredSpaces(&m_lineMidpointState, br);

        if (!m_lineInfo.isEmpty())
            clear = m_currentStyle->clear();
    }
    m_atEnd = true;
}

inline LayoutUnit borderPaddingMarginStart(LineLayoutInline child)
{
    return child.marginStart() + child.paddingStart() + child.borderStart();
}

inline LayoutUnit borderPaddingMarginEnd(LineLayoutInline child)
{
    return child.marginEnd() + child.paddingEnd() + child.borderEnd();
}

inline bool shouldAddBorderPaddingMargin(LineLayoutItem child, bool &checkSide)
{
    if (!child || (child.isText() && !LineLayoutText(child).textLength()))
        return true;
    checkSide = false;
    return checkSide;
}

inline LayoutUnit inlineLogicalWidthFromAncestorsIfNeeded(LineLayoutItem child, bool start = true, bool end = true)
{
    unsigned lineDepth = 1;
    LayoutUnit extraWidth;
    LineLayoutItem parent = child.parent();
    while (parent.isLayoutInline() && lineDepth++ < cMaxLineDepth) {
        LineLayoutInline parentAsLayoutInline(parent);
        if (!isEmptyInline(parentAsLayoutInline)) {
            if (start && shouldAddBorderPaddingMargin(child.previousSibling(), start))
                extraWidth += borderPaddingMarginStart(parentAsLayoutInline);
            if (end && shouldAddBorderPaddingMargin(child.nextSibling(), end))
                extraWidth += borderPaddingMarginEnd(parentAsLayoutInline);
            if (!start && !end)
                return extraWidth;
        }
        child = parent;
        parent = child.parent();
    }
    return extraWidth;
}

inline void BreakingContext::handleOutOfFlowPositioned(Vector<LineLayoutBox>& positionedObjects)
{
    // If our original display wasn't an inline type, then we can
    // go ahead and determine our static inline position now.
    LineLayoutBox box(m_current.getLineLayoutItem());
    bool isInlineType = box.style()->isOriginalDisplayInlineType();
    if (!isInlineType) {
        m_block.setStaticInlinePositionForChild(box, m_block.startOffsetForContent());
    } else {
        // If our original display was an INLINE type, then we can go ahead
        // and determine our static y position now.
        box.layer()->setStaticBlockPosition(m_block.logicalHeight());
    }

    // If we're ignoring spaces, we have to stop and include this object and
    // then start ignoring spaces again.
    if (isInlineType || box.container().isLayoutInline()) {
        if (m_ignoringSpaces)
            ensureLineBoxInsideIgnoredSpaces(&m_lineMidpointState, box);
        m_trailingObjects.appendObjectIfNeeded(box);
    } else {
        positionedObjects.append(box);
    }
    m_width.addUncommittedWidth(inlineLogicalWidthFromAncestorsIfNeeded(box).toFloat());
    // Reset prior line break context characters.
    m_layoutTextInfo.m_lineBreakIterator.resetPriorContext();
}

inline void BreakingContext::handleFloat()
{
    LineLayoutBox floatBox(m_current.getLineLayoutItem());
    FloatingObject* floatingObject = m_block.insertFloatingObject(floatBox);
    // check if it fits in the current line.
    // If it does, position it now, otherwise, position
    // it after moving to next line (in newLine() func)
    // FIXME: Bug 110372: Properly position multiple stacked floats with non-rectangular shape outside.
    if (m_floatsFitOnLine && m_width.fitsOnLine(m_block.logicalWidthForFloat(*floatingObject).toFloat(), ExcludeWhitespace)) {
        m_block.positionNewFloats(&m_width);
        if (m_lineBreak.getLineLayoutItem() == m_current.getLineLayoutItem()) {
            ASSERT(!m_lineBreak.offset());
            m_lineBreak.increment();
        }
    } else {
        m_floatsFitOnLine = false;
    }
    // Update prior line break context characters, using U+FFFD (OBJECT REPLACEMENT CHARACTER) for floating element.
    m_layoutTextInfo.m_lineBreakIterator.updatePriorContext(replacementCharacter);
}

// This is currently just used for list markers and inline flows that have line boxes. Neither should
// have an effect on whitespace at the start of the line.
inline bool shouldSkipWhitespaceAfterStartObject(LineLayoutBlockFlow block, LineLayoutItem o, LineMidpointState& lineMidpointState)
{
    LineLayoutItem next = bidiNextSkippingEmptyInlines(block, o);
    while (next && next.isFloatingOrOutOfFlowPositioned())
        next = bidiNextSkippingEmptyInlines(block, next);

    if (next && isEmptyInline(next))
        next = LineLayoutInline(next).firstChild();

    if (next && !next.isBR() && next.isText() && LineLayoutText(next).textLength() > 0) {
        LineLayoutText nextText(next);
        UChar nextChar = nextText.characterAt(0);
        if (nextText.style()->isCollapsibleWhiteSpace(nextChar)) {
            lineMidpointState.startIgnoringSpaces(InlineIterator(0, o, 0));
            return true;
        }
    }

    return false;
}

inline void BreakingContext::handleEmptyInline()
{
    // This should only end up being called on empty inlines
    ASSERT(m_current.getLineLayoutItem());

    LineLayoutInline flowBox(m_current.getLineLayoutItem());

    bool requiresLineBox = alwaysRequiresLineBox(m_current.getLineLayoutItem());
    if (requiresLineBox || requiresLineBoxForContent(flowBox, m_lineInfo)) {
        // An empty inline that only has line-height, vertical-align or font-metrics will
        // not force linebox creation (and thus affect the height of the line) if the rest of the line is empty.
        if (requiresLineBox)
            m_lineInfo.setEmpty(false);
        if (m_ignoringSpaces) {
            // If we are in a run of ignored spaces then ensure we get a linebox if lineboxes are eventually
            // created for the line...
            m_trailingObjects.clear();
            ensureLineBoxInsideIgnoredSpaces(&m_lineMidpointState, m_current.getLineLayoutItem());
        } else if (m_blockStyle->collapseWhiteSpace() && m_resolver.position().getLineLayoutItem() == m_current.getLineLayoutItem()
            && shouldSkipWhitespaceAfterStartObject(m_block, m_current.getLineLayoutItem(), m_lineMidpointState)) {
            // If this object is at the start of the line, we need to behave like list markers and
            // start ignoring spaces.
            m_currentCharacterIsSpace = true;
            m_ignoringSpaces = true;
        } else {
            // If we are after a trailing space but aren't ignoring spaces yet then ensure we get a linebox
            // if we encounter collapsible whitepace.
            m_trailingObjects.appendObjectIfNeeded(m_current.getLineLayoutItem());
        }
    }

    m_width.addUncommittedWidth((inlineLogicalWidthFromAncestorsIfNeeded(m_current.getLineLayoutItem()) + borderPaddingMarginStart(flowBox) + borderPaddingMarginEnd(flowBox)).toFloat());
}

inline void BreakingContext::handleReplaced()
{
    LineLayoutBox replacedBox(m_current.getLineLayoutItem());

    if (m_atStart)
        m_width.updateAvailableWidth(replacedBox.logicalHeight());

    // Break on replaced elements if either has normal white-space,
    // or if the replaced element is ruby that can break before.
    if ((m_autoWrap || ComputedStyle::autoWrap(m_lastWS)) && (!m_current.getLineLayoutItem().isImage() || m_allowImagesToBreak)
        && (!m_current.getLineLayoutItem().isRubyRun() || LineLayoutRubyRun(m_current.getLineLayoutItem()).canBreakBefore(m_layoutTextInfo.m_lineBreakIterator))) {
        m_width.commit();
        m_lineBreak.moveToStartOf(m_current.getLineLayoutItem());
    }

    if (m_ignoringSpaces)
        m_lineMidpointState.stopIgnoringSpaces(InlineIterator(0, m_current.getLineLayoutItem(), 0));

    m_lineInfo.setEmpty(false);
    m_ignoringSpaces = false;
    m_currentCharacterIsSpace = false;
    m_trailingObjects.clear();

    // Optimize for a common case. If we can't find whitespace after the list
    // item, then this is all moot.
    LayoutUnit replacedLogicalWidth = m_block.logicalWidthForChild(replacedBox) + m_block.marginStartForChild(replacedBox) + m_block.marginEndForChild(replacedBox) + inlineLogicalWidthFromAncestorsIfNeeded(m_current.getLineLayoutItem());
    if (m_current.getLineLayoutItem().isListMarker()) {
        if (m_blockStyle->collapseWhiteSpace() && shouldSkipWhitespaceAfterStartObject(m_block, m_current.getLineLayoutItem(), m_lineMidpointState)) {
            // Like with inline flows, we start ignoring spaces to make sure that any
            // additional spaces we see will be discarded.
            m_currentCharacterIsSpace = true;
            m_ignoringSpaces = true;
        }
        if (LineLayoutListMarker(m_current.getLineLayoutItem()).isInside())
            m_width.addUncommittedWidth(replacedLogicalWidth.toFloat());
    } else {
        m_width.addUncommittedWidth(replacedLogicalWidth.toFloat());
    }
    if (m_current.getLineLayoutItem().isRubyRun())
        m_width.applyOverhang(LineLayoutRubyRun(m_current.getLineLayoutItem()), m_lastObject, m_nextObject);
    // Update prior line break context characters, using U+FFFD (OBJECT REPLACEMENT CHARACTER) for replaced element.
    m_layoutTextInfo.m_lineBreakIterator.updatePriorContext(replacementCharacter);
}

inline void nextCharacter(UChar& currentCharacter, UChar& lastCharacter, UChar& secondToLastCharacter)
{
    secondToLastCharacter = lastCharacter;
    lastCharacter = currentCharacter;
}

inline float firstPositiveWidth(const WordMeasurements& wordMeasurements)
{
    for (size_t i = 0; i < wordMeasurements.size(); ++i) {
        if (wordMeasurements[i].width > 0)
            return wordMeasurements[i].width;
    }
    return 0;
}

ALWAYS_INLINE TextDirection textDirectionFromUnicode(WTF::Unicode::CharDirection direction)
{
    return direction == WTF::Unicode::RightToLeft
        || direction == WTF::Unicode::RightToLeftArabic ? RTL : LTR;
}

ALWAYS_INLINE float textWidth(LineLayoutText text, unsigned from, unsigned len, const Font& font, float xPos, bool collapseWhiteSpace, HashSet<const SimpleFontData*>* fallbackFonts = nullptr, FloatRect* glyphBounds = nullptr)
{
    if ((!from && len == text.textLength()) || text.style()->hasTextCombine())
        return text.width(from, len, font, LayoutUnit(xPos), text.style()->direction(), fallbackFonts, glyphBounds);

    TextRun run = constructTextRun(font, text, from, len, text.styleRef());
    run.setTabSize(!collapseWhiteSpace, text.style()->getTabSize());
    run.setXPos(xPos);
    return font.width(run, fallbackFonts, glyphBounds);
}

ALWAYS_INLINE bool BreakingContext::shouldMidWordBreak(
    UChar c, LineLayoutText layoutText, const Font& font,
    float& charWidth, float& widthFromLastBreakingOpportunity,
    bool breakAll, int& nextBreakablePositionForBreakAll)
{
    // For breakWords/breakAll, we need to measure up to normal break
    // opportunity and then rewindToMidWordBreak() because ligatures/kerning can
    // shorten the width as we add more characters.
    // However, doing so can hit the performance when a "word" is really long,
    // such as minimized JS, because the next line will re-shape the rest of the
    // word in the current architecture.
    // This function is a heuristic optimization to stop at 2em overflow.
    float overflowAllowance = 2 * font.getFontDescription().computedSize();

    widthFromLastBreakingOpportunity += charWidth;
    bool midWordBreakIsBeforeSurrogatePair = U16_IS_LEAD(c) && m_current.offset() + 1 < layoutText.textLength() && U16_IS_TRAIL(layoutText.uncheckedCharacterAt(m_current.offset() + 1));
    charWidth = textWidth(layoutText, m_current.offset(), midWordBreakIsBeforeSurrogatePair ? 2 : 1, font, m_width.committedWidth() + widthFromLastBreakingOpportunity, m_collapseWhiteSpace);
    if (m_width.committedWidth() + widthFromLastBreakingOpportunity + charWidth <= m_width.availableWidth() + overflowAllowance)
        return false;

    // breakAll has different break opportunities. Ensure we break only at
    // breakAll allows to break.
    if (breakAll &&
        !m_layoutTextInfo.m_lineBreakIterator.isBreakable(m_current.offset(), nextBreakablePositionForBreakAll, LineBreakType::BreakAll)) {
        return false;
    }

    return true;
}

ALWAYS_INLINE int lastBreakablePositionForBreakAll(LineLayoutText text,
    const ComputedStyle& style, int start, int end)
{
    LazyLineBreakIterator lineBreakIterator(text.text(), style.locale());
    int lastBreakablePosition = 0, nextBreakablePosition = -1;
    for (int i = start; ;i = nextBreakablePosition + 1) {
        lineBreakIterator.isBreakable(i, nextBreakablePosition, LineBreakType::BreakAll);
        if (nextBreakablePosition == end)
            return end;
        if (nextBreakablePosition < 0 || nextBreakablePosition > end)
            return lastBreakablePosition;
        lastBreakablePosition = nextBreakablePosition;
    }
}

ALWAYS_INLINE bool BreakingContext::rewindToMidWordBreak(
    WordMeasurement& wordMeasurement, int end, float width)
{
    wordMeasurement.endOffset = end;
    wordMeasurement.width = width;

    m_current.moveTo(m_current.getLineLayoutItem(), end, m_current.nextBreakablePosition());
    m_lineBreak.moveTo(m_current.getLineLayoutItem(), end, m_current.nextBreakablePosition());
    return true;
}

ALWAYS_INLINE bool BreakingContext::rewindToFirstMidWordBreak(LineLayoutText text,
    const ComputedStyle& style, const Font& font, bool breakAll,
    WordMeasurement& wordMeasurement)
{
    int start = wordMeasurement.startOffset;
    int end;
    if (breakAll) {
        LazyLineBreakIterator lineBreakIterator(text.text(), style.locale());
        end = -1;
        lineBreakIterator.isBreakable(start + 1, end, LineBreakType::BreakAll);
        if (end < 0)
            return false;
    } else {
        end = start + 1;
    }
    if (end >= wordMeasurement.endOffset)
        return false;

    float width = textWidth(text, start, end - start, font, m_width.currentWidth(), m_collapseWhiteSpace);
    return rewindToMidWordBreak(wordMeasurement, end, width);
}

ALWAYS_INLINE bool BreakingContext::rewindToMidWordBreak(LineLayoutText text,
    const ComputedStyle& style, const Font& font, bool breakAll,
    WordMeasurement& wordMeasurement)
{
    int start = wordMeasurement.startOffset;
    int len = wordMeasurement.endOffset - start;
    if (!len)
        return false;
    if (m_width.availableWidth() <= LayoutUnit::epsilon())
        return rewindToFirstMidWordBreak(text, style, font, breakAll, wordMeasurement);

    TextRun run = constructTextRun(font, text, start, len, style);
    run.setTabSize(!m_collapseWhiteSpace, style.getTabSize());
    run.setXPos(m_width.currentWidth());

    // TODO(kojii): should be replaced with safe-to-break when hb is ready.
    float x = m_width.availableWidth() + LayoutUnit::epsilon() - m_width.currentWidth();
    if (run.rtl())
        x = wordMeasurement.width - x;
    len = font.offsetForPosition(run, x, false);
    if (!len && !m_width.currentWidth())
        return rewindToFirstMidWordBreak(text, style, font, breakAll, wordMeasurement);

    int end = start + len;
    if (breakAll) {
        end = lastBreakablePositionForBreakAll(text, style, start, end);
        if (!end)
            return false;
        len = end - start;
    }
    FloatRect rect = font.selectionRectForText(run, FloatPoint(), 0, 0, len);
    DCHECK(m_width.fitsOnLine(rect.width() - 1)); // avoid failure when rect is rounded up.
    return rewindToMidWordBreak(wordMeasurement, end, rect.width());
}

ALWAYS_INLINE bool BreakingContext::hyphenate(LineLayoutText text,
    const ComputedStyle& style, const Font& font,
    const Hyphenation& hyphenation, float lastSpaceWordSpacing,
    WordMeasurement& wordMeasurement)
{
    unsigned start = wordMeasurement.startOffset;
    unsigned len = wordMeasurement.endOffset - start;
    if (len <= Hyphenation::minimumSuffixLength)
        return false;

    float hyphenWidth = text.hyphenWidth(font, textDirectionFromUnicode(m_resolver.position().direction()));
    float maxPrefixWidth = m_width.availableWidth()
        - m_width.currentWidth() - hyphenWidth - lastSpaceWordSpacing;

    if (maxPrefixWidth <= Hyphenation::minimumPrefixWidth(font))
        return false;

    TextRun run = constructTextRun(font, text, start, len, style);
    run.setTabSize(!m_collapseWhiteSpace, style.getTabSize());
    run.setXPos(m_width.currentWidth());
    unsigned maxPrefixLength = font.offsetForPosition(run, maxPrefixWidth, false);
    if (maxPrefixLength < Hyphenation::minimumPrefixLength)
        return false;

    unsigned prefixLength = hyphenation.lastHyphenLocation(
        StringView(text.text(), start, len),
        std::min(maxPrefixLength, len - Hyphenation::minimumSuffixLength) + 1);
    if (!prefixLength || prefixLength < Hyphenation::minimumPrefixLength)
        return false;

    // TODO(kojii): getCharacterRange() measures as if the word were not broken
    // as defined in the spec, and is faster than measuring each fragment, but
    // ignores the kerning between the last letter and the hyphen.
    return rewindToMidWordBreak(wordMeasurement, start + prefixLength,
        font.getCharacterRange(run, 0, prefixLength).width() + hyphenWidth);
}

inline bool BreakingContext::handleText(WordMeasurements& wordMeasurements, bool& hyphenated)
{
    if (!m_current.offset())
        m_appliedStartWidth = false;

    LineLayoutText layoutText(m_current.getLineLayoutItem());

    // If we have left a no-wrap inline and entered an autowrap inline while ignoring spaces
    // then we need to mark the start of the autowrap inline as a potential linebreak now.
    if (m_autoWrap && !ComputedStyle::autoWrap(m_lastWS) && m_ignoringSpaces) {
        m_width.commit();
        m_lineBreak.moveToStartOf(m_current.getLineLayoutItem());
    }

    const ComputedStyle& style = layoutText.styleRef(m_lineInfo.isFirstLine());
    const Font& font = style.font();

    unsigned lastSpace = m_current.offset();
    float wordSpacing = m_currentStyle->wordSpacing();
    float lastSpaceWordSpacing = 0;
    float wordSpacingForWordMeasurement = 0;

    float widthFromLastBreakingOpportunity = m_width.uncommittedWidth();
    float charWidth = 0;
    // Auto-wrapping text should wrap in the middle of a word only if it could not wrap before the word,
    // which is only possible if the word is the first thing on the line, that is, if |w| is zero.
    bool breakWords = m_currentStyle->breakWords() && ((m_autoWrap && !m_width.committedWidth()) || m_currWS == PRE);
    bool midWordBreak = false;
    bool breakAll = m_currentStyle->wordBreak() == BreakAllWordBreak && m_autoWrap;
    bool keepAll = m_currentStyle->wordBreak() == KeepAllWordBreak && m_autoWrap;
    bool prohibitBreakInside = m_currentStyle->hasTextCombine() && layoutText.isCombineText() && LineLayoutTextCombine(layoutText).isCombined();

    // This is currently only used for word-break: break-all, specifically for the case
    // where we have a break opportunity within a word, then a string of non-breakable
    // content that ends up making our word wider than the current line.
    // See: fast/css3-text/css3-word-break/word-break-all-wrap-with-floats.html
    float widthMeasurementAtLastBreakOpportunity = 0;

    Hyphenation* hyphenation = style.getHyphens() == HyphensAuto
        ? Hyphenation::get(font.getFontDescription().locale()) : nullptr;
    bool disableSoftHyphen = style.getHyphens() == HyphensNone;
    float hyphenWidth = 0;

    if (layoutText.isSVGInlineText()) {
        breakWords = false;
        breakAll = false;
        keepAll = false;
    }

    // Use LineBreakType::Normal for break-all. When a word does not fit,
    // rewindToMidWordBreak() finds the mid-word break point.
    LineBreakType lineBreakType = keepAll ? LineBreakType::KeepAll : LineBreakType::Normal;
    bool canBreakMidWord = breakAll || breakWords;
    int nextBreakablePositionForBreakAll = -1;

    if (layoutText.isWordBreak()) {
        m_width.commit();
        m_lineBreak.moveToStartOf(m_current.getLineLayoutItem());
        ASSERT(m_current.offset() == layoutText.textLength());
    }

    if (m_layoutTextInfo.m_text != layoutText) {
        m_layoutTextInfo.m_text = layoutText;
        m_layoutTextInfo.m_font = &font;
        m_layoutTextInfo.m_lineBreakIterator.resetStringAndReleaseIterator(layoutText.text(), style.locale());
    } else if (m_layoutTextInfo.m_font != &font) {
        m_layoutTextInfo.m_font = &font;
    }

    // Non-zero only when kerning is enabled, in which case we measure
    // words with their trailing space, then subtract its width.
    float wordTrailingSpaceWidth = (font.getFontDescription().getTypesettingFeatures() & Kerning) ?
        font.width(constructTextRun(font, &spaceCharacter, 1, style, style.direction())) + wordSpacing
        : 0;

    UChar lastCharacter = m_layoutTextInfo.m_lineBreakIterator.lastCharacter();
    UChar secondToLastCharacter = m_layoutTextInfo.m_lineBreakIterator.secondToLastCharacter();
    for (; m_current.offset() < layoutText.textLength(); m_current.fastIncrementInTextNode()) {
        bool previousCharacterIsSpace = m_currentCharacterIsSpace;
        UChar c = m_current.current();
        m_currentCharacterIsSpace = c == spaceCharacter || c == tabulationCharacter || (!m_preservesNewline && (c == newlineCharacter));

        if (!m_collapseWhiteSpace || !m_currentCharacterIsSpace) {
            m_lineInfo.setEmpty(false);
            m_width.setTrailingWhitespaceWidth(0);
        }

        if (c == softHyphenCharacter && m_autoWrap && !hyphenWidth && !disableSoftHyphen) {
            hyphenWidth = layoutText.hyphenWidth(font, textDirectionFromUnicode(m_resolver.position().direction()));
            m_width.addUncommittedWidth(hyphenWidth);
        }

        bool applyWordSpacing = false;

        // Determine if we should try breaking in the middle of a word.
        if (canBreakMidWord && !midWordBreak && !U16_IS_TRAIL(c))
            midWordBreak = shouldMidWordBreak(c, layoutText, font, charWidth, widthFromLastBreakingOpportunity, breakAll, nextBreakablePositionForBreakAll);

        // Determine if we are in the whitespace between words.
        int nextBreakablePosition = m_current.nextBreakablePosition();
        bool betweenWords = c == newlineCharacter || (m_currWS != PRE && !m_atStart && m_layoutTextInfo.m_lineBreakIterator.isBreakable(m_current.offset(), nextBreakablePosition, lineBreakType)
            && (!disableSoftHyphen || m_current.previousInSameNode() != softHyphenCharacter));
        m_current.setNextBreakablePosition(nextBreakablePosition);

        // If we're in the middle of a word or at the start of a new one and can't break there, then continue to the next character.
        if (!betweenWords && !midWordBreak) {
            if (m_ignoringSpaces) {
                // Stop ignoring spaces and begin at this
                // new point.
                lastSpaceWordSpacing = applyWordSpacing ? wordSpacing : 0;
                wordSpacingForWordMeasurement = (applyWordSpacing && wordMeasurements.last().width) ? wordSpacing : 0;
                stopIgnoringSpaces(lastSpace);
            }

            prepareForNextCharacter(layoutText, prohibitBreakInside, previousCharacterIsSpace);
            m_atStart = false;
            nextCharacter(c, lastCharacter, secondToLastCharacter);
            continue;
        }

        // If we're collapsing space and we're at a collapsible space such as a space or tab, continue to the next character.
        if (m_ignoringSpaces && m_currentCharacterIsSpace) {
            lastSpaceWordSpacing = 0;
            // Just keep ignoring these spaces.
            nextCharacter(c, lastCharacter, secondToLastCharacter);
            continue;
        }

        // We're in the first whitespace after a word or in whitespace that we don't collapse, which means we may have a breaking opportunity here.

        // If we're here and we're collapsing space then the current character isn't a form of whitespace we can collapse. Stop ignoring spaces.
        bool stoppedIgnoringSpaces = false;
        if (m_ignoringSpaces) {
            lastSpaceWordSpacing = 0;
            wordSpacingForWordMeasurement = 0;
            stoppedIgnoringSpaces = true;
            stopIgnoringSpaces(lastSpace);
        }

        // Update our tally of the width since the last breakable position with the width of the word we're now at the end of.
        float lastWidthMeasurement;
        WordMeasurement& wordMeasurement = calculateWordWidth(wordMeasurements, layoutText, lastSpace, lastWidthMeasurement, wordSpacingForWordMeasurement, font, wordTrailingSpaceWidth, c);
        lastWidthMeasurement += lastSpaceWordSpacing;
        m_width.addUncommittedWidth(lastWidthMeasurement);

        // We keep track of the total width contributed by trailing space as we often want to exclude it when determining
        // if a run fits on a line.
        if (m_collapseWhiteSpace && previousCharacterIsSpace && m_currentCharacterIsSpace && lastWidthMeasurement)
            m_width.setTrailingWhitespaceWidth(lastWidthMeasurement);

        // If this is the end of the first word in run of text then make sure we apply the width from any leading inlines.
        // For example: '<span style="margin-left: 5px;"><span style="margin-left: 10px;">FirstWord</span></span>' would
        // apply a width of 15px from the two span ancestors.
        if (!m_appliedStartWidth) {
            m_width.addUncommittedWidth(inlineLogicalWidthFromAncestorsIfNeeded(m_current.getLineLayoutItem(), true, false).toFloat());
            m_appliedStartWidth = true;
        }

        midWordBreak = false;
        if (!m_width.fitsOnLine()) {
            if (canBreakMidWord) {
                m_width.addUncommittedWidth(-wordMeasurement.width);
                if (rewindToMidWordBreak(layoutText, style, font, breakAll, wordMeasurement)) {
                    lastWidthMeasurement = wordMeasurement.width + lastSpaceWordSpacing;
                    midWordBreak = true;
                }
                m_width.addUncommittedWidth(wordMeasurement.width);
            } else if (hyphenation) {
                m_width.addUncommittedWidth(-wordMeasurement.width);
                DCHECK(lastSpace == static_cast<unsigned>(wordMeasurement.startOffset));
                DCHECK(m_current.offset() == static_cast<unsigned>(wordMeasurement.endOffset));
                if (hyphenate(layoutText, style, font, *hyphenation, lastSpaceWordSpacing, wordMeasurement)) {
                    m_width.addUncommittedWidth(wordMeasurement.width);
                    hyphenated = true;
                    m_atEnd = true;
                    return false;
                }
                m_width.addUncommittedWidth(wordMeasurement.width);
            }
        }

        // If we haven't hit a breakable position yet and already don't fit on the line try to move below any floats.
        if (!m_width.committedWidth() && m_autoWrap && !m_width.fitsOnLine() && !widthMeasurementAtLastBreakOpportunity) {
            float availableWidthBefore = m_width.availableWidth();
            m_width.fitBelowFloats(m_lineInfo.isFirstLine());
            // If availableWidth changes by moving the line below floats, needs to measure midWordBreak again.
            if (midWordBreak && availableWidthBefore != m_width.availableWidth())
                midWordBreak = false;
        }

        // If there is a soft-break available at this whitespace position then take it.
        applyWordSpacing = wordSpacing && m_currentCharacterIsSpace;
        if (canBreakAtWhitespace(breakWords, wordMeasurement, stoppedIgnoringSpaces, charWidth, hyphenated, disableSoftHyphen, hyphenWidth, betweenWords, midWordBreak, canBreakMidWord, previousCharacterIsSpace, lastWidthMeasurement, layoutText, font, applyWordSpacing, wordSpacing))
            return false;

        // If there is a hard-break available at this whitespace position then take it.
        if (c == newlineCharacter && m_preservesNewline) {
            if (!stoppedIgnoringSpaces && m_current.offset())
                m_lineMidpointState.ensureCharacterGetsLineBox(m_current);
            m_lineBreak.moveTo(m_current.getLineLayoutItem(), m_current.offset(), m_current.nextBreakablePosition());
            m_lineBreak.increment();
            m_lineInfo.setPreviousLineBrokeCleanly(true);
            return true;
        }

        // Auto-wrapping text should not wrap in the middle of a word once it has had an
        // opportunity to break after a word.
        if (m_autoWrap && betweenWords) {
            m_width.commit();
            widthFromLastBreakingOpportunity = 0;
            m_lineBreak.moveTo(m_current.getLineLayoutItem(), m_current.offset(), m_current.nextBreakablePosition());
            breakWords = false;
            canBreakMidWord = breakAll;
            widthMeasurementAtLastBreakOpportunity = lastWidthMeasurement;
        }

        if (betweenWords) {
            lastSpaceWordSpacing = applyWordSpacing ? wordSpacing : 0;
            wordSpacingForWordMeasurement = (applyWordSpacing && wordMeasurement.width) ? wordSpacing : 0;
            lastSpace = m_current.offset();
        }

        // If we encounter a newline, or if we encounter a second space, we need to go ahead and break up
        // this run and enter a mode where we start collapsing spaces.
        if (!m_ignoringSpaces && m_currentStyle->collapseWhiteSpace()) {
            if (m_currentCharacterIsSpace && previousCharacterIsSpace) {
                m_ignoringSpaces = true;

                // We just entered a mode where we are ignoring spaces. Create a midpoint to terminate the run
                // before the second space.
                m_lineMidpointState.startIgnoringSpaces(m_startOfIgnoredSpaces);
                m_trailingObjects.updateMidpointsForTrailingObjects(m_lineMidpointState, InlineIterator(), TrailingObjects::DoNotCollapseFirstSpace);
            }
        }

        prepareForNextCharacter(layoutText, prohibitBreakInside, previousCharacterIsSpace);
        m_atStart = false;
        nextCharacter(c, lastCharacter, secondToLastCharacter);
    }

    m_layoutTextInfo.m_lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);

    wordMeasurements.grow(wordMeasurements.size() + 1);
    WordMeasurement& wordMeasurement = wordMeasurements.last();
    wordMeasurement.layoutText = layoutText;

    // IMPORTANT: current.offset() is > layoutText.textLength() here!
    float lastWidthMeasurement = 0;
    wordMeasurement.startOffset = lastSpace;
    wordMeasurement.endOffset = m_current.offset();
    midWordBreak = false;
    if (!m_ignoringSpaces) {
        lastWidthMeasurement = textWidth(layoutText, lastSpace, m_current.offset() - lastSpace, font, m_width.currentWidth(), m_collapseWhiteSpace, &wordMeasurement.fallbackFonts, &wordMeasurement.glyphBounds);
        wordMeasurement.width = lastWidthMeasurement + wordSpacingForWordMeasurement;
        wordMeasurement.glyphBounds.move(wordSpacingForWordMeasurement, 0);

        if (canBreakMidWord && !m_width.fitsOnLine(lastWidthMeasurement)
            && rewindToMidWordBreak(layoutText, style, font, breakAll, wordMeasurement)) {
            lastWidthMeasurement = wordMeasurement.width;
            midWordBreak = true;
        }
    }
    lastWidthMeasurement += lastSpaceWordSpacing;

    LayoutUnit additionalWidthFromAncestors = inlineLogicalWidthFromAncestorsIfNeeded(m_current.getLineLayoutItem(), !m_appliedStartWidth, m_includeEndWidth);
    m_width.addUncommittedWidth(lastWidthMeasurement + additionalWidthFromAncestors);

    if (m_collapseWhiteSpace && m_currentCharacterIsSpace && lastWidthMeasurement)
        m_width.setTrailingWhitespaceWidth(lastWidthMeasurement + additionalWidthFromAncestors);

    m_includeEndWidth = false;

    if (midWordBreak) {
        m_width.commit();
        m_atEnd = true;
    } else if (!m_width.fitsOnLine()) {
        if (hyphenation && (m_nextObject || m_lineInfo.isEmpty())) {
            m_width.addUncommittedWidth(-wordMeasurement.width);
            DCHECK(lastSpace == static_cast<unsigned>(wordMeasurement.startOffset));
            DCHECK(m_current.offset() == static_cast<unsigned>(wordMeasurement.endOffset));
            if (hyphenate(layoutText, style, font, *hyphenation, lastSpaceWordSpacing, wordMeasurement)) {
                hyphenated = true;
                m_atEnd = true;
            }
            m_width.addUncommittedWidth(wordMeasurement.width);
        }
        if (!hyphenated
            && m_lineBreak.previousInSameNode() == softHyphenCharacter
            && !disableSoftHyphen) {
            hyphenated = true;
            m_atEnd = true;
        }
    }
    return false;
}

inline void BreakingContext::prepareForNextCharacter(const LineLayoutText& layoutText, bool& prohibitBreakInside, bool previousCharacterIsSpace)
{
    if (layoutText.isSVGInlineText() && m_current.offset()) {
        // Force creation of new InlineBoxes for each absolute positioned character (those that start new text chunks).
        if (LineLayoutSVGInlineText(layoutText).characterStartsNewTextChunk(m_current.offset()))
            m_lineMidpointState.ensureCharacterGetsLineBox(m_current);
    }
    if (prohibitBreakInside) {
        m_current.setNextBreakablePosition(layoutText.textLength());
        prohibitBreakInside = false;
    }
    if (m_currentCharacterIsSpace && !previousCharacterIsSpace) {
        m_startOfIgnoredSpaces.setLineLayoutItem(m_current.getLineLayoutItem());
        m_startOfIgnoredSpaces.setOffset(m_current.offset());
    }
    if (!m_currentCharacterIsSpace && previousCharacterIsSpace) {
        if (m_autoWrap && m_currentStyle->breakOnlyAfterWhiteSpace())
            m_lineBreak.moveTo(m_current.getLineLayoutItem(), m_current.offset(), m_current.nextBreakablePosition());
    }
    if (m_collapseWhiteSpace && m_currentCharacterIsSpace && !m_ignoringSpaces)
        m_trailingObjects.setTrailingWhitespace(LineLayoutText(m_current.getLineLayoutItem()));
    else if (!m_currentStyle->collapseWhiteSpace() || !m_currentCharacterIsSpace)
        m_trailingObjects.clear();
}


inline void BreakingContext::stopIgnoringSpaces(unsigned& lastSpace)
{
    m_ignoringSpaces = false;
    lastSpace = m_current.offset(); // e.g., "Foo    goo", don't add in any of the ignored spaces.
    m_lineMidpointState.stopIgnoringSpaces(InlineIterator(0, m_current.getLineLayoutItem(), m_current.offset()));
}

inline WordMeasurement& BreakingContext::calculateWordWidth(WordMeasurements& wordMeasurements, LineLayoutText& layoutText, unsigned lastSpace, float& lastWidthMeasurement, float wordSpacingForWordMeasurement, const Font& font, float wordTrailingSpaceWidth, UChar c)
{
    wordMeasurements.grow(wordMeasurements.size() + 1);
    WordMeasurement& wordMeasurement = wordMeasurements.last();
    wordMeasurement.layoutText = layoutText;
    wordMeasurement.endOffset = m_current.offset();
    wordMeasurement.startOffset = lastSpace;

    if (wordTrailingSpaceWidth && c == spaceCharacter)
        lastWidthMeasurement = textWidth(layoutText, lastSpace, m_current.offset() + 1 - lastSpace, font, m_width.currentWidth(), m_collapseWhiteSpace, &wordMeasurement.fallbackFonts, &wordMeasurement.glyphBounds) - wordTrailingSpaceWidth;
    else
        lastWidthMeasurement = textWidth(layoutText, lastSpace, m_current.offset() - lastSpace, font, m_width.currentWidth(), m_collapseWhiteSpace, &wordMeasurement.fallbackFonts, &wordMeasurement.glyphBounds);

    wordMeasurement.width = lastWidthMeasurement + wordSpacingForWordMeasurement;
    wordMeasurement.glyphBounds.move(wordSpacingForWordMeasurement, 0);
    return wordMeasurement;
}

inline bool BreakingContext::trailingSpaceExceedsAvailableWidth(bool canBreakMidWord, const LineLayoutText& layoutText, WordMeasurement& wordMeasurement, bool applyWordSpacing, bool wordSpacing, const Font& font)
{
    // If we break only after white-space, consider the current character
    // as candidate width for this line.
    if (m_width.fitsOnLine() && m_currentCharacterIsSpace && m_currentStyle->breakOnlyAfterWhiteSpace() && !canBreakMidWord) {
        float charWidth = textWidth(layoutText, m_current.offset(), 1, font, m_width.currentWidth(), m_collapseWhiteSpace, &wordMeasurement.fallbackFonts, &wordMeasurement.glyphBounds) + (applyWordSpacing ? wordSpacing : 0);
        // Check if line is too big even without the extra space
        // at the end of the line. If it is not, do nothing.
        // If the line needs the extra whitespace to be too long,
        // then move the line break to the space and skip all
        // additional whitespace.
        if (!m_width.fitsOnLine(charWidth)) {
            m_lineBreak.moveTo(m_current.getLineLayoutItem(), m_current.offset(), m_current.nextBreakablePosition());
            skipTrailingWhitespace(m_lineBreak, m_lineInfo);
            return true;
        }
    }
    return false;
}

inline bool BreakingContext::canBreakAtWhitespace(bool breakWords, WordMeasurement& wordMeasurement, bool stoppedIgnoringSpaces, float charWidth, bool& hyphenated, bool disableSoftHyphen, float& hyphenWidth, bool betweenWords, bool midWordBreak, bool canBreakMidWord, bool previousCharacterIsSpace, float lastWidthMeasurement, const LineLayoutText& layoutText, const Font& font, bool applyWordSpacing, float wordSpacing)
{
    if (!m_autoWrap && !breakWords)
        return false;

    // If we break only after white-space, consider the current character
    // as candidate width for this line.
    if (midWordBreak
        || trailingSpaceExceedsAvailableWidth(canBreakMidWord, layoutText, wordMeasurement, applyWordSpacing, wordSpacing, font)
        || !m_width.fitsOnLine()) {
        if (m_lineBreak.atTextParagraphSeparator()) {
            if (!stoppedIgnoringSpaces && m_current.offset() > 0)
                m_lineMidpointState.ensureCharacterGetsLineBox(m_current);
            m_lineBreak.increment();
            m_lineInfo.setPreviousLineBrokeCleanly(true);
            wordMeasurement.endOffset = m_lineBreak.offset();
        }
        if (m_lineBreak.getLineLayoutItem() && m_lineBreak.offset() && m_lineBreak.getLineLayoutItem().isText() && LineLayoutText(m_lineBreak.getLineLayoutItem()).textLength() && LineLayoutText(m_lineBreak.getLineLayoutItem()).characterAt(m_lineBreak.offset() - 1) == softHyphenCharacter && !disableSoftHyphen)
            hyphenated = true;
        if (m_lineBreak.offset() && m_lineBreak.offset() != (unsigned)wordMeasurement.endOffset && !wordMeasurement.width) {
            if (charWidth) {
                wordMeasurement.endOffset = m_lineBreak.offset();
                wordMeasurement.width = charWidth;
            }
        }
        // Didn't fit. Jump to the end unless there's still an opportunity to collapse whitespace.
        if (m_ignoringSpaces || !m_collapseWhiteSpace || !m_currentCharacterIsSpace || !previousCharacterIsSpace) {
            m_atEnd = true;
            return true;
        }
    } else {
        if (!betweenWords || (midWordBreak && !m_autoWrap))
            m_width.addUncommittedWidth(-lastWidthMeasurement);
        if (hyphenWidth) {
            // Subtract the width of the soft hyphen out since we fit on a line.
            m_width.addUncommittedWidth(-hyphenWidth);
            hyphenWidth = 0;
        }
    }
    return false;
}

inline void BreakingContext::commitAndUpdateLineBreakIfNeeded()
{
    bool checkForBreak = m_autoWrap;
    if (m_width.committedWidth() && !m_width.fitsOnLine() && m_lineBreak.getLineLayoutItem() && m_currWS == NOWRAP) {
        if (m_width.fitsOnLine(0, ExcludeWhitespace)) {
            m_width.commit();
            m_lineBreak.moveToStartOf(m_nextObject);
        }
        checkForBreak = true;
    } else if (m_nextObject && m_current.getLineLayoutItem().isText() && m_nextObject.isText() && !m_nextObject.isBR() && (m_autoWrap || m_nextObject.style()->autoWrap())) {
        if (m_autoWrap && m_currentCharacterIsSpace) {
            checkForBreak = true;
        } else {
            LineLayoutText nextText(m_nextObject);
            if (nextText.textLength()) {
                UChar c = nextText.characterAt(0);
                // If the next item on the line is text, and if we did not end with
                // a space, then the next text run continues our word (and so it needs to
                // keep adding to the uncommitted width. Just update and continue.
                checkForBreak = !m_currentCharacterIsSpace && (c == spaceCharacter || c == tabulationCharacter || (c == newlineCharacter && !m_nextObject.preservesNewline()));
            } else if (nextText.isWordBreak()) {
                checkForBreak = true;
            }

            if (!m_width.fitsOnLine() && !m_width.committedWidth())
                m_width.fitBelowFloats(m_lineInfo.isFirstLine());

            bool canPlaceOnLine = m_width.fitsOnLine() || !m_autoWrapWasEverTrueOnLine;
            if (canPlaceOnLine && checkForBreak) {
                m_width.commit();
                m_lineBreak.moveToStartOf(m_nextObject);
            }
        }
    }

    ASSERT_WITH_SECURITY_IMPLICATION(m_currentStyle->refCount() > 0);
    if (checkForBreak && !m_width.fitsOnLine()) {
        // if we have floats, try to get below them.
        if (m_currentCharacterIsSpace && !m_ignoringSpaces && m_currentStyle->collapseWhiteSpace())
            m_trailingObjects.clear();

        if (m_width.committedWidth()) {
            m_atEnd = true;
            return;
        }

        m_width.fitBelowFloats(m_lineInfo.isFirstLine());

        // |width| may have been adjusted because we got shoved down past a float (thus
        // giving us more room), so we need to retest, and only jump to
        // the end label if we still don't fit on the line. -dwh
        if (!m_width.fitsOnLine()) {
            m_atEnd = true;
            return;
        }
    } else if (m_blockStyle->autoWrap() && !m_width.fitsOnLine() && !m_width.committedWidth()) {
        // If the container autowraps but the current child does not then we still need to ensure that it
        // wraps and moves below any floats.
        m_width.fitBelowFloats(m_lineInfo.isFirstLine());
    }

    if (!m_current.getLineLayoutItem().isFloatingOrOutOfFlowPositioned()) {
        m_lastObject = m_current.getLineLayoutItem();
        if (m_lastObject.isAtomicInlineLevel() && m_autoWrap && (!m_lastObject.isImage() || m_allowImagesToBreak) && (!m_lastObject.isListMarker() || LineLayoutListMarker(m_lastObject).isInside())
            && !m_lastObject.isRubyRun()) {
            m_width.commit();
            m_lineBreak.moveToStartOf(m_nextObject);
        }
    }
}

inline IndentTextOrNot requiresIndent(bool isFirstLine, bool isAfterHardLineBreak, const ComputedStyle& style)
{
    IndentTextOrNot indentText = DoNotIndentText;
    if (isFirstLine || (isAfterHardLineBreak && style.getTextIndentLine()) == TextIndentEachLine)
        indentText = IndentText;

    if (style.getTextIndentType() == TextIndentHanging)
        indentText = indentText == IndentText ? DoNotIndentText : IndentText;

    return indentText;
}

} // namespace blink

#endif // BreakingContextInlineHeaders_h
