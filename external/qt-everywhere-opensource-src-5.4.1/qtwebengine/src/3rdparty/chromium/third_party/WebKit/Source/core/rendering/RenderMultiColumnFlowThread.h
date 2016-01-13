/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef RenderMultiColumnFlowThread_h
#define RenderMultiColumnFlowThread_h

#include "core/rendering/RenderFlowThread.h"

namespace WebCore {

class RenderMultiColumnSet;

// Flow thread implementation for CSS multicol. This will be inserted as an anonymous child block of
// the actual multicol container (i.e. the RenderBlockFlow whose style computes to non-auto
// column-count and/or column-width). RenderMultiColumnFlowThread is the heart of the multicol
// implementation, and there is only one instance per multicol container. Child content of the
// multicol container is parented into the flow thread at the time of renderer insertion.
//
// Apart from this flow thread child, the multicol container will also have RenderMultiColumnSet
// "region" children, which are used to position the columns visually. The flow thread is in charge
// of layout, and, after having calculated the column width, it lays out content as if everything
// were in one tall single column, except that there will typically be some amount of blank space
// (also known as pagination struts) at the offsets where the actual column boundaries are. This
// way, content that needs to be preceded by a break will appear at the top of the next
// column. Content needs to be preceded by a break when there's a forced break or when the content
// is unbreakable and cannot fully fit in the same column as the preceding piece of
// content. Although a RenderMultiColumnFlowThread is laid out, it does not take up any space in its
// container. It's the RenderMultiColumnSet objects that take up the necessary amount of space, and
// make sure that the columns are painted and hit-tested correctly.
//
// The width of the flow thread is the same as the column width. The width of a column set is the
// same as the content box width of the multicol container; in other words exactly enough to hold
// the number of columns to be used, stacked horizontally, plus column gaps between them.
//
// Since it's the first child of the multicol container, the flow thread is laid out first, albeit
// in a slightly special way, since it's not to take up any space in its ancestors. Afterwards, the
// column sets are laid out. They get their height from the columns that they hold. In single
// column-row constrained height non-balancing cases this will simply be the same as the content
// height of the multicol container itself. In most other cases we'll have to calculate optimal
// column heights ourselves, though. This process is referred to as column balancing, and then we
// infer the column set height from the flow thread's height.
//
// More on column balancing: the columns' height is unknown in the first layout pass when
// balancing. This means that we cannot insert any implicit (soft / unforced) breaks (and pagination
// struts) when laying out the contents of the flow thread. We'll just lay out everything in tall
// single strip. After the initial flow thread layout pass we can determine a tentative / minimal /
// initial column height. This is calculated by simply dividing the flow thread's height by the
// number of specified columns. In the layout pass that follows, we can insert breaks (and
// pagination struts) at column boundaries, since we now have a column height. It may very easily
// turn out that the calculated height wasn't enough, though. We'll notice this at end of layout. If
// we end up with too many columns (i.e. columns overflowing the multicol container), it wasn't
// enough. In this case we need to increase the column heights. We'll increase them by the lowest
// amount of space that could possibly affect where the breaks occur (see
// RenderMultiColumnSet::recordSpaceShortage()). We'll relayout (to find new break points and the
// new lowest amount of space increase that could affect where they occur, in case we need another
// round) until we've reached an acceptable height (where everything fits perfectly in the number of
// columns that we have specified). The rule of thumb is that we shouldn't have to perform more of
// such iterations than the number of columns that we have.
//
// For each layout iteration done for column balancing, the flow thread will need a deep layout if
// column heights changed in the previous pass, since column height changes may affect break points
// and pagination struts anywhere in the tree, and currently no way exists to do this in a more
// optimized manner.
class RenderMultiColumnFlowThread FINAL : public RenderFlowThread {
public:
    virtual ~RenderMultiColumnFlowThread();

    static RenderMultiColumnFlowThread* createAnonymous(Document&, RenderStyle* parentStyle);

    virtual bool isRenderMultiColumnFlowThread() const OVERRIDE FINAL { return true; }

    RenderBlockFlow* multiColumnBlockFlow() const { return toRenderBlockFlow(parent()); }

    RenderMultiColumnSet* firstMultiColumnSet() const;
    RenderMultiColumnSet* lastMultiColumnSet() const;

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) OVERRIDE;

    // Populate the flow thread with what's currently its siblings. Called when a regular block
    // becomes a multicol container.
    void populate();

    // Empty the flow thread by moving everything to the parent. Remove all multicol specific
    // renderers. Then destroy the flow thread. Called when a multicol container becomes a regular
    // block.
    void evacuateAndDestroy();

    unsigned columnCount() const { return m_columnCount; }
    LayoutUnit columnHeightAvailable() const { return m_columnHeightAvailable; }
    void setColumnHeightAvailable(LayoutUnit available) { m_columnHeightAvailable = available; }
    bool requiresBalancing() const { return !columnHeightAvailable() || multiColumnBlockFlow()->style()->columnFill() == ColumnFillBalance; }

    virtual LayoutSize columnOffset(const LayoutPoint&) const OVERRIDE FINAL;

    // Do we need to set a new width and lay out?
    bool needsNewWidth() const;

    void layoutColumns(bool relayoutChildren, SubtreeLayoutScope&);

    bool recalculateColumnHeights();

private:
    RenderMultiColumnFlowThread();

    void calculateColumnCountAndWidth(LayoutUnit& width, unsigned& count) const;

    virtual const char* renderName() const OVERRIDE;
    virtual void addRegionToThread(RenderRegion*) OVERRIDE;
    virtual void willBeRemovedFromTree() OVERRIDE;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const OVERRIDE;
    virtual void updateLogicalWidth() OVERRIDE FINAL;
    virtual void layout() OVERRIDE FINAL;
    virtual void setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage) OVERRIDE;
    virtual void updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight) OVERRIDE;
    virtual RenderRegion* regionAtBlockOffset(LayoutUnit) const OVERRIDE;
    virtual bool addForcedRegionBreak(LayoutUnit, RenderObject* breakChild, bool isBefore, LayoutUnit* offsetBreakAdjustment = 0) OVERRIDE;
    virtual bool isPageLogicalHeightKnown() const OVERRIDE;

    unsigned m_columnCount; // The used value of column-count
    LayoutUnit m_columnHeightAvailable; // Total height available to columns, or 0 if auto.
    bool m_inBalancingPass; // Set when relayouting for column balancing.
    bool m_needsColumnHeightsRecalculation; // Set when we need to recalculate the column set heights after layout.
};

} // namespace WebCore

#endif // RenderMultiColumnFlowThread_h

