// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/MultiColumnFragmentainerGroup.h"

#include "core/layout/ColumnBalancer.h"
#include "core/layout/FragmentationContext.h"
#include "core/layout/LayoutMultiColumnSet.h"

namespace blink {

MultiColumnFragmentainerGroup::MultiColumnFragmentainerGroup(const LayoutMultiColumnSet& columnSet)
    : m_columnSet(columnSet)
{
}

bool MultiColumnFragmentainerGroup::isFirstGroup() const
{
    return &m_columnSet.firstFragmentainerGroup() == this;
}

bool MultiColumnFragmentainerGroup::isLastGroup() const
{
    return &m_columnSet.lastFragmentainerGroup() == this;
}

LayoutSize MultiColumnFragmentainerGroup::offsetFromColumnSet() const
{
    LayoutSize offset(LayoutUnit(), logicalTop());
    if (!m_columnSet.flowThread()->isHorizontalWritingMode())
        return offset.transposedSize();
    return offset;
}

LayoutUnit MultiColumnFragmentainerGroup::blockOffsetInEnclosingFragmentationContext() const
{
    return logicalTop() + m_columnSet.logicalTopFromMulticolContentEdge() + m_columnSet.multiColumnFlowThread()->blockOffsetInEnclosingFragmentationContext();
}

void MultiColumnFragmentainerGroup::resetColumnHeight()
{
    m_maxColumnHeight = calculateMaxColumnHeight();

    LayoutMultiColumnFlowThread* flowThread = m_columnSet.multiColumnFlowThread();
    if (m_columnSet.heightIsAuto()) {
        FragmentationContext* enclosingFragmentationContext = flowThread->enclosingFragmentationContext();
        if (enclosingFragmentationContext && enclosingFragmentationContext->isFragmentainerLogicalHeightKnown()) {
            // Even if height is auto, we set an initial height, in order to tell how much content
            // this MultiColumnFragmentainerGroup can hold, and when we need to append a new one.
            m_columnHeight = m_maxColumnHeight;
        } else {
            m_columnHeight = LayoutUnit();
        }
    } else {
        setAndConstrainColumnHeight(heightAdjustedForRowOffset(flowThread->columnHeightAvailable()));
    }
}

bool MultiColumnFragmentainerGroup::recalculateColumnHeight(LayoutMultiColumnSet& columnSet)
{
    LayoutUnit oldColumnHeight = m_columnHeight;

    m_maxColumnHeight = calculateMaxColumnHeight();

    // Only the last row may have auto height, and thus be balanced. There are no good reasons to
    // balance the preceding rows, and that could potentially lead to an insane number of layout
    // passes as well.
    if (isLastGroup() && columnSet.heightIsAuto()) {
        LayoutUnit newColumnHeight;
        if (!columnSet.isInitialHeightCalculated()) {
            // Initial balancing: Start with the lowest imaginable column height. Also calculate the
            // height of the tallest piece of unbreakable content. Columns should never get any
            // shorter than that (unless constrained by max-height). Propagate this to our
            // containing column set, in case there is an outer multicol container that also needs
            // to balance. After having calculated the initial column height, the multicol container
            // needs another layout pass with the column height that we just calculated.
            InitialColumnHeightFinder initialHeightFinder(columnSet, logicalTopInFlowThread(), logicalBottomInFlowThread());
            LayoutUnit tallestUnbreakableLogicalHeight = initialHeightFinder.tallestUnbreakableLogicalHeight();
            columnSet.propagateTallestUnbreakableLogicalHeight(tallestUnbreakableLogicalHeight);
            newColumnHeight = std::max(initialHeightFinder.initialMinimalBalancedHeight(), tallestUnbreakableLogicalHeight);
        } else {
            // Rebalancing: After having laid out again, we'll need to rebalance if the height
            // wasn't enough and we're allowed to stretch it, and then re-lay out. There are further
            // details on the column balancing machinery in ColumnBalancer and its derivates.
            newColumnHeight = rebalanceColumnHeightIfNeeded();
        }
        setAndConstrainColumnHeight(newColumnHeight);
    } else {
        // The position of the column set may have changed, in which case height available for
        // columns may have changed as well.
        setAndConstrainColumnHeight(m_columnHeight);
    }

    if (m_columnHeight == oldColumnHeight)
        return false; // No change. We're done.

    return true; // Need another pass.
}

LayoutSize MultiColumnFragmentainerGroup::flowThreadTranslationAtOffset(LayoutUnit offsetInFlowThread, CoordinateSpaceConversion mode) const
{
    LayoutMultiColumnFlowThread* flowThread = m_columnSet.multiColumnFlowThread();
    unsigned columnIndex = columnIndexAtOffset(offsetInFlowThread);
    LayoutRect portionRect(flowThreadPortionRectAt(columnIndex));
    flowThread->flipForWritingMode(portionRect);
    LayoutRect columnRect(columnRectAt(columnIndex));
    m_columnSet.flipForWritingMode(columnRect);
    LayoutSize translationRelativeToGroup = columnRect.location() - portionRect.location();
    LayoutSize translationRelativeToFlowThread = translationRelativeToGroup + offsetFromColumnSet() + m_columnSet.topLeftLocationOffset() - flowThread->topLeftLocationOffset();
    if (mode == CoordinateSpaceConversion::Containing)
        return translationRelativeToFlowThread;

    LayoutSize enclosingTranslation;
    if (LayoutMultiColumnFlowThread* enclosingFlowThread = flowThread->enclosingFlowThread()) {
        const MultiColumnFragmentainerGroup& firstRow = flowThread->firstMultiColumnSet()->firstFragmentainerGroup();
        // Translation that would map points in the coordinate space of the outermost flow thread to
        // visual points in the first column in the first fragmentainer group (row) in our multicol
        // container.
        LayoutSize enclosingTranslationOrigin = enclosingFlowThread->flowThreadTranslationAtOffset(firstRow.blockOffsetInEnclosingFragmentationContext(), mode);

        // Translation that would map points in the coordinate space of the outermost flow thread to
        // visual points in the first column in this fragmentainer group.
        enclosingTranslation = enclosingFlowThread->flowThreadTranslationAtOffset(blockOffsetInEnclosingFragmentationContext(), mode);

        // What we ultimately return from this method is a translation that maps points in the
        // coordinate space of our flow thread to a visual point in a certain column in this
        // fragmentainer group. We had to go all the way up to the outermost flow thread, since this
        // fragmentainer group may be in a different outer column than the first outer column that
        // this multicol container lives in. It's the visual distance between the first
        // fragmentainer group and this fragmentainer group that we need to add to the translation.
        enclosingTranslation -= enclosingTranslationOrigin;
    }

    return enclosingTranslation + translationRelativeToFlowThread;
}

LayoutUnit MultiColumnFragmentainerGroup::columnLogicalTopForOffset(LayoutUnit offsetInFlowThread) const
{
    unsigned columnIndex = columnIndexAtOffset(offsetInFlowThread, AssumeNewColumns);
    return logicalTopInFlowThreadAt(columnIndex);
}

LayoutPoint MultiColumnFragmentainerGroup::visualPointToFlowThreadPoint(const LayoutPoint& visualPoint) const
{
    unsigned columnIndex = columnIndexAtVisualPoint(visualPoint);
    LayoutRect columnRect = columnRectAt(columnIndex);
    LayoutPoint localPoint(visualPoint);
    localPoint.moveBy(-columnRect.location());
    // Before converting to a flow thread position, if the block direction coordinate is outside the
    // column, snap to the bounds of the column, and reset the inline direction coordinate to the
    // start position in the column. The effect of this is that if the block position is before the
    // column rectangle, we'll get to the beginning of this column, while if the block position is
    // after the column rectangle, we'll get to the beginning of the next column.
    if (!m_columnSet.isHorizontalWritingMode()) {
        LayoutUnit columnStart = m_columnSet.style()->isLeftToRightDirection() ? LayoutUnit() : columnRect.height();
        if (localPoint.x() < 0)
            localPoint = LayoutPoint(LayoutUnit(), columnStart);
        else if (localPoint.x() > logicalHeight())
            localPoint = LayoutPoint(logicalHeight(), columnStart);
        return LayoutPoint(localPoint.x() + logicalTopInFlowThreadAt(columnIndex), localPoint.y());
    }
    LayoutUnit columnStart = m_columnSet.style()->isLeftToRightDirection() ? LayoutUnit() : columnRect.width();
    if (localPoint.y() < 0)
        localPoint = LayoutPoint(columnStart, LayoutUnit());
    else if (localPoint.y() > logicalHeight())
        localPoint = LayoutPoint(columnStart, logicalHeight());
    return LayoutPoint(localPoint.x(), localPoint.y() + logicalTopInFlowThreadAt(columnIndex));
}

LayoutRect MultiColumnFragmentainerGroup::fragmentsBoundingBox(const LayoutRect& boundingBoxInFlowThread) const
{
    // Find the start and end column intersected by the bounding box.
    LayoutRect flippedBoundingBoxInFlowThread(boundingBoxInFlowThread);
    LayoutFlowThread* flowThread = m_columnSet.flowThread();
    flowThread->flipForWritingMode(flippedBoundingBoxInFlowThread);
    bool isHorizontalWritingMode = m_columnSet.isHorizontalWritingMode();
    LayoutUnit boundingBoxLogicalTop = isHorizontalWritingMode ? flippedBoundingBoxInFlowThread.y() : flippedBoundingBoxInFlowThread.x();
    LayoutUnit boundingBoxLogicalBottom = isHorizontalWritingMode ? flippedBoundingBoxInFlowThread.maxY() : flippedBoundingBoxInFlowThread.maxX();
    if (boundingBoxLogicalBottom <= logicalTopInFlowThread() || boundingBoxLogicalTop >= logicalBottomInFlowThread())
        return LayoutRect(); // The bounding box doesn't intersect this fragmentainer group.
    unsigned startColumn;
    unsigned endColumn;
    columnIntervalForBlockRangeInFlowThread(boundingBoxLogicalTop, boundingBoxLogicalBottom, startColumn, endColumn);

    LayoutRect startColumnFlowThreadOverflowPortion = flowThreadPortionOverflowRectAt(startColumn);
    flowThread->flipForWritingMode(startColumnFlowThreadOverflowPortion);
    LayoutRect startColumnRect(boundingBoxInFlowThread);
    startColumnRect.intersect(startColumnFlowThreadOverflowPortion);
    startColumnRect.move(flowThreadTranslationAtOffset(logicalTopInFlowThreadAt(startColumn), CoordinateSpaceConversion::Containing));
    if (startColumn == endColumn)
        return startColumnRect; // It all takes place in one column. We're done.

    LayoutRect endColumnFlowThreadOverflowPortion = flowThreadPortionOverflowRectAt(endColumn);
    flowThread->flipForWritingMode(endColumnFlowThreadOverflowPortion);
    LayoutRect endColumnRect(boundingBoxInFlowThread);
    endColumnRect.intersect(endColumnFlowThreadOverflowPortion);
    endColumnRect.move(flowThreadTranslationAtOffset(logicalTopInFlowThreadAt(endColumn), CoordinateSpaceConversion::Containing));
    return unionRect(startColumnRect, endColumnRect);
}

void MultiColumnFragmentainerGroup::collectLayerFragments(PaintLayerFragments& fragments, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect) const
{
    // |layerBoundingBox| is in the flow thread coordinate space, relative to the top/left edge of
    // the flow thread, but note that it has been converted with respect to writing mode (so that
    // it's visual/physical in that sense).
    //
    // |dirtyRect| is visual, relative to the multicol container.
    //
    // Then there's the output from this method - the stuff we put into the list of fragments. The
    // fragment.paginationOffset point is the actual visual translation required to get from a
    // location in the flow thread to a location in a given column. The fragment.paginationClip
    // rectangle, on the other hand, is in flow thread coordinates, but otherwise completely
    // physical in terms of writing mode.

    LayoutMultiColumnFlowThread* flowThread = m_columnSet.multiColumnFlowThread();
    bool isHorizontalWritingMode = m_columnSet.isHorizontalWritingMode();

    // Put the layer bounds into flow thread-local coordinates by flipping it first. Since we're in
    // a layoutObject, most rectangles are represented this way.
    LayoutRect layerBoundsInFlowThread(layerBoundingBox);
    flowThread->flipForWritingMode(layerBoundsInFlowThread);

    // Now we can compare with the flow thread portions owned by each column. First let's
    // see if the rect intersects our flow thread portion at all.
    LayoutRect clippedRect(layerBoundsInFlowThread);
    clippedRect.intersect(m_columnSet.flowThreadPortionOverflowRect());
    if (clippedRect.isEmpty())
        return;

    // Now we know we intersect at least one column. Let's figure out the logical top and logical
    // bottom of the area we're checking.
    LayoutUnit layerLogicalTop = isHorizontalWritingMode ? layerBoundsInFlowThread.y() : layerBoundsInFlowThread.x();
    LayoutUnit layerLogicalBottom = (isHorizontalWritingMode ? layerBoundsInFlowThread.maxY() : layerBoundsInFlowThread.maxX());

    // Figure out the start and end columns for the layer and only check within that range so that
    // we don't walk the entire column row.
    unsigned startColumn;
    unsigned endColumn;
    columnIntervalForBlockRangeInFlowThread(layerLogicalTop, layerLogicalBottom, startColumn, endColumn);

    // Now intersect with the columns actually occupied by the dirty rect, to narrow it down even further.
    unsigned firstColumnInDirtyRect, lastColumnInDirtyRect;
    columnIntervalForVisualRect(dirtyRect, firstColumnInDirtyRect, lastColumnInDirtyRect);
    if (firstColumnInDirtyRect > endColumn || lastColumnInDirtyRect < startColumn)
        return; // The two column intervals are disjoint. There's nothing to collect.
    if (startColumn < firstColumnInDirtyRect)
        startColumn = firstColumnInDirtyRect;
    if (endColumn > lastColumnInDirtyRect)
        endColumn = lastColumnInDirtyRect;
    ASSERT(endColumn >= startColumn);

    for (unsigned i = startColumn; i <= endColumn; i++) {
        PaintLayerFragment fragment;

        // Set the physical translation offset.
        fragment.paginationOffset = toLayoutPoint(flowThreadTranslationAtOffset(logicalTopInFlowThreadAt(i), CoordinateSpaceConversion::Visual));

        // Set the overflow clip rect that corresponds to the column.
        fragment.paginationClip = flowThreadPortionOverflowRectAt(i);
        // Flip it into more a physical (PaintLayer-style) rectangle.
        flowThread->flipForWritingMode(fragment.paginationClip);

        fragments.append(fragment);
    }
}

LayoutRect MultiColumnFragmentainerGroup::calculateOverflow() const
{
    // Note that we just return the bounding rectangle of the column boxes here. We currently don't
    // examine overflow caused by the actual content that ends up in each column.
    LayoutRect overflowRect;
    if (unsigned columnCount = actualColumnCount()) {
        overflowRect = columnRectAt(0);
        if (columnCount > 1)
            overflowRect.uniteEvenIfEmpty(columnRectAt(columnCount - 1));
    }
    return overflowRect;
}

unsigned MultiColumnFragmentainerGroup::actualColumnCount() const
{
    // We must always return a value of 1 or greater. Column count = 0 is a meaningless situation,
    // and will confuse and cause problems in other parts of the code.
    if (!m_columnHeight)
        return 1;

    // Our flow thread portion determines our column count. We have as many columns as needed to fit
    // all the content.
    LayoutUnit flowThreadPortionHeight = logicalHeightInFlowThread();
    if (!flowThreadPortionHeight)
        return 1;

    unsigned count = (flowThreadPortionHeight / m_columnHeight).floor();
    // flowThreadPortionHeight may be saturated, so detect the remainder manually.
    if (count * m_columnHeight < flowThreadPortionHeight)
        count++;
    ASSERT(count >= 1);
    return count;
}

LayoutUnit MultiColumnFragmentainerGroup::heightAdjustedForRowOffset(LayoutUnit height) const
{
    // Let's avoid zero height, as that would cause an infinite amount of columns to be created.
    return std::max(height - logicalTop() - m_columnSet.logicalTopFromMulticolContentEdge(), LayoutUnit(1));
}

LayoutUnit MultiColumnFragmentainerGroup::calculateMaxColumnHeight() const
{
    LayoutMultiColumnFlowThread* flowThread = m_columnSet.multiColumnFlowThread();
    LayoutUnit maxColumnHeight = flowThread->maxColumnLogicalHeight();
    LayoutUnit maxHeight = heightAdjustedForRowOffset(maxColumnHeight);
    if (FragmentationContext* enclosingFragmentationContext = flowThread->enclosingFragmentationContext()) {
        if (enclosingFragmentationContext->isFragmentainerLogicalHeightKnown()) {
            // We're nested inside another fragmentation context whose fragmentainer heights are
            // known. This constrains the max height.
            LayoutUnit remainingOuterLogicalHeight = enclosingFragmentationContext->remainingLogicalHeightAt(blockOffsetInEnclosingFragmentationContext());
            ASSERT(remainingOuterLogicalHeight > 0);
            if (maxHeight > remainingOuterLogicalHeight)
                maxHeight = remainingOuterLogicalHeight;
        }
    }
    return maxHeight;
}

void MultiColumnFragmentainerGroup::setAndConstrainColumnHeight(LayoutUnit newHeight)
{
    m_columnHeight = newHeight;
    if (m_columnHeight > m_maxColumnHeight)
        m_columnHeight = m_maxColumnHeight;
}

LayoutUnit MultiColumnFragmentainerGroup::rebalanceColumnHeightIfNeeded() const
{
    if (actualColumnCount() <= m_columnSet.usedColumnCount()) {
        // With the current column height, the content fits without creating overflowing columns. We're done.
        return m_columnHeight;
    }

    if (m_columnHeight >= m_maxColumnHeight) {
        // We cannot stretch any further. We'll just have to live with the overflowing columns. This
        // typically happens if the max column height is less than the height of the tallest piece
        // of unbreakable content (e.g. lines).
        return m_columnHeight;
    }

    MinimumSpaceShortageFinder shortageFinder(columnSet(), logicalTopInFlowThread(), logicalBottomInFlowThread());

    if (shortageFinder.forcedBreaksCount() + 1 >= m_columnSet.usedColumnCount()) {
        // Too many forced breaks to allow any implicit breaks. Initial balancing should already
        // have set a good height. There's nothing more we should do.
        return m_columnHeight;
    }

    // If the initial guessed column height wasn't enough, stretch it now. Stretch by the lowest
    // amount of space.
    LayoutUnit minSpaceShortage = shortageFinder.minimumSpaceShortage();

    ASSERT(minSpaceShortage > 0); // We should never _shrink_ the height!
    ASSERT(minSpaceShortage != LayoutUnit::max()); // If this happens, we probably have a bug.
    if (minSpaceShortage == LayoutUnit::max())
        return m_columnHeight; // So bail out rather than looping infinitely.

    return m_columnHeight + minSpaceShortage;
}

LayoutRect MultiColumnFragmentainerGroup::columnRectAt(unsigned columnIndex) const
{
    LayoutUnit columnLogicalWidth = m_columnSet.pageLogicalWidth();
    LayoutUnit columnLogicalHeight = m_columnHeight;
    LayoutUnit columnLogicalTop;
    LayoutUnit columnLogicalLeft;
    LayoutUnit columnGap = m_columnSet.columnGap();
    LayoutUnit portionOutsideFlowThread = logicalTopInFlowThread() + (columnIndex + 1) * columnLogicalHeight - logicalBottomInFlowThread();
    if (portionOutsideFlowThread > 0) {
        // The last column may not be using all available space.
        ASSERT(columnIndex + 1 == actualColumnCount());
        columnLogicalHeight -= portionOutsideFlowThread;
        ASSERT(columnLogicalHeight >= 0);
    }

    if (m_columnSet.multiColumnFlowThread()->progressionIsInline()) {
        if (m_columnSet.style()->isLeftToRightDirection())
            columnLogicalLeft += columnIndex * (columnLogicalWidth + columnGap);
        else
            columnLogicalLeft += m_columnSet.contentLogicalWidth() - columnLogicalWidth - columnIndex * (columnLogicalWidth + columnGap);
    } else {
        columnLogicalTop += columnIndex * (m_columnHeight + columnGap);
    }

    LayoutRect columnRect(columnLogicalLeft, columnLogicalTop, columnLogicalWidth, columnLogicalHeight);
    if (!m_columnSet.isHorizontalWritingMode())
        return columnRect.transposedRect();
    return columnRect;
}

LayoutRect MultiColumnFragmentainerGroup::flowThreadPortionRectAt(unsigned columnIndex) const
{
    LayoutUnit logicalTop = logicalTopInFlowThreadAt(columnIndex);
    LayoutUnit logicalBottom = logicalTop + m_columnHeight;
    if (logicalBottom > logicalBottomInFlowThread()) {
        // The last column may not be using all available space.
        ASSERT(columnIndex + 1 == actualColumnCount());
        logicalBottom = logicalBottomInFlowThread();
        ASSERT(logicalBottom >= logicalTop);
    }
    LayoutUnit portionLogicalHeight = logicalBottom - logicalTop;
    if (m_columnSet.isHorizontalWritingMode())
        return LayoutRect(LayoutUnit(), logicalTop, m_columnSet.pageLogicalWidth(), portionLogicalHeight);
    return LayoutRect(logicalTop, LayoutUnit(), portionLogicalHeight, m_columnSet.pageLogicalWidth());
}

LayoutRect MultiColumnFragmentainerGroup::flowThreadPortionOverflowRectAt(unsigned columnIndex) const
{
    // This function determines the portion of the flow thread that paints for the column. Along the inline axis, columns are
    // unclipped at outside edges (i.e., the first and last column in the set), and they clip to half the column
    // gap along interior edges.
    //
    // In the block direction, we will not clip overflow out of the top of the first column, or out of the bottom of
    // the last column. This applies only to the true first column and last column across all column sets.
    //
    // FIXME: Eventually we will know overflow on a per-column basis, but we can't do this until we have a painting
    // mode that understands not to paint contents from a previous column in the overflow area of a following column.
    bool isFirstColumnInRow = !columnIndex;
    bool isLastColumnInRow = columnIndex == actualColumnCount() - 1;
    bool isLTR = m_columnSet.style()->isLeftToRightDirection();
    bool isLeftmostColumn = isLTR ? isFirstColumnInRow : isLastColumnInRow;
    bool isRightmostColumn = isLTR ? isLastColumnInRow : isFirstColumnInRow;

    LayoutRect portionRect = flowThreadPortionRectAt(columnIndex);
    bool isFirstColumnInMulticolContainer = isFirstColumnInRow && this == &m_columnSet.firstFragmentainerGroup() && !m_columnSet.previousSiblingMultiColumnSet();
    bool isLastColumnInMulticolContainer = isLastColumnInRow && this == &m_columnSet.lastFragmentainerGroup() && !m_columnSet.nextSiblingMultiColumnSet();
    // Calculate the overflow rectangle, based on the flow thread's, clipped at column logical
    // top/bottom unless it's the first/last column.
    LayoutRect overflowRect = m_columnSet.overflowRectForFlowThreadPortion(portionRect, isFirstColumnInMulticolContainer, isLastColumnInMulticolContainer);

    // Avoid overflowing into neighboring columns, by clipping in the middle of adjacent column
    // gaps. Also make sure that we avoid rounding errors.
    LayoutUnit columnGap = m_columnSet.columnGap();
    if (m_columnSet.isHorizontalWritingMode()) {
        if (!isLeftmostColumn)
            overflowRect.shiftXEdgeTo(portionRect.x() - columnGap / 2);
        if (!isRightmostColumn)
            overflowRect.shiftMaxXEdgeTo(portionRect.maxX() + columnGap - columnGap / 2);
    } else {
        if (!isLeftmostColumn)
            overflowRect.shiftYEdgeTo(portionRect.y() - columnGap / 2);
        if (!isRightmostColumn)
            overflowRect.shiftMaxYEdgeTo(portionRect.maxY() + columnGap - columnGap / 2);
    }
    return overflowRect;
}

unsigned MultiColumnFragmentainerGroup::columnIndexAtOffset(LayoutUnit offsetInFlowThread, ColumnIndexCalculationMode mode) const
{
    // Handle the offset being out of range.
    if (offsetInFlowThread < m_logicalTopInFlowThread)
        return 0;
    // If we're laying out right now, we cannot constrain against some logical bottom, since it
    // isn't known yet. Otherwise, just return the last column if we're past the logical bottom.
    if (mode == ClampToExistingColumns) {
        if (offsetInFlowThread >= m_logicalBottomInFlowThread)
            return actualColumnCount() - 1;
    }

    if (m_columnHeight)
        return ((offsetInFlowThread - m_logicalTopInFlowThread) / m_columnHeight).floor();
    return 0;
}

unsigned MultiColumnFragmentainerGroup::columnIndexAtVisualPoint(const LayoutPoint& visualPoint) const
{
    bool isColumnProgressionInline = m_columnSet.multiColumnFlowThread()->progressionIsInline();
    bool isHorizontalWritingMode = m_columnSet.isHorizontalWritingMode();
    LayoutUnit columnLengthInColumnProgressionDirection = isColumnProgressionInline ? m_columnSet.pageLogicalWidth() : logicalHeight();
    LayoutUnit offsetInColumnProgressionDirection = isHorizontalWritingMode == isColumnProgressionInline ? visualPoint.x() : visualPoint.y();
    if (!m_columnSet.style()->isLeftToRightDirection() && isColumnProgressionInline)
        offsetInColumnProgressionDirection = m_columnSet.logicalWidth() - offsetInColumnProgressionDirection;
    LayoutUnit columnGap = m_columnSet.columnGap();
    if (columnLengthInColumnProgressionDirection + columnGap <= 0)
        return 0;
    // Column boundaries are in the middle of the column gap.
    int index = (offsetInColumnProgressionDirection + columnGap / 2) / (columnLengthInColumnProgressionDirection + columnGap);
    if (index < 0)
        return 0;
    return std::min(unsigned(index), actualColumnCount() - 1);
}

void MultiColumnFragmentainerGroup::columnIntervalForBlockRangeInFlowThread(LayoutUnit logicalTopInFlowThread, LayoutUnit logicalBottomInFlowThread, unsigned& firstColumn, unsigned& lastColumn) const
{
    ASSERT(logicalTopInFlowThread <= logicalBottomInFlowThread);
    firstColumn = columnIndexAtOffset(logicalTopInFlowThread);
    lastColumn = columnIndexAtOffset(logicalBottomInFlowThread);
    // logicalBottomInFlowThread is an exclusive endpoint, so some additional adjustments may be necessary.
    if (lastColumn > firstColumn && logicalTopInFlowThreadAt(lastColumn) == logicalBottomInFlowThread)
        lastColumn--;
}

void MultiColumnFragmentainerGroup::columnIntervalForVisualRect(const LayoutRect& rect, unsigned& firstColumn, unsigned& lastColumn) const
{
    bool isColumnProgressionInline = m_columnSet.multiColumnFlowThread()->progressionIsInline();
    bool isFlippedColumnProgression = !m_columnSet.style()->isLeftToRightDirection() && isColumnProgressionInline;
    if (m_columnSet.isHorizontalWritingMode() == isColumnProgressionInline) {
        if (isFlippedColumnProgression) {
            firstColumn = columnIndexAtVisualPoint(rect.maxXMinYCorner());
            lastColumn = columnIndexAtVisualPoint(rect.minXMinYCorner());
        } else {
            firstColumn = columnIndexAtVisualPoint(rect.minXMinYCorner());
            lastColumn = columnIndexAtVisualPoint(rect.maxXMinYCorner());
        }
    } else {
        if (isFlippedColumnProgression) {
            firstColumn = columnIndexAtVisualPoint(rect.minXMaxYCorner());
            lastColumn = columnIndexAtVisualPoint(rect.minXMinYCorner());
        } else {
            firstColumn = columnIndexAtVisualPoint(rect.minXMinYCorner());
            lastColumn = columnIndexAtVisualPoint(rect.minXMaxYCorner());
        }
    }
    ASSERT(firstColumn <= lastColumn);
}

MultiColumnFragmentainerGroupList::MultiColumnFragmentainerGroupList(LayoutMultiColumnSet& columnSet)
    : m_columnSet(columnSet)
{
    append(MultiColumnFragmentainerGroup(m_columnSet));
}

// An explicit empty destructor of MultiColumnFragmentainerGroupList should be in
// MultiColumnFragmentainerGroup.cpp, because if an implicit destructor is used,
// msvc 2015 tries to generate its destructor (because the class is dll-exported class)
// and causes a compile error because of lack of MultiColumnFragmentainerGroup::operator=.
// Since MultiColumnFragmentainerGroup is non-copyable, we cannot define the operator=.
MultiColumnFragmentainerGroupList::~MultiColumnFragmentainerGroupList()
{
}

MultiColumnFragmentainerGroup& MultiColumnFragmentainerGroupList::addExtraGroup()
{
    append(MultiColumnFragmentainerGroup(m_columnSet));
    return last();
}

void MultiColumnFragmentainerGroupList::deleteExtraGroups()
{
    shrink(1);
}

} // namespace blink
