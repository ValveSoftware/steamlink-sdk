/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderListBox_h
#define RenderListBox_h

#include "core/rendering/RenderBlockFlow.h"
#include "platform/scroll/ScrollableArea.h"

namespace WebCore {

class HTMLSelectElement;

class RenderListBox FINAL : public RenderBlockFlow, private ScrollableArea {
public:
    explicit RenderListBox(Element*);
    virtual ~RenderListBox();

    void selectionChanged();

    void setOptionsChanged(bool changed) { m_optionsChanged = changed; }

    int listIndexAtOffset(const LayoutSize&) const;
    LayoutRect itemBoundingBoxRect(const LayoutPoint&, int index) const;

    bool scrollToRevealElementAtListIndex(int index);

    int scrollToward(const IntPoint&); // Returns the new index or -1 if no scroll occurred

    int size() const;

    void repaintScrollbarIfNeeded();

private:
    HTMLSelectElement* selectElement() const;

    virtual const char* renderName() const OVERRIDE { return "RenderListBox"; }

    virtual bool isListBox() const OVERRIDE { return true; }
    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const OVERRIDE;

    virtual void updateFromElement() OVERRIDE;
    virtual bool hasControlClip() const OVERRIDE { return true; }
    virtual void paintObject(PaintInfo&, const LayoutPoint&) OVERRIDE;
    virtual LayoutRect controlClipRect(const LayoutPoint&) const OVERRIDE;

    virtual bool isPointInOverflowControl(HitTestResult&, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset) OVERRIDE;

    virtual bool scroll(ScrollDirection, ScrollGranularity, float) OVERRIDE;

    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const OVERRIDE;
    virtual void computePreferredLogicalWidths() OVERRIDE;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const OVERRIDE;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const OVERRIDE;

    virtual void layout() OVERRIDE;

    virtual void invalidateTreeAfterLayout(const RenderLayerModelObject&) OVERRIDE FINAL;

    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) OVERRIDE;

    virtual bool canBeProgramaticallyScrolled() const OVERRIDE { return true; }
    virtual void autoscroll(const IntPoint&) OVERRIDE;
    virtual void stopAutoscroll() OVERRIDE;

    virtual void panScroll(const IntPoint&) OVERRIDE;

    virtual int verticalScrollbarWidth() const OVERRIDE;
    virtual LayoutUnit scrollLeft() const OVERRIDE;
    virtual LayoutUnit scrollTop() const OVERRIDE;
    virtual LayoutUnit scrollWidth() const OVERRIDE;
    virtual LayoutUnit scrollHeight() const OVERRIDE;
    virtual void setScrollLeft(LayoutUnit) OVERRIDE;
    virtual void setScrollTop(LayoutUnit) OVERRIDE;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;

    // ScrollableArea interface.
    virtual int scrollSize(ScrollbarOrientation) const OVERRIDE;
    virtual IntPoint scrollPosition() const OVERRIDE;
    virtual void setScrollOffset(const IntPoint&) OVERRIDE;
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&) OVERRIDE;
    virtual bool isActive() const OVERRIDE;
    virtual bool isScrollCornerVisible() const OVERRIDE { return false; } // We don't support resize on list boxes yet. If we did these would have to change.
    virtual IntRect scrollCornerRect() const OVERRIDE { return IntRect(); }
    virtual void invalidateScrollCornerRect(const IntRect&) OVERRIDE { }
    virtual IntRect convertFromScrollbarToContainingView(const Scrollbar*, const IntRect&) const OVERRIDE;
    virtual IntRect convertFromContainingViewToScrollbar(const Scrollbar*, const IntRect&) const OVERRIDE;
    virtual IntPoint convertFromScrollbarToContainingView(const Scrollbar*, const IntPoint&) const OVERRIDE;
    virtual IntPoint convertFromContainingViewToScrollbar(const Scrollbar*, const IntPoint&) const OVERRIDE;
    virtual Scrollbar* verticalScrollbar() const OVERRIDE { return m_vBar.get(); }
    virtual IntSize contentsSize() const OVERRIDE;
    virtual int visibleHeight() const OVERRIDE;
    virtual int visibleWidth() const OVERRIDE;
    virtual IntPoint lastKnownMousePosition() const OVERRIDE;
    virtual bool shouldSuspendScrollAnimations() const OVERRIDE;
    virtual bool scrollbarsCanBeActive() const OVERRIDE;
    virtual IntPoint minimumScrollPosition() const OVERRIDE;
    virtual IntPoint maximumScrollPosition() const OVERRIDE;
    virtual bool userInputScrollable(ScrollbarOrientation) const OVERRIDE;
    virtual bool shouldPlaceVerticalScrollbarOnLeft() const OVERRIDE;
    virtual int lineStep(ScrollbarOrientation) const OVERRIDE;
    virtual int pageStep(ScrollbarOrientation) const OVERRIDE;
    virtual float pixelStep(ScrollbarOrientation) const OVERRIDE;
    virtual IntRect scrollableAreaBoundingBox() const OVERRIDE;

    LayoutRect itemBoundingBoxRectInternal(const LayoutPoint&, int index) const;
    bool scrollToRevealElementAtListIndexInternal(int index);

    // NOTE: This should only be called by the overriden setScrollOffset from ScrollableArea.
    void scrollTo(int newOffset);

    void setHasVerticalScrollbar(bool hasScrollbar);
    PassRefPtr<Scrollbar> createScrollbar();
    void destroyScrollbar();

    LayoutUnit itemHeight() const;
    int numVisibleItems() const;
    bool listIndexIsVisible(int index) const;
    int numItems() const;
    LayoutUnit listHeight() const;
    int scrollbarLeft() const;
    void paintScrollbar(PaintInfo&, const LayoutPoint&);
    void paintItemForeground(PaintInfo&, const LayoutPoint&, int listIndex);
    void paintItemBackground(PaintInfo&, const LayoutPoint&, int listIndex);
    void scrollToRevealSelection();

    int renderListBoxIndexToListIndex(int index) const;
    int listIndexToRenderListBoxIndex(int index) const;

    bool m_optionsChanged;
    bool m_scrollToRevealSelectionAfterLayout;
    bool m_inAutoscroll;
    int m_optionsWidth;
    int m_indexOffset;
    int m_listItemCount;

    RefPtr<Scrollbar> m_vBar;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderListBox, isListBox());

} // namepace WebCore

#endif // RenderListBox_h
