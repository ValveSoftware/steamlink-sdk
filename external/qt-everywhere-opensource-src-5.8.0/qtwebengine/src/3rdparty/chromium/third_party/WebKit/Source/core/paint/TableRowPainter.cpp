// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableRowPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableRow.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TableCellPainter.h"

namespace blink {

void TableRowPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    DCHECK(m_layoutTableRow.hasSelfPaintingLayer());

    // TODO(crbug.com/577282): This painting order is inconsistent with other outlines.
    if (shouldPaintSelfOutline(paintInfo.phase))
        paintOutline(paintInfo, paintOffset);
    if (paintInfo.phase == PaintPhaseSelfOutlineOnly)
        return;

    PaintInfo paintInfoForCells = paintInfo.forDescendants();
    if (shouldPaintSelfBlockBackground(paintInfo.phase)) {
        paintBoxShadow(paintInfo, paintOffset, Normal);
        if (m_layoutTableRow.hasBackground()) {
            // Paint row background of behind the cells.
            for (LayoutTableCell* cell = m_layoutTableRow.firstCell(); cell; cell = cell->nextCell())
                TableCellPainter(*cell).paintContainerBackgroundBehindCell(paintInfoForCells, paintOffset, m_layoutTableRow, DisplayItem::TableCellBackgroundFromRow);
        }
        paintBoxShadow(paintInfo, paintOffset, Inset);
    }

    if (paintInfo.phase == PaintPhaseSelfBlockBackgroundOnly)
        return;

    for (LayoutTableCell* cell = m_layoutTableRow.firstCell(); cell; cell = cell->nextCell()) {
        if (!cell->hasSelfPaintingLayer())
            cell->paint(paintInfoForCells, paintOffset);
    }
}

void TableRowPainter::paintOutline(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    DCHECK(shouldPaintSelfOutline(paintInfo.phase));
    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableRow.location();
    ObjectPainter(m_layoutTableRow).paintOutline(paintInfo, adjustedPaintOffset);
}

void TableRowPainter::paintBoxShadow(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, ShadowStyle shadowStyle)
{
    DCHECK(shouldPaintSelfBlockBackground(paintInfo.phase));
    if (!m_layoutTableRow.styleRef().boxShadow())
        return;

    DisplayItem::Type type = shadowStyle == Normal ? DisplayItem::TableRowBoxShadowNormal : DisplayItem::TableRowBoxShadowInset;
    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutTableRow, type))
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableRow.location();
    LayoutRect bounds = BoxPainter(m_layoutTableRow).boundsForDrawingRecorder(adjustedPaintOffset);
    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutTableRow, type, bounds);
    BoxPainter::paintBoxShadow(paintInfo, LayoutRect(adjustedPaintOffset, m_layoutTableRow.size()), m_layoutTableRow.styleRef(), shadowStyle);
}

void TableRowPainter::paintBackgroundBehindCell(const LayoutTableCell& cell, const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    DCHECK(m_layoutTableRow.hasBackground() && !m_layoutTableRow.hasSelfPaintingLayer());
    LayoutPoint cellPoint = m_layoutTableRow.section()->flipForWritingModeForChild(&cell, paintOffset);
    TableCellPainter(cell).paintContainerBackgroundBehindCell(paintInfo, cellPoint, m_layoutTableRow, DisplayItem::TableCellBackgroundFromRow);
}

} // namespace blink
