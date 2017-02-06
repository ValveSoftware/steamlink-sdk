// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGClipPainter.h"

#include "core/dom/ElementTraversal.h"
#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/SkPictureBuilder.h"

namespace blink {

namespace {

class SVGClipExpansionCycleHelper {
public:
    SVGClipExpansionCycleHelper(LayoutSVGResourceClipper& clip) : m_clip(clip) { clip.beginClipExpansion(); }
    ~SVGClipExpansionCycleHelper() { m_clip.endClipExpansion(); }
private:
    LayoutSVGResourceClipper& m_clip;
};

} // namespace

bool SVGClipPainter::prepareEffect(const LayoutObject& target, const FloatRect& targetBoundingBox,
    const FloatRect& paintInvalidationRect, const FloatPoint& layerPositionOffset, GraphicsContext& context, ClipperState& clipperState)
{
    ASSERT(clipperState == ClipperNotApplied);
    ASSERT_WITH_SECURITY_IMPLICATION(!m_clip.needsLayout());

    m_clip.clearInvalidationMask();

    if (paintInvalidationRect.isEmpty() || m_clip.hasCycle())
        return false;

    SVGClipExpansionCycleHelper inClipExpansionChange(m_clip);

    AffineTransform animatedLocalTransform = toSVGClipPathElement(m_clip.element())->calculateAnimatedLocalTransform();
    // When drawing a clip for non-SVG elements, the CTM does not include the zoom factor.
    // In this case, we need to apply the zoom scale explicitly - but only for clips with
    // userSpaceOnUse units (the zoom is accounted for objectBoundingBox-resolved lengths).
    if (!target.isSVG() && m_clip.clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
        ASSERT(m_clip.style());
        animatedLocalTransform.scale(m_clip.style()->effectiveZoom());
    }

    // First, try to apply the clip as a clipPath.
    Path clipPath;
    if (m_clip.asPath(animatedLocalTransform, targetBoundingBox, clipPath)) {
        AffineTransform positionTransform;
        positionTransform.translate(layerPositionOffset.x(), layerPositionOffset.y());
        clipPath.transform(positionTransform);
        clipperState = ClipperAppliedPath;
        context.getPaintController().createAndAppend<BeginClipPathDisplayItem>(target, clipPath);
        return true;
    }

    // Fall back to masking.
    clipperState = ClipperAppliedMask;

    // Begin compositing the clip mask.
    CompositingRecorder::beginCompositing(context, target, SkXfermode::kSrcOver_Mode, 1, &paintInvalidationRect);
    {
        if (!drawClipAsMask(context, target, targetBoundingBox, paintInvalidationRect, animatedLocalTransform, layerPositionOffset)) {
            // End the clip mask's compositor.
            CompositingRecorder::endCompositing(context, target);
            return false;
        }
    }

    // Masked content layer start.
    CompositingRecorder::beginCompositing(context, target, SkXfermode::kSrcIn_Mode, 1, &paintInvalidationRect);

    return true;
}

void SVGClipPainter::finishEffect(const LayoutObject& target, GraphicsContext& context, ClipperState& clipperState)
{
    switch (clipperState) {
    case ClipperAppliedPath:
        // Path-only clipping, no layers to restore but we need to emit an end to the clip path display item.
        context.getPaintController().endItem<EndClipPathDisplayItem>(target);
        break;
    case ClipperAppliedMask:
        // Transfer content -> clip mask (SrcIn)
        CompositingRecorder::endCompositing(context, target);

        // Transfer clip mask -> bg (SrcOver)
        CompositingRecorder::endCompositing(context, target);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

bool SVGClipPainter::drawClipAsMask(GraphicsContext& context, const LayoutObject& layoutObject, const FloatRect& targetBoundingBox,
    const FloatRect& targetPaintInvalidationRect, const AffineTransform& localTransform, const FloatPoint& layerPositionOffset)
{
    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(context, layoutObject, DisplayItem::SVGClip))
        return true;

    SkPictureBuilder maskPictureBuilder(targetPaintInvalidationRect, nullptr, &context);
    GraphicsContext& maskContext = maskPictureBuilder.context();
    {
        TransformRecorder recorder(maskContext, layoutObject, localTransform);

        // Create a clipPathClipper if this clipPath is clipped by another clipPath.
        SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(&m_clip);
        LayoutSVGResourceClipper* clipPathClipper = resources ? resources->clipper() : nullptr;
        ClipperState clipPathClipperState = ClipperNotApplied;
        if (clipPathClipper && !SVGClipPainter(*clipPathClipper).prepareEffect(m_clip, targetBoundingBox, targetPaintInvalidationRect, layerPositionOffset, maskContext, clipPathClipperState))
            return false;

        {
            AffineTransform contentTransform;
            if (m_clip.clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
                contentTransform.translate(targetBoundingBox.x(), targetBoundingBox.y());
                contentTransform.scaleNonUniform(targetBoundingBox.width(), targetBoundingBox.height());
            }
            SubtreeContentTransformScope contentTransformScope(contentTransform);

            TransformRecorder contentTransformRecorder(maskContext, layoutObject, contentTransform);
            RefPtr<const SkPicture> clipContentPicture = m_clip.createContentPicture();
            maskContext.getPaintController().createAndAppend<DrawingDisplayItem>(layoutObject, DisplayItem::SVGClip, clipContentPicture.get());
        }

        if (clipPathClipper)
            SVGClipPainter(*clipPathClipper).finishEffect(m_clip, maskContext, clipPathClipperState);
    }

    LayoutObjectDrawingRecorder drawingRecorder(context, layoutObject, DisplayItem::SVGClip, targetPaintInvalidationRect);
    RefPtr<SkPicture> maskPicture = maskPictureBuilder.endRecording();
    context.drawPicture(maskPicture.get());
    return true;
}

} // namespace blink
