/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef InlineFlowBox_h
#define InlineFlowBox_h

#include "core/layout/OverflowModel.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/line/InlineBox.h"
#include "core/style/ShadowData.h"
#include <memory>

namespace blink {

class HitTestRequest;
class HitTestResult;
class InlineTextBox;
class LineBoxList;
class SimpleFontData;
class VerticalPositionCache;

struct GlyphOverflow;

typedef HashMap<const InlineTextBox*, std::pair<Vector<const SimpleFontData*>, GlyphOverflow>> GlyphOverflowAndFallbackFontsMap;

class InlineFlowBox : public InlineBox {
public:
    InlineFlowBox(LineLayoutItem lineLayoutItem)
        : InlineBox(lineLayoutItem)
        , m_firstChild(nullptr)
        , m_lastChild(nullptr)
        , m_prevLineBox(nullptr)
        , m_nextLineBox(nullptr)
        , m_includeLogicalLeftEdge(false)
        , m_includeLogicalRightEdge(false)
        , m_descendantsHaveSameLineHeightAndBaseline(true)
        , m_baselineType(AlphabeticBaseline)
        , m_hasAnnotationsBefore(false)
        , m_hasAnnotationsAfter(false)
        , m_lineBreakBidiStatusEor(WTF::Unicode::LeftToRight)
        , m_lineBreakBidiStatusLastStrong(WTF::Unicode::LeftToRight)
        , m_lineBreakBidiStatusLast(WTF::Unicode::LeftToRight)
        , m_isFirstAfterPageBreak(false)
#if ENABLE(ASSERT)
        , m_hasBadChildList(false)
#endif
    {
        // Internet Explorer and Firefox always create a marker for list items, even when the list-style-type is none.  We do not make a marker
        // in the list-style-type: none case, since it is wasteful to do so.  However, in order to match other browsers we have to pretend like
        // an invisible marker exists.  The side effect of having an invisible marker is that the quirks mode behavior of shrinking lines with no
        // text children must not apply.  This change also means that gaps will exist between image bullet list items.  Even when the list bullet
        // is an image, the line is still considered to be immune from the quirk.
        m_hasTextChildren = lineLayoutItem.style()->display() == LIST_ITEM;
        m_hasTextDescendants = m_hasTextChildren;
    }

#if ENABLE(ASSERT)
    ~InlineFlowBox() override;
#endif

#ifndef NDEBUG
    void showLineTreeAndMark(const InlineBox* = nullptr, const char* = nullptr, const InlineBox* = nullptr, const char* = nullptr, const LayoutObject* = nullptr, int = 0) const override;
#endif
    const char* boxName() const override;

    InlineFlowBox* prevLineBox() const { return m_prevLineBox; }
    InlineFlowBox* nextLineBox() const { return m_nextLineBox; }
    void setNextLineBox(InlineFlowBox* n) { m_nextLineBox = n; }
    void setPreviousLineBox(InlineFlowBox* p) { m_prevLineBox = p; }

    InlineBox* firstChild() const { checkConsistency(); return m_firstChild; }
    InlineBox* lastChild() const { checkConsistency(); return m_lastChild; }

    bool isLeaf() const final { return false; }

    InlineBox* firstLeafChild() const;
    InlineBox* lastLeafChild() const;

    typedef void (*CustomInlineBoxRangeReverse)(Vector<InlineBox*>::iterator first, Vector<InlineBox*>::iterator last);
    void collectLeafBoxesInLogicalOrder(Vector<InlineBox*>&, CustomInlineBoxRangeReverse customReverseImplementation = 0) const;

    void setConstructed() final
    {
        InlineBox::setConstructed();
        for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
            child->setConstructed();
    }

    void addToLine(InlineBox* child);
    void deleteLine() final;
    void extractLine() final;
    void attachLine() final;
    void move(const LayoutSize&) override;

    virtual void extractLineBoxFromLayoutObject();
    virtual void attachLineBoxToLayoutObject();
    virtual void removeLineBoxFromLayoutObject();

    void clearTruncation() override;

    LayoutRect frameRect() const;

    void paint(const PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) const override;
    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom) override;

    bool boxShadowCanBeAppliedToBackground(const FillLayer&) const;

    virtual LineBoxList* lineBoxes() const;

    // logicalLeft = left in a horizontal line and top in a vertical line.
    LayoutUnit marginBorderPaddingLogicalLeft() const { return marginLogicalLeft() + borderLogicalLeft() + paddingLogicalLeft(); }
    LayoutUnit marginBorderPaddingLogicalRight() const { return marginLogicalRight() + borderLogicalRight() + paddingLogicalRight(); }
    LayoutUnit marginLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return LayoutUnit();
        return isHorizontal() ? boxModelObject().marginLeft() : boxModelObject().marginTop();
    }
    LayoutUnit marginLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return LayoutUnit();
        return isHorizontal() ? boxModelObject().marginRight() : boxModelObject().marginBottom();
    }
    int borderLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return 0;
        return isHorizontal() ? getLineLayoutItem().style(isFirstLineStyle())->borderLeftWidth() : getLineLayoutItem().style(isFirstLineStyle())->borderTopWidth();
    }
    int borderLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return 0;
        return isHorizontal() ? getLineLayoutItem().style(isFirstLineStyle())->borderRightWidth() : getLineLayoutItem().style(isFirstLineStyle())->borderBottomWidth();
    }
    int paddingLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return 0;
        return isHorizontal() ? boxModelObject().paddingLeft() : boxModelObject().paddingTop();
    }
    int paddingLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return 0;
        return isHorizontal() ? boxModelObject().paddingRight() : boxModelObject().paddingBottom();
    }

    bool includeLogicalLeftEdge() const { return m_includeLogicalLeftEdge; }
    bool includeLogicalRightEdge() const { return m_includeLogicalRightEdge; }
    void setEdges(bool includeLeft, bool includeRight)
    {
        m_includeLogicalLeftEdge = includeLeft;
        m_includeLogicalRightEdge = includeRight;
    }

    // Helper functions used during line construction and placement.
    void determineSpacingForFlowBoxes(bool lastLine, bool isLogicallyLastRunWrapped, LineLayoutItem logicallyLastRunLayoutObject);
    LayoutUnit getFlowSpacingLogicalWidth();
    LayoutUnit placeBoxesInInlineDirection(LayoutUnit logicalLeft, bool& needsWordSpacing);

    void computeLogicalBoxHeights(RootInlineBox*, LayoutUnit& maxPositionTop, LayoutUnit& maxPositionBottom, int& maxAscent, int& maxDescent, bool& setMaxAscent, bool& setMaxDescent, bool noQuirksMode, GlyphOverflowAndFallbackFontsMap&, FontBaseline, VerticalPositionCache&);
    void adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent, int maxPositionTop, int maxPositionBottom);
    void placeBoxesInBlockDirection(LayoutUnit logicalTop, LayoutUnit maxHeight, int maxAscent, bool noQuirksMode, LayoutUnit& lineTop, LayoutUnit& lineBottom, LayoutUnit& selectionBottom, bool& setLineTop, LayoutUnit& lineTopIncludingMargins, LayoutUnit& lineBottomIncludingMargins, bool& hasAnnotationsBefore, bool& hasAnnotationsAfter, FontBaseline);
    void flipLinesInBlockDirection(LayoutUnit lineTop, LayoutUnit lineBottom);
    FontBaseline dominantBaseline() const;

    LayoutUnit computeOverAnnotationAdjustment(LayoutUnit allowedPosition) const;
    LayoutUnit computeUnderAnnotationAdjustment(LayoutUnit allowedPosition) const;

    void computeOverflow(LayoutUnit lineTop, LayoutUnit lineBottom, GlyphOverflowAndFallbackFontsMap&);

    void removeChild(InlineBox* child, MarkLineBoxes);

    SelectionState getSelectionState() const override;

    bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth) const final;
    LayoutUnit placeEllipsisBox(bool ltr, LayoutUnit blockLeftEdge, LayoutUnit blockRightEdge, LayoutUnit ellipsisWidth, LayoutUnit &truncatedWidth, bool&) override;

    bool hasTextChildren() const { return m_hasTextChildren; }
    bool hasTextDescendants() const { return m_hasTextDescendants; }
    void setHasTextDescendants() { m_hasTextDescendants = true; }

    void checkConsistency() const;
    void setHasBadChildList();

    // Line visual and layout overflow are in the coordinate space of the block.  This means that
    // they aren't purely physical directions. For horizontal-tb and vertical-lr they will match
    // physical directions, but for vertical-rl, the left/right respectively are flipped when
    // compared to their physical counterparts.  For example minX is on the left in vertical-lr, but
    // it is on the right in vertical-rl.
    LayoutRect layoutOverflowRect(LayoutUnit lineTop, LayoutUnit lineBottom) const
    {
        return m_overflow ? m_overflow->layoutOverflowRect() : frameRectIncludingLineHeight(lineTop, lineBottom);
    }
    LayoutUnit logicalTopLayoutOverflow(LayoutUnit lineTop) const
    {
        if (m_overflow)
            return isHorizontal() ? m_overflow->layoutOverflowRect().y() : m_overflow->layoutOverflowRect().x();
        return lineTop;
    }
    LayoutUnit logicalBottomLayoutOverflow(LayoutUnit lineBottom) const
    {
        if (m_overflow)
            return isHorizontal() ? m_overflow->layoutOverflowRect().maxY() : m_overflow->layoutOverflowRect().maxX();
        return lineBottom;
    }
    LayoutRect logicalLayoutOverflowRect(LayoutUnit lineTop, LayoutUnit lineBottom) const
    {
        LayoutRect result = layoutOverflowRect(lineTop, lineBottom);
        if (!getLineLayoutItem().isHorizontalWritingMode())
            result = result.transposedRect();
        return result;
    }

    LayoutRect visualOverflowRect(LayoutUnit lineTop, LayoutUnit lineBottom) const
    {
        return m_overflow ? m_overflow->visualOverflowRect() : frameRectIncludingLineHeight(lineTop, lineBottom);
    }
    LayoutUnit logicalLeftVisualOverflow() const { return m_overflow ? (isHorizontal() ? m_overflow->visualOverflowRect().x() : m_overflow->visualOverflowRect().y()) : logicalLeft(); }
    LayoutUnit logicalRightVisualOverflow() const { return m_overflow ? (isHorizontal() ? m_overflow->visualOverflowRect().maxX() : m_overflow->visualOverflowRect().maxY()) : static_cast<LayoutUnit>(logicalRight().ceil()); }
    LayoutUnit logicalTopVisualOverflow(LayoutUnit lineTop) const
    {
        if (m_overflow)
            return isHorizontal() ? m_overflow->visualOverflowRect().y() : m_overflow->visualOverflowRect().x();
        return lineTop;
    }
    LayoutUnit logicalBottomVisualOverflow(LayoutUnit lineBottom) const
    {
        if (m_overflow)
            return isHorizontal() ? m_overflow->visualOverflowRect().maxY() : m_overflow->visualOverflowRect().maxX();
        return lineBottom;
    }
    LayoutRect logicalVisualOverflowRect(LayoutUnit lineTop, LayoutUnit lineBottom) const
    {
        LayoutRect result = visualOverflowRect(lineTop, lineBottom);
        if (!getLineLayoutItem().isHorizontalWritingMode())
            result = result.transposedRect();
        return result;
    }

    LayoutRect frameRectIncludingLineHeight(LayoutUnit lineTop, LayoutUnit lineBottom) const
    {
        if (isHorizontal())
            return LayoutRect(m_topLeft.x(), lineTop, width(), lineBottom - lineTop);
        return LayoutRect(lineTop, m_topLeft.y(), lineBottom - lineTop, height());
    }

    LayoutRect logicalFrameRectIncludingLineHeight(LayoutUnit lineTop, LayoutUnit lineBottom) const
    {
        return LayoutRect(logicalLeft(), lineTop, logicalWidth(), lineBottom - lineTop);
    }

    bool descendantsHaveSameLineHeightAndBaseline() const { return m_descendantsHaveSameLineHeightAndBaseline; }
    void clearDescendantsHaveSameLineHeightAndBaseline()
    {
        m_descendantsHaveSameLineHeightAndBaseline = false;
        if (parent() && parent()->descendantsHaveSameLineHeightAndBaseline())
            parent()->clearDescendantsHaveSameLineHeightAndBaseline();
    }

    bool isFirstAfterPageBreak() const { return m_isFirstAfterPageBreak; }
    void setIsFirstAfterPageBreak(bool isFirstAfterPageBreak) { m_isFirstAfterPageBreak = isFirstAfterPageBreak; }

    // Some callers (LayoutListItem) needs to set extra overflow on their line box.
    void overrideOverflowFromLogicalRects(const LayoutRect& logicalLayoutOverflow, const LayoutRect& logicalVisualOverflow, LayoutUnit lineTop, LayoutUnit lineBottom)
    {
        // If we are setting an overflow, then we can't pretend not to have an overflow.
        clearKnownToHaveNoOverflow();
        setOverflowFromLogicalRects(logicalLayoutOverflow, logicalVisualOverflow, lineTop, lineBottom);
    }

private:
    void placeBoxRangeInInlineDirection(InlineBox* firstChild, InlineBox* lastChild,
        LayoutUnit& logicalLeft, LayoutUnit& minLogicalLeft, LayoutUnit& maxLogicalRight, bool& needsWordSpacing);
    void beginPlacingBoxRangesInInlineDirection(LayoutUnit logicalLeft) { setLogicalLeft(logicalLeft); }
    void endPlacingBoxRangesInInlineDirection(LayoutUnit logicalLeft, LayoutUnit logicalRight, LayoutUnit minLogicalLeft, LayoutUnit maxLogicalRight)
    {
        setLogicalWidth(logicalRight - logicalLeft);
        if (knownToHaveNoOverflow() && (minLogicalLeft < logicalLeft || maxLogicalRight > logicalRight))
            clearKnownToHaveNoOverflow();
    }

    void addBoxShadowVisualOverflow(LayoutRect& logicalVisualOverflow);
    void addBorderOutsetVisualOverflow(LayoutRect& logicalVisualOverflow);
    void addOutlineVisualOverflow(LayoutRect& logicalVisualOverflow);
    void addTextBoxVisualOverflow(InlineTextBox*, GlyphOverflowAndFallbackFontsMap&, LayoutRect& logicalVisualOverflow);
    void addReplacedChildOverflow(const InlineBox*, LayoutRect& logicalLayoutOverflow, LayoutRect& logicalVisualOverflow);

    void setLayoutOverflow(const LayoutRect&, const LayoutRect&);
    void setVisualOverflow(const LayoutRect&, const LayoutRect&);

    void setOverflowFromLogicalRects(const LayoutRect& logicalLayoutOverflow, const LayoutRect& logicalVisualOverflow, LayoutUnit lineTop, LayoutUnit lineBottom);

protected:
    std::unique_ptr<SimpleOverflowModel> m_overflow;

    bool isInlineFlowBox() const final { return true; }

    InlineBox* m_firstChild;
    InlineBox* m_lastChild;

    InlineFlowBox* m_prevLineBox; // The previous box that also uses our LayoutObject
    InlineFlowBox* m_nextLineBox; // The next box that also uses our LayoutObject

    // Maximum logicalTop among all children of an InlineFlowBox. Used to
    // calculate the offset for TextUnderlinePositionUnder.
    void computeMaxLogicalTop(LayoutUnit& maxLogicalTop) const;

private:
    unsigned m_includeLogicalLeftEdge : 1;
    unsigned m_includeLogicalRightEdge : 1;
    unsigned m_hasTextChildren : 1;
    unsigned m_hasTextDescendants : 1;
    unsigned m_descendantsHaveSameLineHeightAndBaseline : 1;

protected:
    // The following members are only used by RootInlineBox but moved here to keep the bits packed.

    // Whether or not this line uses alphabetic or ideographic baselines by default.
    unsigned m_baselineType : 1; // FontBaseline

    // If the line contains any ruby runs, then this will be true.
    unsigned m_hasAnnotationsBefore : 1;
    unsigned m_hasAnnotationsAfter : 1;

    unsigned m_lineBreakBidiStatusEor : 5; // WTF::Unicode::Direction
    unsigned m_lineBreakBidiStatusLastStrong : 5; // WTF::Unicode::Direction
    unsigned m_lineBreakBidiStatusLast : 5; // WTF::Unicode::Direction

    unsigned m_isFirstAfterPageBreak : 1;

    // End of RootInlineBox-specific members.

#if ENABLE(ASSERT)
private:
    unsigned m_hasBadChildList : 1;
#endif
};

DEFINE_INLINE_BOX_TYPE_CASTS(InlineFlowBox);

#if !ENABLE(ASSERT)
inline void InlineFlowBox::checkConsistency() const
{
}
#endif

inline void InlineFlowBox::setHasBadChildList()
{
#if ENABLE(ASSERT)
    m_hasBadChildList = true;
#endif
}

} // namespace blink

#endif // InlineFlowBox_h
