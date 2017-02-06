// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableCellPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

static const CollapsedBorderValue& collapsedLeftBorder(const ComputedStyle& styleForCellFlow, const LayoutTableCell::CollapsedBorderValues& values)
{
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isLeftToRightDirection() ? values.startBorder : values.endBorder;
    return styleForCellFlow.isFlippedBlocksWritingMode() ? values.afterBorder : values.beforeBorder;
}

static const CollapsedBorderValue& collapsedRightBorder(const ComputedStyle& styleForCellFlow, const LayoutTableCell::CollapsedBorderValues& values)
{
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isLeftToRightDirection() ? values.endBorder : values.startBorder;
    return styleForCellFlow.isFlippedBlocksWritingMode() ? values.beforeBorder : values.afterBorder;
}

static const CollapsedBorderValue& collapsedTopBorder(const ComputedStyle& styleForCellFlow, const LayoutTableCell::CollapsedBorderValues& values)
{
    if (styleForCellFlow.isHorizontalWritingMode())
        return values.beforeBorder;
    return styleForCellFlow.isLeftToRightDirection() ? values.startBorder : values.endBorder;
}

static const CollapsedBorderValue& collapsedBottomBorder(const ComputedStyle& styleForCellFlow, const LayoutTableCell::CollapsedBorderValues& values)
{
    if (styleForCellFlow.isHorizontalWritingMode())
        return values.afterBorder;
    return styleForCellFlow.isLeftToRightDirection() ? values.endBorder : values.startBorder;
}

void TableCellPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    BlockPainter(m_layoutTableCell).paint(paintInfo, paintOffset);
}

static EBorderStyle collapsedBorderStyle(EBorderStyle style)
{
    if (style == BorderStyleOutset)
        return BorderStyleGroove;
    if (style == BorderStyleInset)
        return BorderStyleRidge;
    return style;
}

void TableCellPainter::paintCollapsedBorders(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, const CollapsedBorderValue& currentBorderValue)
{
    if (m_layoutTableCell.style()->visibility() != VISIBLE)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableCell.location();
    if (!BlockPainter(m_layoutTableCell).intersectsPaintRect(paintInfo, adjustedPaintOffset))
        return;

    const LayoutTableCell::CollapsedBorderValues* values = m_layoutTableCell.collapsedBorderValues();
    if (!values)
        return;

    const ComputedStyle& styleForCellFlow = m_layoutTableCell.styleForCellFlow();
    const CollapsedBorderValue& leftBorderValue = collapsedLeftBorder(styleForCellFlow, *values);
    const CollapsedBorderValue& rightBorderValue = collapsedRightBorder(styleForCellFlow, *values);
    const CollapsedBorderValue& topBorderValue = collapsedTopBorder(styleForCellFlow, *values);
    const CollapsedBorderValue& bottomBorderValue = collapsedBottomBorder(styleForCellFlow, *values);

    int displayItemType = DisplayItem::TableCollapsedBorderBase;
    if (topBorderValue.shouldPaint(currentBorderValue))
        displayItemType |= DisplayItem::TableCollapsedBorderTop;
    if (bottomBorderValue.shouldPaint(currentBorderValue))
        displayItemType |= DisplayItem::TableCollapsedBorderBottom;
    if (leftBorderValue.shouldPaint(currentBorderValue))
        displayItemType |= DisplayItem::TableCollapsedBorderLeft;
    if (rightBorderValue.shouldPaint(currentBorderValue))
        displayItemType |= DisplayItem::TableCollapsedBorderRight;
    if (displayItemType == DisplayItem::TableCollapsedBorderBase)
        return;

    int topWidth = topBorderValue.width();
    int bottomWidth = bottomBorderValue.width();
    int leftWidth = leftBorderValue.width();
    int rightWidth = rightBorderValue.width();

    // Adjust our x/y/width/height so that we paint the collapsed borders at the correct location.
    LayoutRect paintRect = paintRectNotIncludingVisualOverflow(adjustedPaintOffset);
    IntRect borderRect = pixelSnappedIntRect(paintRect.x() - leftWidth / 2,
        paintRect.y() - topWidth / 2,
        paintRect.width() + leftWidth / 2 + (rightWidth + 1) / 2,
        paintRect.height() + topWidth / 2 + (bottomWidth + 1) / 2);

    GraphicsContext& graphicsContext = paintInfo.context;
    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(graphicsContext, m_layoutTableCell, static_cast<DisplayItem::Type>(displayItemType)))
        return;

    LayoutObjectDrawingRecorder recorder(graphicsContext, m_layoutTableCell, static_cast<DisplayItem::Type>(displayItemType), borderRect);
    Color cellColor = m_layoutTableCell.resolveColor(CSSPropertyColor);

    // We never paint diagonals at the joins.  We simply let the border with the highest
    // precedence paint on top of borders with lower precedence.
    if (displayItemType & DisplayItem::TableCollapsedBorderTop) {
        ObjectPainter::drawLineForBoxSide(graphicsContext, borderRect.x(), borderRect.y(), borderRect.maxX(), borderRect.y() + topWidth, BSTop,
            topBorderValue.color().resolve(cellColor), collapsedBorderStyle(topBorderValue.style()), 0, 0, true);
    }
    if (displayItemType & DisplayItem::TableCollapsedBorderBottom) {
        ObjectPainter::drawLineForBoxSide(graphicsContext, borderRect.x(), borderRect.maxY() - bottomWidth, borderRect.maxX(), borderRect.maxY(), BSBottom,
            bottomBorderValue.color().resolve(cellColor), collapsedBorderStyle(bottomBorderValue.style()), 0, 0, true);
    }
    if (displayItemType & DisplayItem::TableCollapsedBorderLeft) {
        ObjectPainter::drawLineForBoxSide(graphicsContext, borderRect.x(), borderRect.y(), borderRect.x() + leftWidth, borderRect.maxY(), BSLeft,
            leftBorderValue.color().resolve(cellColor), collapsedBorderStyle(leftBorderValue.style()), 0, 0, true);
    }
    if (displayItemType & DisplayItem::TableCollapsedBorderRight) {
        ObjectPainter::drawLineForBoxSide(graphicsContext, borderRect.maxX() - rightWidth, borderRect.y(), borderRect.maxX(), borderRect.maxY(), BSRight,
            rightBorderValue.color().resolve(cellColor), collapsedBorderStyle(rightBorderValue.style()), 0, 0, true);
    }
}

void TableCellPainter::paintContainerBackgroundBehindCell(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, const LayoutObject& backgroundObject, DisplayItem::Type type)
{
    DCHECK(backgroundObject != m_layoutTableCell);

    if (m_layoutTableCell.style()->visibility() != VISIBLE)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutTableCell.location();
    if (!BlockPainter(m_layoutTableCell).intersectsPaintRect(paintInfo, adjustedPaintOffset))
        return;

    LayoutTable* table = m_layoutTableCell.table();
    if (!table->collapseBorders() && m_layoutTableCell.style()->emptyCells() == EmptyCellsHide && !m_layoutTableCell.firstChild())
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutTableCell, type))
        return;

    LayoutRect paintRect = paintRectNotIncludingVisualOverflow(adjustedPaintOffset);
    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutTableCell, type, paintRect);
    paintBackground(paintInfo, paintRect, backgroundObject);
}

void TableCellPainter::paintBackground(const PaintInfo& paintInfo, const LayoutRect& paintRect, const LayoutObject& backgroundObject)
{
    Color c = backgroundObject.resolveColor(CSSPropertyBackgroundColor);
    const FillLayer& bgLayer = backgroundObject.styleRef().backgroundLayers();
    if (bgLayer.hasImage() || c.alpha()) {
        // We have to clip here because the background would paint
        // on top of the borders otherwise.  This only matters for cells and rows.
        bool shouldClip = backgroundObject.hasLayer() && (backgroundObject == m_layoutTableCell || backgroundObject == m_layoutTableCell.parent()) && m_layoutTableCell.table()->collapseBorders();
        GraphicsContextStateSaver stateSaver(paintInfo.context, shouldClip);
        if (shouldClip) {
            LayoutRect clipRect(paintRect.location(), m_layoutTableCell.size());
            clipRect.expand(m_layoutTableCell.borderInsets());
            paintInfo.context.clip(pixelSnappedIntRect(clipRect));
        }
        BoxPainter(m_layoutTableCell).paintFillLayers(paintInfo, c, bgLayer, paintRect, BackgroundBleedNone, SkXfermode::kSrcOver_Mode, &backgroundObject);
    }
}

void TableCellPainter::paintBoxDecorationBackground(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutTable* table = m_layoutTableCell.table();
    if (!table->collapseBorders() && m_layoutTableCell.style()->emptyCells() == EmptyCellsHide && !m_layoutTableCell.firstChild())
        return;

    bool needsToPaintBorder = m_layoutTableCell.styleRef().hasBorderDecoration() && !table->collapseBorders();
    if (!m_layoutTableCell.hasBackground() && !m_layoutTableCell.styleRef().boxShadow() && !needsToPaintBorder)
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutTableCell, DisplayItem::BoxDecorationBackground))
        return;

    LayoutRect visualOverflowRect = m_layoutTableCell.visualOverflowRect();
    visualOverflowRect.moveBy(paintOffset);
    // TODO(chrishtr): the pixel-snapping here is likely incorrect.
    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutTableCell, DisplayItem::BoxDecorationBackground, pixelSnappedIntRect(visualOverflowRect));

    LayoutRect paintRect = paintRectNotIncludingVisualOverflow(paintOffset);

    BoxPainter::paintBoxShadow(paintInfo, paintRect, m_layoutTableCell.styleRef(), Normal);
    paintBackground(paintInfo, paintRect, m_layoutTableCell);
    BoxPainter::paintBoxShadow(paintInfo, paintRect, m_layoutTableCell.styleRef(), Inset);

    if (!needsToPaintBorder)
        return;

    BoxPainter::paintBorder(m_layoutTableCell, paintInfo, paintRect, m_layoutTableCell.styleRef());
}

void TableCellPainter::paintMask(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_layoutTableCell.style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    LayoutTable* tableElt = m_layoutTableCell.table();
    if (!tableElt->collapseBorders() && m_layoutTableCell.style()->emptyCells() == EmptyCellsHide && !m_layoutTableCell.firstChild())
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutTableCell, paintInfo.phase))
        return;

    LayoutRect paintRect = paintRectNotIncludingVisualOverflow(paintOffset);
    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutTableCell, paintInfo.phase, paintRect);
    BoxPainter(m_layoutTableCell).paintMaskImages(paintInfo, paintRect);
}

LayoutRect TableCellPainter::paintRectNotIncludingVisualOverflow(const LayoutPoint& paintOffset)
{
    return LayoutRect(paintOffset, LayoutSize(m_layoutTableCell.pixelSnappedSize()));
}

} // namespace blink

