// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGClipPainter.h"

#include "core/dom/ElementTraversal.h"
#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/ClipPathClipper.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/SkPictureBuilder.h"

namespace blink {

namespace {

class SVGClipExpansionCycleHelper {
 public:
  SVGClipExpansionCycleHelper(LayoutSVGResourceClipper& clip) : m_clip(clip) {
    clip.beginClipExpansion();
  }
  ~SVGClipExpansionCycleHelper() { m_clip.endClipExpansion(); }

 private:
  LayoutSVGResourceClipper& m_clip;
};

}  // namespace

bool SVGClipPainter::prepareEffect(const LayoutObject& target,
                                   const FloatRect& targetBoundingBox,
                                   const FloatRect& visualRect,
                                   const FloatPoint& layerPositionOffset,
                                   GraphicsContext& context,
                                   ClipperState& clipperState) {
  DCHECK_EQ(clipperState, ClipperState::NotApplied);
  SECURITY_DCHECK(!m_clip.needsLayout());

  m_clip.clearInvalidationMask();

  if (m_clip.hasCycle())
    return false;

  SVGClipExpansionCycleHelper inClipExpansionChange(m_clip);

  AffineTransform animatedLocalTransform =
      toSVGClipPathElement(m_clip.element())->calculateAnimatedLocalTransform();
  // When drawing a clip for non-SVG elements, the CTM does not include the zoom
  // factor.  In this case, we need to apply the zoom scale explicitly - but
  // only for clips with userSpaceOnUse units (the zoom is accounted for
  // objectBoundingBox-resolved lengths).
  if (!target.isSVG() &&
      m_clip.clipPathUnits() == SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    DCHECK(m_clip.style());
    animatedLocalTransform.scale(m_clip.style()->effectiveZoom());
  }

  // First, try to apply the clip as a clipPath.
  Path clipPath;
  if (m_clip.asPath(animatedLocalTransform, targetBoundingBox, clipPath)) {
    AffineTransform positionTransform;
    positionTransform.translate(layerPositionOffset.x(),
                                layerPositionOffset.y());
    clipPath.transform(positionTransform);
    clipperState = ClipperState::AppliedPath;
    context.getPaintController().createAndAppend<BeginClipPathDisplayItem>(
        target, clipPath);
    return true;
  }

  // Fall back to masking.
  clipperState = ClipperState::AppliedMask;

  // Begin compositing the clip mask.
  CompositingRecorder::beginCompositing(context, target, SkBlendMode::kSrcOver,
                                        1, &visualRect);
  {
    if (!drawClipAsMask(context, target, targetBoundingBox, visualRect,
                        animatedLocalTransform, layerPositionOffset)) {
      // End the clip mask's compositor.
      CompositingRecorder::endCompositing(context, target);
      return false;
    }
  }

  // Masked content layer start.
  CompositingRecorder::beginCompositing(context, target, SkBlendMode::kSrcIn, 1,
                                        &visualRect);

  return true;
}

void SVGClipPainter::finishEffect(const LayoutObject& target,
                                  GraphicsContext& context,
                                  ClipperState& clipperState) {
  switch (clipperState) {
    case ClipperState::AppliedPath:
      // Path-only clipping, no layers to restore but we need to emit an end to
      // the clip path display item.
      context.getPaintController().endItem<EndClipPathDisplayItem>(target);
      break;
    case ClipperState::AppliedMask:
      // Transfer content -> clip mask (SrcIn)
      CompositingRecorder::endCompositing(context, target);

      // Transfer clip mask -> bg (SrcOver)
      CompositingRecorder::endCompositing(context, target);
      break;
    default:
      NOTREACHED();
  }
}

bool SVGClipPainter::drawClipAsMask(GraphicsContext& context,
                                    const LayoutObject& layoutObject,
                                    const FloatRect& targetBoundingBox,
                                    const FloatRect& targetVisualRect,
                                    const AffineTransform& localTransform,
                                    const FloatPoint& layerPositionOffset) {
  if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(
          context, layoutObject, DisplayItem::kSVGClip))
    return true;

  SkPictureBuilder maskPictureBuilder(targetVisualRect, nullptr, &context);
  GraphicsContext& maskContext = maskPictureBuilder.context();
  {
    TransformRecorder recorder(maskContext, layoutObject, localTransform);

    // Apply any clip-path clipping this clipPath (nested shape/clipPath.)
    Optional<ClipPathClipper> nestedClipPathClipper;
    if (ClipPathOperation* clipPathOperation = m_clip.styleRef().clipPath())
      nestedClipPathClipper.emplace(maskContext, *clipPathOperation, m_clip,
                                    targetBoundingBox, layerPositionOffset);

    {
      AffineTransform contentTransform;
      if (m_clip.clipPathUnits() ==
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
        contentTransform.translate(targetBoundingBox.x(),
                                   targetBoundingBox.y());
        contentTransform.scaleNonUniform(targetBoundingBox.width(),
                                         targetBoundingBox.height());
      }
      SubtreeContentTransformScope contentTransformScope(contentTransform);

      TransformRecorder contentTransformRecorder(maskContext, layoutObject,
                                                 contentTransform);
      maskContext.getPaintController().createAndAppend<DrawingDisplayItem>(
          layoutObject, DisplayItem::kSVGClip, m_clip.createContentPicture());
    }
  }

  LayoutObjectDrawingRecorder drawingRecorder(
      context, layoutObject, DisplayItem::kSVGClip, targetVisualRect);
  sk_sp<SkPicture> maskPicture = maskPictureBuilder.endRecording();
  context.drawPicture(maskPicture.get());
  return true;
}

}  // namespace blink
