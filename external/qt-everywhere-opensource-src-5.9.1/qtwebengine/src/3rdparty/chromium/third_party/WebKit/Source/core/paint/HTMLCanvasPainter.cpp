// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/HTMLCanvasPainter.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/layout/LayoutHTMLCanvas.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/ForeignLayerDisplayItem.h"

namespace blink {

void HTMLCanvasPainter::paintReplaced(const PaintInfo& paintInfo,
                                      const LayoutPoint& paintOffset) {
  GraphicsContext& context = paintInfo.context;

  LayoutRect contentRect = m_layoutHTMLCanvas.contentBoxRect();
  contentRect.moveBy(paintOffset);
  LayoutRect paintRect = m_layoutHTMLCanvas.replacedContentRect();
  paintRect.moveBy(paintOffset);

  HTMLCanvasElement* canvas = toHTMLCanvasElement(m_layoutHTMLCanvas.node());

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
      canvas->renderingContext() &&
      canvas->renderingContext()->isAccelerated()) {
    if (WebLayer* layer = canvas->renderingContext()->platformLayer()) {
      IntRect pixelSnappedRect = pixelSnappedIntRect(contentRect);
      recordForeignLayer(context, m_layoutHTMLCanvas,
                         DisplayItem::kForeignLayerCanvas, layer,
                         pixelSnappedRect.location(), pixelSnappedRect.size());
      return;
    }
  }

  if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(
          context, m_layoutHTMLCanvas, paintInfo.phase))
    return;

  LayoutObjectDrawingRecorder drawingRecorder(context, m_layoutHTMLCanvas,
                                              paintInfo.phase, contentRect);

  bool clip = !contentRect.contains(paintRect);
  if (clip) {
    context.save();
    // TODO(chrishtr): this should be pixel-snapped.
    context.clip(FloatRect(contentRect));
  }

  // FIXME: InterpolationNone should be used if ImageRenderingOptimizeContrast
  // is set.  See bug for more details: crbug.com/353716.
  InterpolationQuality interpolationQuality =
      m_layoutHTMLCanvas.style()->imageRendering() ==
              ImageRenderingOptimizeContrast
          ? InterpolationLow
          : CanvasDefaultInterpolationQuality;
  if (m_layoutHTMLCanvas.style()->imageRendering() == ImageRenderingPixelated)
    interpolationQuality = InterpolationNone;

  InterpolationQuality previousInterpolationQuality =
      context.imageInterpolationQuality();
  context.setImageInterpolationQuality(interpolationQuality);
  canvas->paint(context, paintRect);
  context.setImageInterpolationQuality(previousInterpolationQuality);

  if (clip)
    context.restore();
}

}  // namespace blink
