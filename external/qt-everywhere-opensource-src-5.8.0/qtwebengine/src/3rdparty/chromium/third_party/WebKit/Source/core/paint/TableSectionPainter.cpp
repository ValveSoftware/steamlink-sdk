// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableSectionPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutTableRow.h"
#include "core/paint/BoxClipper.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TableCellPainter.h"
#include "core/paint/TableRowPainter.h"
#include <algorithm>

namespace blink {

inline const LayoutTableCell* TableSectionPainter::primaryCellToPaint(unsigned row, unsigned column, const CellSpan& dirtiedRows, const CellSpan& dirtiedColumns) const
{
    DCHECK(row >= dirtiedRows.start() && row < dirtiedRows.end());
    DCHECK(column >= dirtiedColumns.start() && column < dirtiedColumns.end());

    const LayoutTableCell* cell = m_layoutTableSection.primaryCellAt(row, column);
    if (!cell)
        return nullptr;
    // We have painted (row, column) when painting (row - 1, column).
    if (row > dirtiedRows.start() && m_layoutTableSection.primaryCellAt(row - 1, column) == cell)
        return nullptr;
    // We have painted (row, column) when painting (row, column -1).
    if (column > dirtiedColumns.start() && m_layoutTableSection.primaryCellAt(row, column - 1) == cell)
        return nullptr;
    return cell;
}

void TableSectionPainter::paintRepeatingHeaderGroup(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, const CollapsedBorderValue& currentBorderValue, ItemToPaint itemToPaint)
{
    if (!m_layoutTableSection.hasRepeatingHeaderGroup())
        return;

    LayoutTable* table = m_layoutTableSection.table();
    LayoutPoint paginationOffset = paintOffset;
    LayoutUnit pageHeight = table->pageLogicalHeightForOffset(LayoutUnit());

    // Move paginationOffset to the top of the next page.
    // The header may have a pagination strut before it so we need to account for that when establishing its position.
    LayoutUnit headerGroupOffset = table->pageLogicalOffset();
    if (LayoutTableRow* row = m_layoutTableSection.firstRow())
        headerGroupOffset += m_layoutTableSection.paginationStrutForRow(row, table->pageLogicalOffset());
    LayoutUnit offsetToNextPage = pageHeight - intMod(headerGroupOffset, pageHeight);
    paginationOffset.move(0, offsetToNextPage);
    // Now move paginationOffset to the top of the page the cull rect starts on.
    if (paintInfo.cullRect().m_rect.y() > paginationOffset.y())
        paginationOffset.move(0, pageHeight * static_cast<int>((paintInfo.cullRect().m_rect.y() - paginationOffset.y()) / pageHeight));
    LayoutUnit bottomBound = std::min(LayoutUnit(paintInfo.cullRect().m_rect.maxY()), paintOffset.y() + table->logicalHeight());
    while (paginationOffset.y() < bottomBound) {
        LayoutPoint nestedOffset = paginationOffset + LayoutPoint(0, m_layoutTableSection.offsetForRepeatingHeader());
        if (itemToPaint == PaintCollapsedBorders)
            paintCollapsedSectionBorders(paintInfo, nestedOffset, currentBorderValue);
        else
            paintSection(paintInfo, nestedOffset);
        paginationOffset.move(0, pageHeight);
    }
}

void TableSectionPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    paintSection(paintInfo, paintOffset);
    LayoutTable* table = m_layoutTableSection.table();
    if (table->header() == m_layoutTableSection)
        paintRepeatingHeaderGroup(paintInfo, paintOffset, CollapsedBorderValue(), PaintSection);
}

void TableSectionPainter::paintSection(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    DCHECK(!m_layoutTableSection.needsLayout());
    // avoid crashing on bugs that cause us to paint with dirty layout
    if (m_layoutTableSection.needsLayout())
        return;

    unsigned totalRows = m_layoutTableSection.numRows();
    unsigned totalCols = m_layoutTableSection.table()->numEffectiveColumns();

    if (!totalRows || !totalCols)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableSection.location();

    if (paintInfo.phase != PaintPhaseSelfOutlineOnly) {
        Optional<BoxClipper> boxClipper;
        if (paintInfo.phase != PaintPhaseSelfBlockBackgroundOnly)
            boxClipper.emplace(m_layoutTableSection, paintInfo, adjustedPaintOffset, ForceContentsClip);
        paintObject(paintInfo, adjustedPaintOffset);
    }

    if (shouldPaintSelfOutline(paintInfo.phase))
        ObjectPainter(m_layoutTableSection).paintOutline(paintInfo, adjustedPaintOffset);
}

static inline bool compareCellPositions(LayoutTableCell* elem1, LayoutTableCell* elem2)
{
    return elem1->rowIndex() < elem2->rowIndex();
}

// This comparison is used only when we have overflowing cells as we have an unsorted array to sort. We thus need
// to sort both on rows and columns to properly issue paint invalidations.
static inline bool compareCellPositionsWithOverflowingCells(LayoutTableCell* elem1, LayoutTableCell* elem2)
{
    if (elem1->rowIndex() != elem2->rowIndex())
        return elem1->rowIndex() < elem2->rowIndex();

    return elem1->absoluteColumnIndex() < elem2->absoluteColumnIndex();
}

void TableSectionPainter::paintCollapsedBorders(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, const CollapsedBorderValue& currentBorderValue)
{
    paintCollapsedSectionBorders(paintInfo, paintOffset, currentBorderValue);
    LayoutTable* table = m_layoutTableSection.table();
    if (table->header() == m_layoutTableSection)
        paintRepeatingHeaderGroup(paintInfo, paintOffset, currentBorderValue, PaintCollapsedBorders);
}

void TableSectionPainter::paintCollapsedSectionBorders(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, const CollapsedBorderValue& currentBorderValue)
{
    if (!m_layoutTableSection.numRows() || !m_layoutTableSection.table()->effectiveColumns().size())
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableSection.location();
    BoxClipper boxClipper(m_layoutTableSection, paintInfo, adjustedPaintOffset, ForceContentsClip);

    LayoutRect localPaintInvalidationRect = LayoutRect(paintInfo.cullRect().m_rect);
    localPaintInvalidationRect.moveBy(-adjustedPaintOffset);

    LayoutRect tableAlignedRect = m_layoutTableSection.logicalRectForWritingModeAndDirection(localPaintInvalidationRect);

    CellSpan dirtiedRows = m_layoutTableSection.dirtiedRows(tableAlignedRect);
    CellSpan dirtiedColumns = m_layoutTableSection.dirtiedEffectiveColumns(tableAlignedRect);

    if (dirtiedColumns.start() >= dirtiedColumns.end())
        return;

    // Collapsed borders are painted from the bottom right to the top left so that precedence
    // due to cell position is respected.
    for (unsigned r = dirtiedRows.end(); r > dirtiedRows.start(); r--) {
        unsigned row = r - 1;
        for (unsigned c = dirtiedColumns.end(); c > dirtiedColumns.start(); c--) {
            unsigned col = c - 1;
            const LayoutTableSection::CellStruct& current = m_layoutTableSection.cellAt(row, col);
            const LayoutTableCell* cell = current.primaryCell();
            if (!cell || (row > dirtiedRows.start() && m_layoutTableSection.primaryCellAt(row - 1, col) == cell) || (col > dirtiedColumns.start() && m_layoutTableSection.primaryCellAt(row, col - 1) == cell))
                continue;
            LayoutPoint cellPoint = m_layoutTableSection.flipForWritingModeForChild(cell, adjustedPaintOffset);
            TableCellPainter(*cell).paintCollapsedBorders(paintInfo, cellPoint, currentBorderValue);
        }
    }
}

void TableSectionPainter::paintObject(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutRect localPaintInvalidationRect = LayoutRect(paintInfo.cullRect().m_rect);
    localPaintInvalidationRect.moveBy(-paintOffset);

    LayoutRect tableAlignedRect = m_layoutTableSection.logicalRectForWritingModeAndDirection(localPaintInvalidationRect);

    CellSpan dirtiedRows = m_layoutTableSection.dirtiedRows(tableAlignedRect);
    CellSpan dirtiedColumns = m_layoutTableSection.dirtiedEffectiveColumns(tableAlignedRect);

    if (dirtiedColumns.start() >= dirtiedColumns.end())
        return;

    PaintInfo paintInfoForDescendants = paintInfo.forDescendants();

    if (shouldPaintSelfBlockBackground(paintInfo.phase)) {
        paintBoxShadow(paintInfo, paintOffset, Normal);
        for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
            for (unsigned c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
                if (const LayoutTableCell* cell = primaryCellToPaint(r, c, dirtiedRows, dirtiedColumns))
                    paintBackgroundsBehindCell(*cell, paintInfoForDescendants, paintOffset);
            }
        }
        paintBoxShadow(paintInfo, paintOffset, Inset);
    }

    if (paintInfo.phase == PaintPhaseSelfBlockBackgroundOnly)
        return;

    if (shouldPaintDescendantBlockBackgrounds(paintInfo.phase)) {
        for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
            const LayoutTableRow* row = m_layoutTableSection.rowLayoutObjectAt(r);
            // If a row has a layer, we'll paint row background in TableRowPainter.
            if (!row || row->hasSelfPaintingLayer())
                continue;

            TableRowPainter rowPainter(*row);
            rowPainter.paintBoxShadow(paintInfoForDescendants, paintOffset, Normal);
            if (row->hasBackground()) {
                for (unsigned c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
                    if (const LayoutTableCell* cell = primaryCellToPaint(r, c, dirtiedRows, dirtiedColumns))
                        rowPainter.paintBackgroundBehindCell(*cell, paintInfoForDescendants, paintOffset);
                }
            }
            rowPainter.paintBoxShadow(paintInfoForDescendants, paintOffset, Inset);
        }
    }

    const HashSet<LayoutTableCell*>& overflowingCells = m_layoutTableSection.overflowingCells();
    if (!m_layoutTableSection.hasMultipleCellLevels() && overflowingCells.isEmpty()) {
        for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
            const LayoutTableRow* row = m_layoutTableSection.rowLayoutObjectAt(r);
            // TODO(crbug.com/577282): This painting order is inconsistent with other outlines.
            if (row && !row->hasSelfPaintingLayer() && shouldPaintSelfOutline(paintInfoForDescendants.phase))
                TableRowPainter(*row).paintOutline(paintInfoForDescendants, paintOffset);
            for (unsigned c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
                if (const LayoutTableCell * cell = primaryCellToPaint(r, c, dirtiedRows, dirtiedColumns))
                    paintCell(*cell, paintInfoForDescendants, paintOffset);
            }
        }
    } else {
        // The overflowing cells should be scarce to avoid adding a lot of cells to the HashSet.
        DCHECK(overflowingCells.size() < m_layoutTableSection.numRows() * m_layoutTableSection.table()->effectiveColumns().size() * gMaxAllowedOverflowingCellRatioForFastPaintPath);

        // To make sure we properly paint the section, we paint all the overflowing cells that we collected.
        Vector<LayoutTableCell*> cells;
        copyToVector(overflowingCells, cells);

        HashSet<LayoutTableCell*> spanningCells;
        for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
            const LayoutTableRow* row = m_layoutTableSection.rowLayoutObjectAt(r);
            // TODO(crbug.com/577282): This painting order is inconsistent with other outlines.
            if (row && !row->hasSelfPaintingLayer() && shouldPaintSelfOutline(paintInfoForDescendants.phase))
                TableRowPainter(*row).paintOutline(paintInfoForDescendants, paintOffset);
            for (unsigned c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
                const LayoutTableSection::CellStruct& current = m_layoutTableSection.cellAt(r, c);
                for (LayoutTableCell* cell : current.cells) {
                    if (overflowingCells.contains(cell))
                        continue;
                    if (cell->rowSpan() > 1 || cell->colSpan() > 1) {
                        if (!spanningCells.add(cell).isNewEntry)
                            continue;
                    }
                    cells.append(cell);
                }
            }
        }

        // Sort the dirty cells by paint order.
        if (!overflowingCells.size())
            std::stable_sort(cells.begin(), cells.end(), compareCellPositions);
        else
            std::sort(cells.begin(), cells.end(), compareCellPositionsWithOverflowingCells);

        for (const LayoutTableCell* cell : cells)
            paintCell(*cell, paintInfoForDescendants, paintOffset);
    }
}

void TableSectionPainter::paintBackgroundsBehindCell(const LayoutTableCell& cell, const PaintInfo& paintInfoForCells, const LayoutPoint& paintOffset)
{
    LayoutPoint cellPoint = m_layoutTableSection.flipForWritingModeForChild(&cell, paintOffset);

    // We need to handle painting a stack of backgrounds. This stack (from bottom to top) consists of
    // the column group, column, row group, row, and then the cell.

    LayoutTable::ColAndColGroup colAndColGroup = m_layoutTableSection.table()->colElementAtAbsoluteColumn(cell.absoluteColumnIndex());
    LayoutTableCol* column = colAndColGroup.col;
    LayoutTableCol* columnGroup = colAndColGroup.colgroup;
    TableCellPainter tableCellPainter(cell);

    // Column groups and columns first.
    // FIXME: Columns and column groups do not currently support opacity, and they are being painted "too late" in
    // the stack, since we have already opened a transparency layer (potentially) for the table row group.
    // Note that we deliberately ignore whether or not the cell has a layer, since these backgrounds paint "behind" the
    // cell.
    if (columnGroup && columnGroup->hasBackground())
        tableCellPainter.paintContainerBackgroundBehindCell(paintInfoForCells, cellPoint, *columnGroup, DisplayItem::TableCellBackgroundFromColumnGroup);
    if (column && column->hasBackground())
        tableCellPainter.paintContainerBackgroundBehindCell(paintInfoForCells, cellPoint, *column, DisplayItem::TableCellBackgroundFromColumn);

    // Paint the row group next.
    if (m_layoutTableSection.hasBackground())
        tableCellPainter.paintContainerBackgroundBehindCell(paintInfoForCells, cellPoint, m_layoutTableSection, DisplayItem::TableCellBackgroundFromSection);
}

void TableSectionPainter::paintCell(const LayoutTableCell& cell, const PaintInfo& paintInfoForCells, const LayoutPoint& paintOffset)
{
    if (!cell.hasSelfPaintingLayer() && !cell.row()->hasSelfPaintingLayer()) {
        LayoutPoint cellPoint = m_layoutTableSection.flipForWritingModeForChild(&cell, paintOffset);
        cell.paint(paintInfoForCells, cellPoint);
    }
}

void TableSectionPainter::paintBoxShadow(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, ShadowStyle shadowStyle)
{
    DCHECK(shouldPaintSelfBlockBackground(paintInfo.phase));
    if (!m_layoutTableSection.styleRef().boxShadow())
        return;

    DisplayItem::Type type = shadowStyle == Normal ? DisplayItem::TableSectionBoxShadowNormal : DisplayItem::TableSectionBoxShadowInset;
    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutTableSection, type))
        return;

    LayoutRect bounds = BoxPainter(m_layoutTableSection).boundsForDrawingRecorder(paintOffset);
    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutTableSection, type, bounds);
    BoxPainter::paintBoxShadow(paintInfo, LayoutRect(paintOffset, m_layoutTableSection.size()), m_layoutTableSection.styleRef(), shadowStyle);
}

} // namespace blink
