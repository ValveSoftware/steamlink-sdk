// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGMaskPainter.h"

#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

bool SVGMaskPainter::prepareEffect(const LayoutObject& object,
                                   GraphicsContext& context) {
  DCHECK(m_mask.style());
  SECURITY_DCHECK(!m_mask.needsLayout());

  m_mask.clearInvalidationMask();

  FloatRect visualRect = object.visualRectInLocalSVGCoordinates();
  if (visualRect.isEmpty() || !m_mask.element()->hasChildren())
    return false;

  context.getPaintController().createAndAppend<BeginCompositingDisplayItem>(
      object, SkBlendMode::kSrcOver, 1, &visualRect);
  return true;
}

void SVGMaskPainter::finishEffect(const LayoutObject& object,
                                  GraphicsContext& context) {
  DCHECK(m_mask.style());
  SECURITY_DCHECK(!m_mask.needsLayout());

  FloatRect visualRect = object.visualRectInLocalSVGCoordinates();
  {
    ColorFilter maskLayerFilter =
        m_mask.style()->svgStyle().maskType() == MT_LUMINANCE
            ? ColorFilterLuminanceToAlpha
            : ColorFilterNone;
    CompositingRecorder maskCompositing(context, object, SkBlendMode::kDstIn, 1,
                                        &visualRect, maskLayerFilter);
    drawMaskForLayoutObject(context, object, object.objectBoundingBox(),
                            visualRect);
  }

  context.getPaintController().endItem<EndCompositingDisplayItem>(object);
}

void SVGMaskPainter::drawMaskForLayoutObject(
    GraphicsContext& context,
    const LayoutObject& layoutObject,
    const FloatRect& targetBoundingBox,
    const FloatRect& targetVisualRect) {
  AffineTransform contentTransformation;
  sk_sp<const SkPicture> maskContentPicture = m_mask.createContentPicture(
      contentTransformation, targetBoundingBox, context);

  if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(
          context, layoutObject, DisplayItem::kSVGMask))
    return;

  LayoutObjectDrawingRecorder drawingRecorder(
      context, layoutObject, DisplayItem::kSVGMask, targetVisualRect);
  context.save();
  context.concatCTM(contentTransformation);
  context.drawPicture(maskContentPicture.get());
  context.restore();
}

}  // namespace blink
