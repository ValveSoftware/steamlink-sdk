/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "core/layout/line/InlineTextBox.h"

#include "core/layout/HitTestResult.h"
#include "core/layout/api/LineLayoutBR.h"
#include "core/layout/api/LineLayoutBox.h"
#include "core/layout/api/LineLayoutRubyRun.h"
#include "core/layout/api/LineLayoutRubyText.h"
#include "core/layout/line/AbstractInlineTextBox.h"
#include "core/layout/line/EllipsisBox.h"
#include "core/paint/InlineTextBoxPainter.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/shaping/SimpleShaper.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"

#include <algorithm>

namespace blink {

struct SameSizeAsInlineTextBox : public InlineBox {
    unsigned variables[1];
    unsigned short variables2[2];
    void* pointers[2];
};

static_assert(sizeof(InlineTextBox) == sizeof(SameSizeAsInlineTextBox), "InlineTextBox should stay small");

typedef WTF::HashMap<const InlineTextBox*, LayoutRect> InlineTextBoxOverflowMap;
static InlineTextBoxOverflowMap* gTextBoxesWithOverflow;

void InlineTextBox::destroy()
{
    AbstractInlineTextBox::willDestroy(this);

    if (!knownToHaveNoOverflow() && gTextBoxesWithOverflow)
        gTextBoxesWithOverflow->remove(this);
    InlineTextBoxPainter::removeFromTextBlobCache(*this);
    InlineBox::destroy();
}

void InlineTextBox::offsetRun(int delta)
{
    ASSERT(!isDirty());
    InlineTextBoxPainter::removeFromTextBlobCache(*this);
    m_start += delta;
}

void InlineTextBox::markDirty()
{
    // FIXME: Is it actually possible to try and paint a dirty InlineTextBox?
    InlineTextBoxPainter::removeFromTextBlobCache(*this);

    m_len = 0;
    m_start = 0;
    InlineBox::markDirty();
}

LayoutRect InlineTextBox::logicalOverflowRect() const
{
    if (knownToHaveNoOverflow() || !gTextBoxesWithOverflow)
        return logicalFrameRect();

    const auto& it = gTextBoxesWithOverflow->find(this);
    if (it != gTextBoxesWithOverflow->end())
        return it->value;

    return logicalFrameRect();
}

void InlineTextBox::setLogicalOverflowRect(const LayoutRect& rect)
{
    ASSERT(!knownToHaveNoOverflow());
    DCHECK(rect != logicalFrameRect());
    if (!gTextBoxesWithOverflow)
        gTextBoxesWithOverflow = new InlineTextBoxOverflowMap;
    gTextBoxesWithOverflow->set(this, rect);
}

void InlineTextBox::move(const LayoutSize& delta)
{
    InlineBox::move(delta);

    if (!knownToHaveNoOverflow() && gTextBoxesWithOverflow) {
        const auto& it = gTextBoxesWithOverflow->find(this);
        if (it != gTextBoxesWithOverflow->end())
            it->value.move(isHorizontal() ? delta : delta.transposedSize());
    }
}

int InlineTextBox::baselinePosition(FontBaseline baselineType) const
{
    if (!isText() || !parent())
        return 0;
    if (parent()->getLineLayoutItem() == getLineLayoutItem().parent())
        return parent()->baselinePosition(baselineType);
    return LineLayoutBoxModel(getLineLayoutItem().parent()).baselinePosition(baselineType, isFirstLineStyle(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit InlineTextBox::lineHeight() const
{
    if (!isText() || !getLineLayoutItem().parent())
        return LayoutUnit();
    if (getLineLayoutItem().isBR())
        return LayoutUnit(LineLayoutBR(getLineLayoutItem()).lineHeight(isFirstLineStyle()));
    if (parent()->getLineLayoutItem() == getLineLayoutItem().parent())
        return parent()->lineHeight();
    return LineLayoutBoxModel(getLineLayoutItem().parent()).lineHeight(isFirstLineStyle(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

bool InlineTextBox::isSelected(int startPos, int endPos) const
{
    int sPos = std::max(startPos - m_start, 0);
    // The position after a hard line break is considered to be past its end.
    // See the corresponding code in InlineTextBox::getSelectionState.
    int ePos = std::min(endPos - m_start, int(m_len) + (isLineBreak() ? 0 : 1));
    return (sPos < ePos);
}

SelectionState InlineTextBox::getSelectionState() const
{
    SelectionState state = getLineLayoutItem().getSelectionState();
    if (state == SelectionStart || state == SelectionEnd || state == SelectionBoth) {
        int startPos, endPos;
        getLineLayoutItem().selectionStartEnd(startPos, endPos);
        // The position after a hard line break is considered to be past its end.
        // See the corresponding code in InlineTextBox::isSelected.
        int lastSelectable = start() + len() - (isLineBreak() ? 1 : 0);

        // FIXME: Remove -webkit-line-break: LineBreakAfterWhiteSpace.
        int endOfLineAdjustmentForCSSLineBreak = getLineLayoutItem().style()->getLineBreak() == LineBreakAfterWhiteSpace ? -1 : 0;
        bool start = (state != SelectionEnd && startPos >= m_start && startPos <= m_start + m_len + endOfLineAdjustmentForCSSLineBreak);
        bool end = (state != SelectionStart && endPos > m_start && endPos <= lastSelectable);
        if (start && end)
            state = SelectionBoth;
        else if (start)
            state = SelectionStart;
        else if (end)
            state = SelectionEnd;
        else if ((state == SelectionEnd || startPos < m_start)
            && (state == SelectionStart || endPos > lastSelectable))
            state = SelectionInside;
        else if (state == SelectionBoth)
            state = SelectionNone;
    }

    // If there are ellipsis following, make sure their selection is updated.
    if (m_truncation != cNoTruncation && root().ellipsisBox()) {
        EllipsisBox* ellipsis = root().ellipsisBox();
        if (state != SelectionNone) {
            int start, end;
            selectionStartEnd(start, end);
            // The ellipsis should be considered to be selected if the end of
            // the selection is past the beginning of the truncation and the
            // beginning of the selection is before or at the beginning of the
            // truncation.
            ellipsis->setSelectionState(end >= m_truncation && start <= m_truncation ?
                SelectionInside : SelectionNone);
        } else {
            ellipsis->setSelectionState(SelectionNone);
        }
    }

    return state;
}

bool InlineTextBox::hasWrappedSelectionNewline() const
{
    // TODO(wkorman): We shouldn't need layout at this point and it should
    // be enforced by DocumentLifecycle. http://crbug.com/537821
    // Bail out as currently looking up selection state can cause the editing
    // code can force a re-layout while scrutinizing the editing position, and
    // InlineTextBox instances are not guaranteed to survive a re-layout.
    if (getLineLayoutItem().needsLayout())
        return false;

    SelectionState state = getSelectionState();
    return (state == SelectionStart || state == SelectionInside)
        // Checking last leaf child can be slow, so we make sure to do this only
        // after the other simple conditionals.
        && (root().lastLeafChild() == this)
        // It's possible to have mixed LTR/RTL on a single line, and we only
        // want to paint a newline when we're the last leaf child and we make
        // sure there isn't a differently-directioned box following us.
        && ((!isLeftToRightDirection() && root().firstSelectedBox() == this)
            || (isLeftToRightDirection() && root().lastSelectedBox() == this));
}

float InlineTextBox::newlineSpaceWidth() const
{
    const ComputedStyle& styleToUse = getLineLayoutItem().styleRef(isFirstLineStyle());
    return styleToUse.font().spaceWidth();
}

LayoutRect InlineTextBox::localSelectionRect(int startPos, int endPos) const
{
    int sPos = std::max(startPos - m_start, 0);
    int ePos = std::min(endPos - m_start, (int)m_len);

    if (sPos > ePos)
        return LayoutRect();

    FontCachePurgePreventer fontCachePurgePreventer;

    LayoutUnit selTop = root().selectionTop();
    LayoutUnit selHeight = root().selectionHeight();
    const ComputedStyle& styleToUse = getLineLayoutItem().styleRef(isFirstLineStyle());
    const Font& font = styleToUse.font();

    StringBuilder charactersWithHyphen;
    bool respectHyphen = ePos == m_len && hasHyphen();
    TextRun textRun = constructTextRun(styleToUse, font, respectHyphen ? &charactersWithHyphen : 0);

    LayoutPoint startingPoint = LayoutPoint(logicalLeft(), selTop);
    LayoutRect r;
    if (sPos || ePos != static_cast<int>(m_len)) {
        r = LayoutRect(enclosingIntRect(font.selectionRectForText(textRun, FloatPoint(startingPoint), selHeight, sPos, ePos)));
    } else { // Avoid computing the font width when the entire line box is selected as an optimization.
        r = LayoutRect(enclosingIntRect(LayoutRect(startingPoint, LayoutSize(m_logicalWidth, selHeight))));
    }

    LayoutUnit logicalWidth = r.width();
    if (r.x() > logicalRight())
        logicalWidth = LayoutUnit();
    else if (r.maxX() > logicalRight())
        logicalWidth = logicalRight() - r.x();

    LayoutPoint topPoint;
    LayoutUnit width;
    LayoutUnit height;
    if (isHorizontal()) {
        topPoint = LayoutPoint(r.x(), selTop);
        width = logicalWidth;
        height = selHeight;
        if (hasWrappedSelectionNewline()) {
            if (!isLeftToRightDirection())
                topPoint.setX(LayoutUnit(topPoint.x() - newlineSpaceWidth()));
            width += newlineSpaceWidth();
        }
    } else {
        topPoint = LayoutPoint(selTop, r.x());
        width = selHeight;
        height = logicalWidth;
        // TODO(wkorman): RTL text embedded in top-to-bottom text can create
        // bottom-to-top situations. Add tests and ensure we handle correctly.
        if (hasWrappedSelectionNewline())
            height += newlineSpaceWidth();
    }

    return LayoutRect(topPoint, LayoutSize(width, height));
}

void InlineTextBox::deleteLine()
{
    getLineLayoutItem().removeTextBox(this);
    destroy();
}

void InlineTextBox::extractLine()
{
    if (extracted())
        return;

    getLineLayoutItem().extractTextBox(this);
}

void InlineTextBox::attachLine()
{
    if (!extracted())
        return;

    getLineLayoutItem().attachTextBox(this);
}

void InlineTextBox::setTruncation(unsigned truncation)
{
    if (truncation == m_truncation)
        return;

    m_truncation = truncation;
    InlineTextBoxPainter::removeFromTextBlobCache(*this);
}

void InlineTextBox::clearTruncation()
{
    setTruncation(cNoTruncation);
}

LayoutUnit InlineTextBox::placeEllipsisBox(bool flowIsLTR, LayoutUnit visibleLeftEdge, LayoutUnit visibleRightEdge, LayoutUnit ellipsisWidth, LayoutUnit &truncatedWidth, bool& foundBox)
{
    if (foundBox) {
        setTruncation(cFullTruncation);
        return LayoutUnit(-1);
    }

    // For LTR this is the left edge of the box, for RTL, the right edge in parent coordinates.
    LayoutUnit ellipsisX = flowIsLTR ? visibleRightEdge - ellipsisWidth : visibleLeftEdge + ellipsisWidth;

    // Criteria for full truncation:
    // LTR: the left edge of the ellipsis is to the left of our text run.
    // RTL: the right edge of the ellipsis is to the right of our text run.
    bool ltrFullTruncation = flowIsLTR && ellipsisX <= logicalLeft();
    bool rtlFullTruncation = !flowIsLTR && ellipsisX >= logicalLeft() + logicalWidth();
    if (ltrFullTruncation || rtlFullTruncation) {
        // Too far.  Just set full truncation, but return -1 and let the ellipsis just be placed at the edge of the box.
        setTruncation(cFullTruncation);
        foundBox = true;
        return LayoutUnit(-1);
    }

    bool ltrEllipsisWithinBox = flowIsLTR && (ellipsisX < logicalRight());
    bool rtlEllipsisWithinBox = !flowIsLTR && (ellipsisX > logicalLeft());
    if (ltrEllipsisWithinBox || rtlEllipsisWithinBox) {
        foundBox = true;

        // The inline box may have different directionality than it's parent.  Since truncation
        // behavior depends both on both the parent and the inline block's directionality, we
        // must keep track of these separately.
        bool ltr = isLeftToRightDirection();
        if (ltr != flowIsLTR) {
            // Width in pixels of the visible portion of the box, excluding the ellipsis.
            int visibleBoxWidth = visibleRightEdge - visibleLeftEdge - ellipsisWidth;
            ellipsisX = flowIsLTR ? logicalLeft() + visibleBoxWidth : logicalRight() - visibleBoxWidth;
        }

        int offset = offsetForPosition(ellipsisX, false);
        if (offset == 0) {
            // No characters should be laid out.  Set ourselves to full truncation and place the ellipsis at the min of our start
            // and the ellipsis edge.
            setTruncation(cFullTruncation);
            truncatedWidth += ellipsisWidth;
            return std::min(ellipsisX, logicalLeft());
        }

        // Set the truncation index on the text run.
        setTruncation(offset);

        // If we got here that means that we were only partially truncated and we need to return the pixel offset at which
        // to place the ellipsis. Where the text and its flow have opposite directions then our offset into the text is at
        // the start of the part that will be visible.
        LayoutUnit widthOfVisibleText(getLineLayoutItem().width(ltr == flowIsLTR ? m_start : offset, ltr == flowIsLTR ? offset : m_len - offset, textPos(), flowIsLTR ? LTR : RTL, isFirstLineStyle()));

        // The ellipsis needs to be placed just after the last visible character.
        // Where "after" is defined by the flow directionality, not the inline
        // box directionality.
        // e.g. In the case of an LTR inline box truncated in an RTL flow then we can
        // have a situation such as |Hello| -> |...He|
        truncatedWidth += widthOfVisibleText + ellipsisWidth;
        if (flowIsLTR)
            return logicalLeft() + widthOfVisibleText;
        return logicalRight() - widthOfVisibleText - ellipsisWidth;
    }
    truncatedWidth += logicalWidth();
    return LayoutUnit(-1);
}

bool InlineTextBox::isLineBreak() const
{
    return getLineLayoutItem().isBR() || (getLineLayoutItem().style()->preserveNewline() && len() == 1 && (*getLineLayoutItem().text().impl())[start()] == '\n');
}

bool InlineTextBox::nodeAtPoint(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/)
{
    if (isLineBreak() || m_truncation == cFullTruncation)
        return false;

    LayoutPoint boxOrigin = locationIncludingFlipping();
    boxOrigin.moveBy(accumulatedOffset);
    LayoutRect rect(boxOrigin, size());
    if (visibleToHitTestRequest(result.hitTestRequest()) && locationInContainer.intersects(rect)) {
        getLineLayoutItem().updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - toLayoutSize(accumulatedOffset)));
        if (result.addNodeToListBasedTestResult(getLineLayoutItem().node(), locationInContainer, rect) == StopHitTesting)
            return true;
    }
    return false;
}

bool InlineTextBox::getEmphasisMarkPosition(const ComputedStyle& style, TextEmphasisPosition& emphasisPosition) const
{
    // This function returns true if there are text emphasis marks and they are suppressed by ruby text.
    if (style.getTextEmphasisMark() == TextEmphasisMarkNone)
        return false;

    emphasisPosition = style.getTextEmphasisPosition();
    if (emphasisPosition == TextEmphasisPositionUnder)
        return true; // Ruby text is always over, so it cannot suppress emphasis marks under.

    LineLayoutBox containingBlock = getLineLayoutItem().containingBlock();
    if (!containingBlock.isRubyBase())
        return true; // This text is not inside a ruby base, so it does not have ruby text over it.

    if (!containingBlock.parent().isRubyRun())
        return true; // Cannot get the ruby text.

    LineLayoutRubyText rubyText = LineLayoutRubyRun(containingBlock.parent()).rubyText();

    // The emphasis marks over are suppressed only if there is a ruby text box and it not empty.
    return !rubyText || !rubyText.firstLineBox();
}

void InlineTextBox::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit /*lineTop*/, LayoutUnit /*lineBottom*/) const
{
    InlineTextBoxPainter(*this).paint(paintInfo, paintOffset);
}

void InlineTextBox::selectionStartEnd(int& sPos, int& ePos) const
{
    int startPos, endPos;
    if (getLineLayoutItem().getSelectionState() == SelectionInside) {
        startPos = 0;
        endPos = getLineLayoutItem().textLength();
    } else {
        getLineLayoutItem().selectionStartEnd(startPos, endPos);
        if (getLineLayoutItem().getSelectionState() == SelectionStart)
            endPos = getLineLayoutItem().textLength();
        else if (getLineLayoutItem().getSelectionState() == SelectionEnd)
            startPos = 0;
    }

    sPos = std::max(startPos - m_start, 0);
    ePos = std::min(endPos - m_start, (int)m_len);
}

void InlineTextBox::paintDocumentMarker(GraphicsContext& pt, const LayoutPoint& boxOrigin, DocumentMarker* marker, const ComputedStyle& style, const Font& font, bool grammar) const
{
    InlineTextBoxPainter(*this).paintDocumentMarker(pt, boxOrigin, marker, style, font, grammar);
}

void InlineTextBox::paintTextMatchMarkerForeground(const PaintInfo& paintInfo, const LayoutPoint& boxOrigin, DocumentMarker* marker, const ComputedStyle& style, const Font& font) const
{
    InlineTextBoxPainter(*this).paintTextMatchMarkerForeground(paintInfo, boxOrigin, marker, style, font);
}

void InlineTextBox::paintTextMatchMarkerBackground(const PaintInfo& paintInfo, const LayoutPoint& boxOrigin, DocumentMarker* marker, const ComputedStyle& style, const Font& font) const
{
    InlineTextBoxPainter(*this).paintTextMatchMarkerBackground(paintInfo, boxOrigin, marker, style, font);
}

int InlineTextBox::caretMinOffset() const
{
    return m_start;
}

int InlineTextBox::caretMaxOffset() const
{
    return m_start + m_len;
}

LayoutUnit InlineTextBox::textPos() const
{
    // When computing the width of a text run, LayoutBlock::computeInlineDirectionPositionsForLine() doesn't include the actual offset
    // from the containing block edge in its measurement. textPos() should be consistent so the text are laid out in the same width.
    if (logicalLeft() == 0)
        return LayoutUnit();
    return logicalLeft() - root().logicalLeft();
}

int InlineTextBox::offsetForPosition(LayoutUnit lineOffset, bool includePartialGlyphs) const
{
    if (isLineBreak())
        return 0;

    if (lineOffset - logicalLeft() > logicalWidth())
        return isLeftToRightDirection() ? len() : 0;
    if (lineOffset - logicalLeft() < 0)
        return isLeftToRightDirection() ? 0 : len();

    LineLayoutText text = getLineLayoutItem();
    const ComputedStyle& style = text.styleRef(isFirstLineStyle());
    const Font& font = style.font();
    return font.offsetForPosition(constructTextRun(style, font), (lineOffset - logicalLeft()).toFloat(), includePartialGlyphs);
}

LayoutUnit InlineTextBox::positionForOffset(int offset) const
{
    ASSERT(offset >= m_start);
    ASSERT(offset <= m_start + m_len);

    if (isLineBreak())
        return logicalLeft();

    LineLayoutText text = getLineLayoutItem();
    const ComputedStyle& styleToUse = text.styleRef(isFirstLineStyle());
    const Font& font = styleToUse.font();
    int from = !isLeftToRightDirection() ? offset - m_start : 0;
    int to = !isLeftToRightDirection() ? m_len : offset - m_start;
    // FIXME: Do we need to add rightBearing here?
    return LayoutUnit(font.selectionRectForText(constructTextRun(styleToUse, font), IntPoint(logicalLeft(), 0), 0, from, to).maxX());
}

bool InlineTextBox::containsCaretOffset(int offset) const
{
    // Offsets before the box are never "in".
    if (offset < m_start)
        return false;

    int pastEnd = m_start + m_len;

    // Offsets inside the box (not at either edge) are always "in".
    if (offset < pastEnd)
        return true;

    // Offsets outside the box are always "out".
    if (offset > pastEnd)
        return false;

    // Offsets at the end are "out" for line breaks (they are on the next line).
    if (isLineBreak())
        return false;

    // Offsets at the end are "in" for normal boxes (but the caller has to check affinity).
    return true;
}

void InlineTextBox::characterWidths(Vector<float>& widths) const
{
    if (!m_len)
        return;

    FontCachePurgePreventer fontCachePurgePreventer;
    ASSERT(getLineLayoutItem().text());

    const ComputedStyle& styleToUse = getLineLayoutItem().styleRef(isFirstLineStyle());
    const Font& font = styleToUse.font();

    TextRun textRun = constructTextRun(styleToUse, font);
    Vector<CharacterRange> ranges = font.individualCharacterRanges(textRun);
    DCHECK_EQ(ranges.size(), m_len);

    widths.resize(ranges.size());
    for (unsigned i = 0; i < ranges.size(); i++)
        widths[i] = ranges[i].width();
}

TextRun InlineTextBox::constructTextRun(const ComputedStyle& style, const Font& font, StringBuilder* charactersWithHyphen) const
{
    ASSERT(getLineLayoutItem().text());

    String string = getLineLayoutItem().text();
    unsigned startPos = start();
    unsigned length = len();
    return constructTextRun(style, font, StringView(string, startPos, length), getLineLayoutItem().textLength() - startPos, charactersWithHyphen);
}

TextRun InlineTextBox::constructTextRun(const ComputedStyle& style, const Font& font, StringView string, int maximumLength, StringBuilder* charactersWithHyphen) const
{
    if (charactersWithHyphen) {
        const AtomicString& hyphenString = style.hyphenString();
        charactersWithHyphen->reserveCapacity(string.length() + hyphenString.length());
        charactersWithHyphen->append(string);
        charactersWithHyphen->append(hyphenString);
        string = charactersWithHyphen->toString();
        maximumLength = string.length();
    }

    ASSERT(maximumLength >= static_cast<int>(string.length()));

    TextRun run(string, textPos().toFloat(), expansion(), expansionBehavior(), direction(), dirOverride() || style.rtlOrdering() == VisualOrder);
    run.setTabSize(!style.collapseWhiteSpace(), style.getTabSize());
    run.setTextJustify(style.getTextJustify());

    // Propagate the maximum length of the characters buffer to the TextRun, even when we're only processing a substring.
    run.setCharactersLength(maximumLength);
    ASSERT(run.charactersLength() >= run.length());
    return run;
}

TextRun InlineTextBox::constructTextRunForInspector(const ComputedStyle& style, const Font& font) const
{
    return InlineTextBox::constructTextRun(style, font);
}

const char* InlineTextBox::boxName() const
{
    return "InlineTextBox";
}

String InlineTextBox::debugName() const
{
    return String(boxName()) + " '" + text() + "'";
}

String InlineTextBox::text() const
{
    return getLineLayoutItem().text().substring(start(), len());
}

#ifndef NDEBUG

void InlineTextBox::showBox(int printedCharacters) const
{
    String value = text();
    value.replace('\\', "\\\\");
    value.replace('\n', "\\n");
    printedCharacters += fprintf(stderr, "%s %p", boxName(), this);
    for (; printedCharacters < showTreeCharacterOffset; printedCharacters++)
        fputc(' ', stderr);
    const LineLayoutText obj = getLineLayoutItem();
    printedCharacters = fprintf(stderr, "\t%s %p", obj.name(), obj.debugPointer());
    const int layoutObjectCharacterOffset = 75;
    for (; printedCharacters < layoutObjectCharacterOffset; printedCharacters++)
        fputc(' ', stderr);
    fprintf(stderr, "(%d,%d) \"%s\"\n", start(), start() + len(), value.utf8().data());
}

#endif

} // namespace blink
