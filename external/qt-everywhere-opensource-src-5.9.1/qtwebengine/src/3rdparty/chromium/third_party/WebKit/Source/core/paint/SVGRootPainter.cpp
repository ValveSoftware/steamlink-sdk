// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGRootPainter.h"

#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/BoxClipper.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintTiming.h"
#include "core/paint/SVGPaintContext.h"
#include "core/paint/TransformRecorder.h"
#include "core/svg/SVGSVGElement.h"
#include "wtf/Optional.h"

namespace blink {

IntRect SVGRootPainter::pixelSnappedSize(const LayoutPoint& paintOffset) const {
  return pixelSnappedIntRect(paintOffset, m_layoutSVGRoot.size());
}

AffineTransform SVGRootPainter::transformToPixelSnappedBorderBox(
    const LayoutPoint& paintOffset) const {
  const IntRect snappedSize = pixelSnappedSize(paintOffset);
  AffineTransform paintOffsetToBorderBox =
      AffineTransform::translation(snappedSize.x(), snappedSize.y());
  LayoutSize size = m_layoutSVGRoot.size();
  if (!size.isEmpty()) {
    paintOffsetToBorderBox.scale(
        snappedSize.width() / size.width().toFloat(),
        snappedSize.height() / size.height().toFloat());
  }
  paintOffsetToBorderBox.multiply(m_layoutSVGRoot.localToBorderBoxTransform());
  return paintOffsetToBorderBox;
}

void SVGRootPainter::paintReplaced(const PaintInfo& paintInfo,
                                   const LayoutPoint& paintOffset) {
  // An empty viewport disables rendering.
  if (pixelSnappedSize(paintOffset).isEmpty())
    return;

  // SVG outlines are painted during PaintPhaseForeground.
  if (shouldPaintSelfOutline(paintInfo.phase))
    return;

  // An empty viewBox also disables rendering.
  // (http://www.w3.org/TR/SVG/coords.html#ViewBoxAttribute)
  SVGSVGElement* svg = toSVGSVGElement(m_layoutSVGRoot.node());
  DCHECK(svg);
  if (svg->hasEmptyViewBox())
    return;

  // Apply initial viewport clip.
  Optional<BoxClipper> boxClipper;
  if (m_layoutSVGRoot.shouldApplyViewportClip()) {
    // TODO(pdr): Clip the paint info cull rect here.
    boxClipper.emplace(m_layoutSVGRoot, paintInfo, paintOffset,
                       ForceContentsClip);
  }

  PaintInfo paintInfoBeforeFiltering(paintInfo);
  AffineTransform transformToBorderBox =
      transformToPixelSnappedBorderBox(paintOffset);
  paintInfoBeforeFiltering.updateCullRect(transformToBorderBox);
  SVGTransformContext transformContext(paintInfoBeforeFiltering.context,
                                       m_layoutSVGRoot, transformToBorderBox);

  SVGPaintContext paintContext(m_layoutSVGRoot, paintInfoBeforeFiltering);
  if (paintContext.paintInfo().phase == PaintPhaseForeground &&
      !paintContext.applyClipMaskAndFilterIfNecessary())
    return;

  BoxPainter(m_layoutSVGRoot)
      .paintChildren(paintContext.paintInfo(), LayoutPoint());

  PaintTiming& timing =
      PaintTiming::from(m_layoutSVGRoot.node()->document().topDocument());
  timing.markFirstContentfulPaint();
}

}  // namespace blink
