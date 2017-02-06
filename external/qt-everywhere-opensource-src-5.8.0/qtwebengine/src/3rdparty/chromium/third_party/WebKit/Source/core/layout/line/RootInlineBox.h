/*
 * Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef RootInlineBox_h
#define RootInlineBox_h

#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/line/InlineFlowBox.h"
#include "platform/text/BidiContext.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class EllipsisBox;
class HitTestResult;
class LineLayoutBlockFlow;

struct BidiStatus;
struct GapRects;

class RootInlineBox : public InlineFlowBox {
public:
    explicit RootInlineBox(LineLayoutItem);

    void destroy() final;

    bool isRootInlineBox() const final { return true; }

    void detachEllipsisBox();

    RootInlineBox* nextRootBox() const { return static_cast<RootInlineBox*>(m_nextLineBox); }
    RootInlineBox* prevRootBox() const { return static_cast<RootInlineBox*>(m_prevLineBox); }

    void move(const LayoutSize&) final;

    LayoutUnit lineTop() const { return m_lineTop; }
    LayoutUnit lineBottom() const { return m_lineBottom; }

    LayoutUnit lineTopWithLeading() const { return m_lineTopWithLeading; }
    LayoutUnit lineBottomWithLeading() const { return m_lineBottomWithLeading; }

    LayoutUnit paginationStrut() const { return m_paginationStrut; }
    void setPaginationStrut(LayoutUnit strut) { m_paginationStrut = strut; }

    LayoutUnit selectionTop() const;
    LayoutUnit selectionBottom() const;
    LayoutUnit selectionHeight() const { return (selectionBottom() - selectionTop()).clampNegativeToZero(); }

    LayoutUnit blockDirectionPointInLine() const;

    LayoutUnit alignBoxesInBlockDirection(LayoutUnit heightOfBlock, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&);
    void setLineTopBottomPositions(LayoutUnit top, LayoutUnit bottom, LayoutUnit topWithLeading, LayoutUnit bottomWithLeading, LayoutUnit selectionBottom = LayoutUnit::min())
    {
        m_lineTop = top;
        m_lineBottom = bottom;
        m_lineTopWithLeading = topWithLeading;
        m_lineBottomWithLeading = bottomWithLeading;
        m_selectionBottom = selectionBottom == LayoutUnit::min() ? bottom : selectionBottom;
    }

    LineBoxList* lineBoxes() const final;

    LineLayoutItem lineBreakObj() const { return m_lineBreakObj; }
    BidiStatus lineBreakBidiStatus() const;
    void setLineBreakInfo(LineLayoutItem, unsigned breakPos, const BidiStatus&);

    unsigned lineBreakPos() const { return m_lineBreakPos; }
    void setLineBreakPos(unsigned p) { m_lineBreakPos = p; }

    using InlineBox::endsWithBreak;
    using InlineBox::setEndsWithBreak;

    void childRemoved(InlineBox*);

    bool lineCanAccommodateEllipsis(bool ltr, int blockEdge, int lineBoxEdge, int ellipsisWidth);
    // Return the truncatedWidth, the width of the truncated text + ellipsis.
    LayoutUnit placeEllipsis(const AtomicString& ellipsisStr, bool ltr, LayoutUnit blockLeftEdge, LayoutUnit blockRightEdge, LayoutUnit ellipsisWidth);
    // Return the position of the EllipsisBox or -1.
    LayoutUnit placeEllipsisBox(bool ltr, LayoutUnit blockLeftEdge, LayoutUnit blockRightEdge, LayoutUnit ellipsisWidth, LayoutUnit &truncatedWidth, bool& foundBox) final;

    using InlineBox::hasEllipsisBox;
    EllipsisBox* ellipsisBox() const;

    void clearTruncation() final;

    int baselinePosition(FontBaseline baselineType) const final;
    LayoutUnit lineHeight() const final;

    void paint(const PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) const override;
    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom) override;

    using InlineBox::hasSelectedChildren;
    using InlineBox::setHasSelectedChildren;

    SelectionState getSelectionState() const final;
    InlineBox* firstSelectedBox() const;
    InlineBox* lastSelectedBox() const;

    LineLayoutBlockFlow block() const;

    InlineBox* closestLeafChildForPoint(const LayoutPoint&, bool onlyEditableLeaves);
    InlineBox* closestLeafChildForLogicalLeftPosition(LayoutUnit, bool onlyEditableLeaves = false);

    void appendFloat(LayoutBox* floatingBox)
    {
        ASSERT(!isDirty());
        if (m_floats)
            m_floats->append(floatingBox);
        else
            m_floats= wrapUnique(new Vector<LayoutBox*>(1, floatingBox));
    }

    Vector<LayoutBox*>* floatsPtr() { ASSERT(!isDirty()); return m_floats.get(); }

    void extractLineBoxFromLayoutObject() final;
    void attachLineBoxToLayoutObject() final;
    void removeLineBoxFromLayoutObject() final;

    FontBaseline baselineType() const { return static_cast<FontBaseline>(m_baselineType); }

    bool hasAnnotationsBefore() const { return m_hasAnnotationsBefore; }
    bool hasAnnotationsAfter() const { return m_hasAnnotationsAfter; }

    LayoutRect paddedLayoutOverflowRect(LayoutUnit endPadding) const;

    void ascentAndDescentForBox(InlineBox*, GlyphOverflowAndFallbackFontsMap&, int& ascent, int& descent, bool& affectsAscent, bool& affectsDescent) const;
    LayoutUnit verticalPositionForBox(InlineBox*, VerticalPositionCache&);
    bool includeLeadingForBox(InlineBox*) const;

    LayoutUnit logicalTopVisualOverflow() const
    {
        return InlineFlowBox::logicalTopVisualOverflow(lineTop());
    }
    LayoutUnit logicalBottomVisualOverflow() const
    {
        return InlineFlowBox::logicalBottomVisualOverflow(lineBottom());
    }
    LayoutUnit logicalTopLayoutOverflow() const
    {
        return InlineFlowBox::logicalTopLayoutOverflow(lineTop());
    }
    LayoutUnit logicalBottomLayoutOverflow() const
    {
        return InlineFlowBox::logicalBottomLayoutOverflow(lineBottom());
    }

    // Used to calculate the underline offset for TextUnderlinePositionUnder.
    LayoutUnit maxLogicalTop() const;

    Node* getLogicalStartBoxWithNode(InlineBox*&) const;
    Node* getLogicalEndBoxWithNode(InlineBox*&) const;

    const char* boxName() const override;

private:
    LayoutUnit beforeAnnotationsAdjustment() const;

    // This folds into the padding at the end of InlineFlowBox on 64-bit.
    unsigned m_lineBreakPos;

    // Where this line ended.  The exact object and the position within that object are stored so that
    // we can create an InlineIterator beginning just after the end of this line.
    LineLayoutItem m_lineBreakObj;
    RefPtr<BidiContext> m_lineBreakContext;

    // Floats hanging off the line are pushed into this vector during layout. It is only
    // good for as long as the line has not been marked dirty.
    std::unique_ptr<Vector<LayoutBox*>> m_floats;

    LayoutUnit m_lineTop;
    LayoutUnit m_lineBottom;
    LayoutUnit m_lineTopWithLeading;
    LayoutUnit m_lineBottomWithLeading;
    LayoutUnit m_selectionBottom;
    LayoutUnit m_paginationStrut;
};

} // namespace blink

#endif // RootInlineBox_h
