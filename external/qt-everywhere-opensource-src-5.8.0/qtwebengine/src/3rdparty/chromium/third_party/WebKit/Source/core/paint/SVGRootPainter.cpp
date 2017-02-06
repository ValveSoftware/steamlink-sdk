// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGRootPainter.h"

#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintTiming.h"
#include "core/paint/SVGPaintContext.h"
#include "core/paint/TransformRecorder.h"
#include "core/svg/SVGSVGElement.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "wtf/Optional.h"

namespace blink {

void SVGRootPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // Pixel-snap to match BoxPainter's alignment.
    const IntRect adjustedRect = pixelSnappedIntRect(paintOffset, m_layoutSVGRoot.size());

    // An empty viewport disables rendering.
    if (adjustedRect.isEmpty())
        return;

    // SVG outlines are painted during PaintPhaseForeground.
    if (shouldPaintSelfOutline(paintInfo.phase))
        return;

    // An empty viewBox also disables rendering.
    // (http://www.w3.org/TR/SVG/coords.html#ViewBoxAttribute)
    SVGSVGElement* svg = toSVGSVGElement(m_layoutSVGRoot.node());
    ASSERT(svg);
    if (svg->hasEmptyViewBox())
        return;

    PaintInfo paintInfoBeforeFiltering(paintInfo);

    // Apply initial viewport clip.
    Optional<ClipRecorder> clipRecorder;
    if (m_layoutSVGRoot.shouldApplyViewportClip()) {
        // TODO(pdr): Clip the paint info cull rect here.
        clipRecorder.emplace(paintInfoBeforeFiltering.context, m_layoutSVGRoot, paintInfoBeforeFiltering.displayItemTypeForClipping(), pixelSnappedIntRect(m_layoutSVGRoot.overflowClipRect(paintOffset)));
    }

    // Convert from container offsets (html layoutObjects) to a relative transform (svg layoutObjects).
    // Transform from our paint container's coordinate system to our local coords.
    AffineTransform paintOffsetToBorderBox =
        AffineTransform::translation(adjustedRect.x(), adjustedRect.y());
    // Compensate for size snapping.
    paintOffsetToBorderBox.scale(
        adjustedRect.width() / m_layoutSVGRoot.size().width().toFloat(),
        adjustedRect.height() / m_layoutSVGRoot.size().height().toFloat());
    paintOffsetToBorderBox.multiply(m_layoutSVGRoot.localToBorderBoxTransform());

    paintInfoBeforeFiltering.updateCullRect(paintOffsetToBorderBox);
    SVGTransformContext transformContext(paintInfoBeforeFiltering.context, m_layoutSVGRoot, paintOffsetToBorderBox);

    SVGPaintContext paintContext(m_layoutSVGRoot, paintInfoBeforeFiltering);
    if (paintContext.paintInfo().phase == PaintPhaseForeground && !paintContext.applyClipMaskAndFilterIfNecessary())
        return;

    BoxPainter(m_layoutSVGRoot).paint(paintContext.paintInfo(), LayoutPoint());

    PaintTiming& timing = PaintTiming::from(m_layoutSVGRoot.node()->document().topDocument());
    timing.markFirstContentfulPaint();
}

} // namespace blink
