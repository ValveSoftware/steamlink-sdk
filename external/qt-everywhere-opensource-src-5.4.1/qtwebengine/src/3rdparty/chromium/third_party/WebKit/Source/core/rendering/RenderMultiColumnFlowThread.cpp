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
 * PROFITS; OR BUSINESS IN..0TERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/RenderMultiColumnFlowThread.h"

#include "core/rendering/RenderMultiColumnSet.h"

namespace WebCore {

RenderMultiColumnFlowThread::RenderMultiColumnFlowThread()
    : m_columnCount(1)
    , m_columnHeightAvailable(0)
    , m_inBalancingPass(false)
    , m_needsColumnHeightsRecalculation(false)
{
    setFlowThreadState(InsideInFlowThread);
}

RenderMultiColumnFlowThread::~RenderMultiColumnFlowThread()
{
}

RenderMultiColumnFlowThread* RenderMultiColumnFlowThread::createAnonymous(Document& document, RenderStyle* parentStyle)
{
    RenderMultiColumnFlowThread* renderer = new RenderMultiColumnFlowThread();
    renderer->setDocumentForAnonymous(&document);
    renderer->setStyle(RenderStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK));
    return renderer;
}

RenderMultiColumnSet* RenderMultiColumnFlowThread::firstMultiColumnSet() const
{
    for (RenderObject* sibling = nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (sibling->isRenderMultiColumnSet())
            return toRenderMultiColumnSet(sibling);
    }
    return 0;
}

RenderMultiColumnSet* RenderMultiColumnFlowThread::lastMultiColumnSet() const
{
    for (RenderObject* sibling = multiColumnBlockFlow()->lastChild(); sibling; sibling = sibling->previousSibling()) {
        if (sibling->isRenderMultiColumnSet())
            return toRenderMultiColumnSet(sibling);
    }
    return 0;
}

void RenderMultiColumnFlowThread::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    RenderBlockFlow::addChild(newChild, beforeChild);
    if (firstMultiColumnSet())
        return;

    // For now we only create one column set. It's created as soon as the multicol container gets
    // any content at all.
    RenderMultiColumnSet* newSet = RenderMultiColumnSet::createAnonymous(this, multiColumnBlockFlow()->style());

    // Need to skip RenderBlockFlow's implementation of addChild(), or we'd get redirected right
    // back here.
    multiColumnBlockFlow()->RenderBlock::addChild(newSet);

    invalidateRegions();
}

void RenderMultiColumnFlowThread::populate()
{
    RenderBlockFlow* multicolContainer = multiColumnBlockFlow();
    ASSERT(!nextSibling());
    // Reparent children preceding the flow thread into the flow thread. It's multicol content
    // now. At this point there's obviously nothing after the flow thread, but renderers (column
    // sets and spanners) will be inserted there as we insert elements into the flow thread.
    multicolContainer->moveChildrenTo(this, multicolContainer->firstChild(), this, true);
}

void RenderMultiColumnFlowThread::evacuateAndDestroy()
{
    RenderBlockFlow* multicolContainer = multiColumnBlockFlow();

    // Remove all sets.
    while (RenderMultiColumnSet* columnSet = firstMultiColumnSet())
        columnSet->destroy();

    ASSERT(!previousSibling());
    ASSERT(!nextSibling());

    // Finally we can promote all flow thread's children. Before we move them to the flow thread's
    // container, we need to unregister the flow thread, so that they aren't just re-added again to
    // the flow thread that we're trying to empty.
    multicolContainer->resetMultiColumnFlowThread();
    moveAllChildrenTo(multicolContainer, true);

    // FIXME: it's scary that neither destroy() nor the move*Children* methods take care of this,
    // and instead leave you with dangling root line box pointers. But since this is how it is done
    // in other parts of the code that deal with reparenting renderers, let's do the cleanup on our
    // own here as well.
    deleteLineBoxTree();

    destroy();
}

LayoutSize RenderMultiColumnFlowThread::columnOffset(const LayoutPoint& point) const
{
    if (!hasValidRegionInfo())
        return LayoutSize(0, 0);

    LayoutPoint flowThreadPoint(point);
    flipForWritingMode(flowThreadPoint);
    LayoutUnit blockOffset = isHorizontalWritingMode() ? flowThreadPoint.y() : flowThreadPoint.x();
    RenderRegion* renderRegion = regionAtBlockOffset(blockOffset);
    if (!renderRegion)
        return LayoutSize(0, 0);
    return toRenderMultiColumnSet(renderRegion)->flowThreadTranslationAtOffset(blockOffset);
}

bool RenderMultiColumnFlowThread::needsNewWidth() const
{
    LayoutUnit newWidth;
    unsigned dummyColumnCount; // We only care if used column-width changes.
    calculateColumnCountAndWidth(newWidth, dummyColumnCount);
    return newWidth != logicalWidth();
}

void RenderMultiColumnFlowThread::layoutColumns(bool relayoutChildren, SubtreeLayoutScope& layoutScope)
{
    if (relayoutChildren)
        layoutScope.setChildNeedsLayout(this);

    if (!needsLayout()) {
        // Just before the multicol container (our parent RenderBlockFlow) finishes laying out, it
        // will call recalculateColumnHeights() on us unconditionally, but we only want that method
        // to do any work if we actually laid out the flow thread. Otherwise, the balancing
        // machinery would kick in needlessly, and trigger additional layout passes. Furthermore, we
        // actually depend on a proper flowthread layout pass in order to do balancing, since it's
        // flowthread layout that sets up content runs.
        m_needsColumnHeightsRecalculation = false;
        return;
    }

    for (RenderMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet()) {
        if (!m_inBalancingPass) {
            // This is the initial layout pass. We need to reset the column height, because contents
            // typically have changed.
            columnSet->resetColumnHeight();
        }
    }

    invalidateRegions();
    m_needsColumnHeightsRecalculation = requiresBalancing();
    layout();
}

bool RenderMultiColumnFlowThread::recalculateColumnHeights()
{
    // All column sets that needed layout have now been laid out, so we can finally validate them.
    validateRegions();

    if (!m_needsColumnHeightsRecalculation)
        return false;

    // Column heights may change here because of balancing. We may have to do multiple layout
    // passes, depending on how the contents is fitted to the changed column heights. In most
    // cases, laying out again twice or even just once will suffice. Sometimes we need more
    // passes than that, though, but the number of retries should not exceed the number of
    // columns, unless we have a bug.
    bool needsRelayout = false;
    for (RenderMultiColumnSet* multicolSet = firstMultiColumnSet(); multicolSet; multicolSet = multicolSet->nextSiblingMultiColumnSet()) {
        needsRelayout |= multicolSet->recalculateColumnHeight(m_inBalancingPass ? RenderMultiColumnSet::StretchBySpaceShortage : RenderMultiColumnSet::GuessFromFlowThreadPortion);
        if (needsRelayout) {
            // Once a column set gets a new column height, that column set and all successive column
            // sets need to be laid out over again, since their logical top will be affected by
            // this, and therefore their column heights may change as well, at least if the multicol
            // height is constrained.
            multicolSet->setChildNeedsLayout(MarkOnlyThis);
        }
    }

    if (needsRelayout)
        setChildNeedsLayout(MarkOnlyThis);

    m_inBalancingPass = needsRelayout;
    return needsRelayout;
}

void RenderMultiColumnFlowThread::calculateColumnCountAndWidth(LayoutUnit& width, unsigned& count) const
{
    RenderBlock* columnBlock = multiColumnBlockFlow();
    const RenderStyle* columnStyle = columnBlock->style();
    LayoutUnit availableWidth = columnBlock->contentLogicalWidth();
    LayoutUnit columnGap = columnBlock->columnGap();
    LayoutUnit computedColumnWidth = max<LayoutUnit>(1, LayoutUnit(columnStyle->columnWidth()));
    unsigned computedColumnCount = max<int>(1, columnStyle->columnCount());

    ASSERT(!columnStyle->hasAutoColumnCount() || !columnStyle->hasAutoColumnWidth());
    if (columnStyle->hasAutoColumnWidth() && !columnStyle->hasAutoColumnCount()) {
        count = computedColumnCount;
        width = std::max<LayoutUnit>(0, (availableWidth - ((count - 1) * columnGap)) / count);
    } else if (!columnStyle->hasAutoColumnWidth() && columnStyle->hasAutoColumnCount()) {
        count = std::max<LayoutUnit>(1, (availableWidth + columnGap) / (computedColumnWidth + columnGap));
        width = ((availableWidth + columnGap) / count) - columnGap;
    } else {
        count = std::max<LayoutUnit>(std::min<LayoutUnit>(computedColumnCount, (availableWidth + columnGap) / (computedColumnWidth + columnGap)), 1);
        width = ((availableWidth + columnGap) / count) - columnGap;
    }
}

const char* RenderMultiColumnFlowThread::renderName() const
{
    return "RenderMultiColumnFlowThread";
}

void RenderMultiColumnFlowThread::addRegionToThread(RenderRegion* renderRegion)
{
    RenderMultiColumnSet* columnSet = toRenderMultiColumnSet(renderRegion);
    if (RenderMultiColumnSet* nextSet = columnSet->nextSiblingMultiColumnSet()) {
        RenderRegionList::iterator it = m_regionList.find(nextSet);
        ASSERT(it != m_regionList.end());
        m_regionList.insertBefore(it, columnSet);
    } else {
        m_regionList.add(columnSet);
    }
    renderRegion->setIsValid(true);
}

void RenderMultiColumnFlowThread::willBeRemovedFromTree()
{
    // Detach all column sets from the flow thread. Cannot destroy them at this point, since they
    // are siblings of this object, and there may be pointers to this object's sibling somewhere
    // further up on the call stack.
    for (RenderMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
        columnSet->detachRegion();
    multiColumnBlockFlow()->resetMultiColumnFlowThread();
    RenderFlowThread::willBeRemovedFromTree();
}

void RenderMultiColumnFlowThread::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    // We simply remain at our intrinsic height.
    computedValues.m_extent = logicalHeight;
    computedValues.m_position = logicalTop;
}

void RenderMultiColumnFlowThread::updateLogicalWidth()
{
    LayoutUnit columnWidth;
    calculateColumnCountAndWidth(columnWidth, m_columnCount);
    setLogicalWidth(columnWidth);
}

void RenderMultiColumnFlowThread::layout()
{
    RenderFlowThread::layout();
    if (RenderMultiColumnSet* lastSet = lastMultiColumnSet())
        lastSet->expandToEncompassFlowThreadContentsIfNeeded();
}

void RenderMultiColumnFlowThread::setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage)
{
    // Only positive values are interesting (and allowed) here. Zero space shortage may be reported
    // when we're at the top of a column and the element has zero height. Ignore this, and also
    // ignore any negative values, which may occur when we set an early break in order to honor
    // widows in the next column.
    if (spaceShortage <= 0)
        return;

    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(offset)))
        multicolSet->recordSpaceShortage(spaceShortage);
}

void RenderMultiColumnFlowThread::updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight)
{
    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(offset)))
        multicolSet->updateMinimumColumnHeight(minHeight);
}

RenderRegion* RenderMultiColumnFlowThread::regionAtBlockOffset(LayoutUnit /*offset*/) const
{
    // For now there's only one column set, so this is easy:
    return firstMultiColumnSet();
}

bool RenderMultiColumnFlowThread::addForcedRegionBreak(LayoutUnit offset, RenderObject* /*breakChild*/, bool /*isBefore*/, LayoutUnit* offsetBreakAdjustment)
{
    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(offset))) {
        multicolSet->addContentRun(offset);
        if (offsetBreakAdjustment)
            *offsetBreakAdjustment = pageLogicalHeightForOffset(offset) ? pageRemainingLogicalHeightForOffset(offset, IncludePageBoundary) : LayoutUnit();
        return true;
    }
    return false;
}

bool RenderMultiColumnFlowThread::isPageLogicalHeightKnown() const
{
    if (RenderMultiColumnSet* columnSet = lastMultiColumnSet())
        return columnSet->pageLogicalHeight();
    return false;
}

}
