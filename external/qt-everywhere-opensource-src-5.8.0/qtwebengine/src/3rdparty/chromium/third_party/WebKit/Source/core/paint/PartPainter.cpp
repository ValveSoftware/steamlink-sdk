// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PartPainter.h"

#include "core/layout/LayoutPart.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ReplacedPainter.h"
#include "core/paint/RoundedInnerRectClipper.h"
#include "core/paint/ScrollableAreaPainter.h"
#include "core/paint/TransformRecorder.h"
#include "wtf/Optional.h"

namespace blink {

bool PartPainter::isSelected() const
{
    SelectionState s = m_layoutPart.getSelectionState();
    if (s == SelectionNone)
        return false;
    if (s == SelectionInside)
        return true;

    int selectionStart, selectionEnd;
    m_layoutPart.selectionStartEnd(selectionStart, selectionEnd);
    if (s == SelectionStart)
        return selectionStart == 0;

    int end = m_layoutPart.node()->hasChildren() ? m_layoutPart.node()->countChildren() : 1;
    if (s == SelectionEnd)
        return selectionEnd == end;
    if (s == SelectionBoth)
        return selectionStart == 0 && selectionEnd == end;

    ASSERT(0);
    return false;
}

void PartPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutPart.location();
    if (!ReplacedPainter(m_layoutPart).shouldPaint(paintInfo, adjustedPaintOffset))
        return;

    LayoutRect borderRect(adjustedPaintOffset, m_layoutPart.size());

    if (m_layoutPart.hasBoxDecorationBackground() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection))
        BoxPainter(m_layoutPart).paintBoxDecorationBackground(paintInfo, adjustedPaintOffset);

    if (paintInfo.phase == PaintPhaseMask) {
        BoxPainter(m_layoutPart).paintMask(paintInfo, adjustedPaintOffset);
        return;
    }

    if (shouldPaintSelfOutline(paintInfo.phase))
        ObjectPainter(m_layoutPart).paintOutline(paintInfo, adjustedPaintOffset);

    if (paintInfo.phase != PaintPhaseForeground)
        return;

    if (m_layoutPart.widget()) {
        // TODO(schenney) crbug.com/93805 Speculative release assert to verify that the crashes
        // we see in widget painting are due to a destroyed LayoutPart object.
        RELEASE_ASSERT(m_layoutPart.node());
        Optional<RoundedInnerRectClipper> clipper;
        if (m_layoutPart.style()->hasBorderRadius()) {
            if (borderRect.isEmpty())
                return;

            FloatRoundedRect roundedInnerRect = m_layoutPart.style()->getRoundedInnerBorderFor(borderRect,
                LayoutRectOutsets(
                    -(m_layoutPart.paddingTop() + m_layoutPart.borderTop()),
                    -(m_layoutPart.paddingRight() + m_layoutPart.borderRight()),
                    -(m_layoutPart.paddingBottom() + m_layoutPart.borderBottom()),
                    -(m_layoutPart.paddingLeft() + m_layoutPart.borderLeft())),
                true, true);
            clipper.emplace(m_layoutPart, paintInfo, borderRect, roundedInnerRect, ApplyToDisplayList);
        }

        m_layoutPart.paintContents(paintInfo, paintOffset);
    }

    // Paint a partially transparent wash over selected widgets.
    if (isSelected() && !paintInfo.isPrinting() && !LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutPart, paintInfo.phase)) {
        LayoutRect rect = m_layoutPart.localSelectionRect();
        rect.moveBy(adjustedPaintOffset);
        IntRect selectionRect = pixelSnappedIntRect(rect);
        LayoutObjectDrawingRecorder drawingRecorder(paintInfo.context, m_layoutPart, paintInfo.phase, selectionRect);
        paintInfo.context.fillRect(selectionRect, m_layoutPart.selectionBackgroundColor());
    }

    if (m_layoutPart.canResize())
        ScrollableAreaPainter(*m_layoutPart.layer()->getScrollableArea()).paintResizer(paintInfo.context, roundedIntPoint(adjustedPaintOffset), paintInfo.cullRect());
}

void PartPainter::paintContents(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutPart.location();

    Widget* widget = m_layoutPart.widget();
    RELEASE_ASSERT(widget);

    // Tell the widget to paint now. This is the only time the widget is allowed
    // to paint itself. That way it will composite properly with z-indexed layers.
    IntPoint widgetLocation = widget->frameRect().location();
    IntPoint paintLocation(roundedIntPoint(adjustedPaintOffset + m_layoutPart.contentBoxOffset()));

    IntSize widgetPaintOffset = paintLocation - widgetLocation;
    // When painting widgets into compositing layers, tx and ty are relative to the enclosing compositing layer,
    // not the root. In this case, shift the CTM and adjust the CullRect to be root-relative to fix plugin drawing.
    TransformRecorder transform(paintInfo.context, m_layoutPart,
        AffineTransform::translation(widgetPaintOffset.width(), widgetPaintOffset.height()));

    CullRect adjustedCullRect(paintInfo.cullRect(), -widgetPaintOffset);
    widget->paint(paintInfo.context, adjustedCullRect);
}

} // namespace blink
