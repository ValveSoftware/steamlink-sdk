// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGImagePainter.h"

#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutImageResource.h"
#include "core/layout/svg/LayoutSVGImage.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGPaintContext.h"
#include "core/svg/SVGImageElement.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/graphics/GraphicsContext.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

void SVGImagePainter::paint(const PaintInfo& paintInfo)
{
    if (paintInfo.phase != PaintPhaseForeground
        || m_layoutSVGImage.style()->visibility() != VISIBLE
        || !m_layoutSVGImage.imageResource()->hasImage())
        return;

    FloatRect boundingBox = m_layoutSVGImage.paintInvalidationRectInLocalSVGCoordinates();
    if (!paintInfo.cullRect().intersectsCullRect(m_layoutSVGImage.localToSVGParentTransform(), boundingBox))
        return;

    PaintInfo paintInfoBeforeFiltering(paintInfo);
    // Images cannot have children so do not call updateCullRect.
    SVGTransformContext transformContext(paintInfoBeforeFiltering.context, m_layoutSVGImage, m_layoutSVGImage.localToSVGParentTransform());
    {
        SVGPaintContext paintContext(m_layoutSVGImage, paintInfoBeforeFiltering);
        if (paintContext.applyClipMaskAndFilterIfNecessary() && !LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintContext.paintInfo().context, m_layoutSVGImage, paintContext.paintInfo().phase)) {
            LayoutObjectDrawingRecorder recorder(paintContext.paintInfo().context, m_layoutSVGImage, paintContext.paintInfo().phase, boundingBox);
            paintForeground(paintContext.paintInfo());
        }
    }

    if (m_layoutSVGImage.style()->outlineWidth()) {
        PaintInfo outlinePaintInfo(paintInfoBeforeFiltering);
        outlinePaintInfo.phase = PaintPhaseSelfOutlineOnly;
        ObjectPainter(m_layoutSVGImage).paintOutline(outlinePaintInfo, LayoutPoint(boundingBox.location()));
    }
}

void SVGImagePainter::paintForeground(const PaintInfo& paintInfo)
{
    const LayoutImageResource* imageResource = m_layoutSVGImage.imageResource();
    IntSize imageViewportSize = expandedIntSize(computeImageViewportSize());
    if (imageViewportSize.isEmpty())
        return;

    RefPtr<Image> image = imageResource->image(imageViewportSize, m_layoutSVGImage.style()->effectiveZoom());
    FloatRect destRect = m_layoutSVGImage.objectBoundingBox();
    FloatRect srcRect(0, 0, image->width(), image->height());

    SVGImageElement* imageElement = toSVGImageElement(m_layoutSVGImage.element());
    imageElement->preserveAspectRatio()->currentValue()->transformRect(destRect, srcRect);

    InterpolationQuality interpolationQuality = InterpolationDefault;
    interpolationQuality = ImageQualityController::imageQualityController()->chooseInterpolationQuality(m_layoutSVGImage, image.get(), image.get(), LayoutSize(destRect.size()));

    InterpolationQuality previousInterpolationQuality = paintInfo.context.imageInterpolationQuality();
    paintInfo.context.setImageInterpolationQuality(interpolationQuality);
    paintInfo.context.drawImage(image.get(), destRect, &srcRect);
    paintInfo.context.setImageInterpolationQuality(previousInterpolationQuality);
}

FloatSize SVGImagePainter::computeImageViewportSize() const
{
    ASSERT(m_layoutSVGImage.imageResource()->hasImage());

    if (toSVGImageElement(m_layoutSVGImage.element())->preserveAspectRatio()->currentValue()->align() != SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE)
        return m_layoutSVGImage.objectBoundingBox().size();

    ImageResource* cachedImage = m_layoutSVGImage.imageResource()->cachedImage();

    // Images with preserveAspectRatio=none should force non-uniform scaling. This can be achieved
    // by setting the image's container size to its viewport size (i.e. concrete object size
    // returned by the default sizing algorithm.)  See
    // https://www.w3.org/TR/SVG/single-page.html#coords-PreserveAspectRatioAttribute and
    // https://drafts.csswg.org/css-images-3/#default-sizing.

    // Avoid returning the size of the broken image.
    if (cachedImage->errorOccurred())
        return FloatSize();

    if (cachedImage->getImage()->isSVGImage())
        return toSVGImage(cachedImage->getImage())->concreteObjectSize(m_layoutSVGImage.objectBoundingBox().size());

    return FloatSize(cachedImage->getImage()->size());
}

} // namespace blink
