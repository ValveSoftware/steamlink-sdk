// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/GridPainter.h"

#include "core/layout/LayoutGrid.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/PaintInfo.h"
#include <algorithm>

namespace blink {

static GridSpan dirtiedGridAreas(const Vector<LayoutUnit>& coordinates, LayoutUnit start, LayoutUnit end)
{
    // This function does a binary search over the coordinates.
    // This doesn't work with grid items overflowing their grid areas, but that is managed with m_gridItemsOverflowingGridArea.

    size_t startGridAreaIndex = std::upper_bound(coordinates.begin(), coordinates.end() - 1, start) - coordinates.begin();
    if (startGridAreaIndex > 0)
        --startGridAreaIndex;

    size_t endGridAreaIndex = std::upper_bound(coordinates.begin() + startGridAreaIndex, coordinates.end() - 1, end) - coordinates.begin();
    if (endGridAreaIndex > 0)
        --endGridAreaIndex;

    // GridSpan stores lines' indexes (not tracks' indexes).
    return GridSpan::translatedDefiniteGridSpan(startGridAreaIndex, endGridAreaIndex + 1);
}

class GridItemsSorter {
public:
    bool operator()(const std::pair<LayoutBox*, size_t>& firstChild, const std::pair<LayoutBox*, size_t>& secondChild) const
    {
        if (firstChild.first->style()->order() != secondChild.first->style()->order())
            return firstChild.first->style()->order() < secondChild.first->style()->order();

        return firstChild.second < secondChild.second;
    }
};

void GridPainter::paintChildren(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(!m_layoutGrid.needsLayout());

    LayoutRect localPaintInvalidationRect = LayoutRect(paintInfo.cullRect().m_rect);
    localPaintInvalidationRect.moveBy(-paintOffset);

    Vector<LayoutUnit> columnPositions = m_layoutGrid.columnPositions();
    if (!m_layoutGrid.styleRef().isLeftToRightDirection()) {
        // Translate columnPositions in RTL as we need the physical coordinates of the columns in order to call dirtiedGridAreas().
        for (size_t i = 0; i < columnPositions.size(); i++)
            columnPositions[i] = m_layoutGrid.translateRTLCoordinate(columnPositions[i]);
        // We change the order of tracks in columnPositions, as in RTL the leftmost track will be the last one.
        std::sort(columnPositions.begin(), columnPositions.end());
    }

    GridSpan dirtiedColumns = dirtiedGridAreas(columnPositions, localPaintInvalidationRect.x(), localPaintInvalidationRect.maxX());
    GridSpan dirtiedRows = dirtiedGridAreas(m_layoutGrid.rowPositions(), localPaintInvalidationRect.y(), localPaintInvalidationRect.maxY());

    if (!m_layoutGrid.styleRef().isLeftToRightDirection()) {
        // As we changed the order of tracks previously, we need to swap the dirtied columns in RTL.
        size_t lastLine = columnPositions.size() - 1;
        dirtiedColumns = GridSpan::translatedDefiniteGridSpan(lastLine - dirtiedColumns.endLine(), lastLine - dirtiedColumns.startLine());
    }

    Vector<std::pair<LayoutBox*, size_t>> gridItemsToBePainted;

    for (const auto& row : dirtiedRows) {
        for (const auto& column : dirtiedColumns) {
            const Vector<LayoutBox*, 1>& children = m_layoutGrid.gridCell(row, column);
            for (auto* child : children)
                gridItemsToBePainted.append(std::make_pair(child, m_layoutGrid.paintIndexForGridItem(child)));
        }
    }

    for (auto* item: m_layoutGrid.itemsOverflowingGridArea()) {
        if (item->frameRect().intersects(localPaintInvalidationRect))
            gridItemsToBePainted.append(std::make_pair(item, m_layoutGrid.paintIndexForGridItem(item)));
    }

    // Sort grid items following order-modified document order.
    // See http://www.w3.org/TR/css-flexbox/#order-modified-document-order
    std::stable_sort(gridItemsToBePainted.begin(), gridItemsToBePainted.end(), GridItemsSorter());

    LayoutBox* previous = 0;
    for (const auto& gridItemAndPaintIndex : gridItemsToBePainted) {
        // We might have duplicates because of spanning children are included in all cells they span.
        // Skip them here to avoid painting items several times.
        LayoutBox* current = gridItemAndPaintIndex.first;
        if (current == previous)
            continue;

        BlockPainter(m_layoutGrid).paintAllChildPhasesAtomically(*current, paintInfo, paintOffset);
        previous = current;
    }
}

} // namespace blink
