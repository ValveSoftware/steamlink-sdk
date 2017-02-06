// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ReplacedPainter.h"

#include "core/layout/LayoutReplaced.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/RoundedInnerRectClipper.h"
#include "wtf/Optional.h"

namespace blink {

static bool shouldApplyViewportClip(const LayoutReplaced& layoutReplaced)
{
    return !layoutReplaced.isSVGRoot() || toLayoutSVGRoot(&layoutReplaced)->shouldApplyViewportClip();
}

void ReplacedPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutReplaced.location();
    if (!shouldPaint(paintInfo, adjustedPaintOffset))
        return;

    LayoutRect borderRect(adjustedPaintOffset, m_layoutReplaced.size());

    if (m_layoutReplaced.style()->visibility() == VISIBLE && m_layoutReplaced.hasBoxDecorationBackground() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection))
        m_layoutReplaced.paintBoxDecorationBackground(paintInfo, adjustedPaintOffset);

    if (paintInfo.phase == PaintPhaseMask) {
        m_layoutReplaced.paintMask(paintInfo, adjustedPaintOffset);
        return;
    }

    if (paintInfo.phase == PaintPhaseClippingMask && (!m_layoutReplaced.hasLayer() || !m_layoutReplaced.layer()->hasCompositedClippingMask()))
        return;

    if (shouldPaintSelfOutline(paintInfo.phase)) {
        ObjectPainter(m_layoutReplaced).paintOutline(paintInfo, adjustedPaintOffset);
        return;
    }

    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection && !m_layoutReplaced.canHaveChildren() && paintInfo.phase != PaintPhaseClippingMask)
        return;

    if (paintInfo.phase == PaintPhaseSelection)
        if (m_layoutReplaced.getSelectionState() == SelectionNone)
            return;

    {
        Optional<RoundedInnerRectClipper> clipper;
        bool completelyClippedOut = false;
        if (m_layoutReplaced.style()->hasBorderRadius()) {
            if (borderRect.isEmpty()) {
                completelyClippedOut = true;
            } else if (shouldApplyViewportClip(m_layoutReplaced)) {
                // Push a clip if we have a border radius, since we want to round the foreground content that gets painted.
                FloatRoundedRect roundedInnerRect = m_layoutReplaced.style()->getRoundedInnerBorderFor(borderRect,
                    LayoutRectOutsets(
                        -(m_layoutReplaced.paddingTop() + m_layoutReplaced.borderTop()),
                        -(m_layoutReplaced.paddingRight() + m_layoutReplaced.borderRight()),
                        -(m_layoutReplaced.paddingBottom() + m_layoutReplaced.borderBottom()),
                        -(m_layoutReplaced.paddingLeft() + m_layoutReplaced.borderLeft())),
                    true, true);

                clipper.emplace(m_layoutReplaced, paintInfo, borderRect, roundedInnerRect, ApplyToDisplayList);
            }
        }

        if (!completelyClippedOut) {
            if (paintInfo.phase == PaintPhaseClippingMask) {
                BoxPainter(m_layoutReplaced).paintClippingMask(paintInfo, adjustedPaintOffset);
            } else {
                m_layoutReplaced.paintReplaced(paintInfo, adjustedPaintOffset);
            }
        }
    }

    // The selection tint never gets clipped by border-radius rounding, since we want it to run right up to the edges of
    // surrounding content.
    bool drawSelectionTint = paintInfo.phase == PaintPhaseForeground && m_layoutReplaced.getSelectionState() != SelectionNone && !paintInfo.isPrinting();
    if (drawSelectionTint && !LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutReplaced, DisplayItem::SelectionTint)) {
        LayoutRect selectionPaintingRect = m_layoutReplaced.localSelectionRect();
        selectionPaintingRect.moveBy(adjustedPaintOffset);
        IntRect selectionPaintingIntRect = pixelSnappedIntRect(selectionPaintingRect);

        LayoutObjectDrawingRecorder drawingRecorder(paintInfo.context, m_layoutReplaced, DisplayItem::SelectionTint, selectionPaintingIntRect);
        paintInfo.context.fillRect(selectionPaintingIntRect, m_layoutReplaced.selectionBackgroundColor());
    }
}

bool ReplacedPainter::shouldPaint(const PaintInfo& paintInfo, const LayoutPoint& adjustedPaintOffset) const
{
    if (paintInfo.phase != PaintPhaseForeground && !shouldPaintSelfOutline(paintInfo.phase)
        && paintInfo.phase != PaintPhaseSelection && paintInfo.phase != PaintPhaseMask && paintInfo.phase != PaintPhaseClippingMask)
        return false;

    // If we're invisible or haven't received a layout yet, just bail.
    // But if it's an SVG root, there can be children, so we'll check visibility later.
    if (!m_layoutReplaced.isSVGRoot() && m_layoutReplaced.style()->visibility() != VISIBLE)
        return false;

    LayoutRect paintRect(m_layoutReplaced.visualOverflowRect());
    paintRect.unite(m_layoutReplaced.localSelectionRect());
    paintRect.moveBy(adjustedPaintOffset);

    if (!paintInfo.cullRect().intersectsCullRect(paintRect))
        return false;

    return true;
}

} // namespace blink
