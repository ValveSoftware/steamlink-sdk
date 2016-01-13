/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "core/svg/graphics/filters/SVGFEImage.h"

#include "SkBitmapSource.h"
#include "SkPictureImageFilter.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGURIReference.h"
#include "platform/graphics/DisplayList.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/text/TextStream.h"
#include "platform/transforms/AffineTransform.h"

namespace WebCore {

FEImage::FEImage(Filter* filter, PassRefPtr<Image> image, PassRefPtr<SVGPreserveAspectRatio> preserveAspectRatio)
    : FilterEffect(filter)
    , m_image(image)
    , m_treeScope(0)
    , m_preserveAspectRatio(preserveAspectRatio)
{
}

FEImage::FEImage(Filter* filter, TreeScope& treeScope, const String& href, PassRefPtr<SVGPreserveAspectRatio> preserveAspectRatio)
    : FilterEffect(filter)
    , m_treeScope(&treeScope)
    , m_href(href)
    , m_preserveAspectRatio(preserveAspectRatio)
{
}

PassRefPtr<FEImage> FEImage::createWithImage(Filter* filter, PassRefPtr<Image> image, PassRefPtr<SVGPreserveAspectRatio> preserveAspectRatio)
{
    return adoptRef(new FEImage(filter, image, preserveAspectRatio));
}

PassRefPtr<FEImage> FEImage::createWithIRIReference(Filter* filter, TreeScope& treeScope, const String& href, PassRefPtr<SVGPreserveAspectRatio> preserveAspectRatio)
{
    return adoptRef(new FEImage(filter, treeScope, href, preserveAspectRatio));
}

static FloatRect getRendererRepaintRect(RenderObject* renderer)
{
    return renderer->localToParentTransform().mapRect(
        renderer->paintInvalidationRectInLocalCoordinates());
}

FloatRect FEImage::determineAbsolutePaintRect(const FloatRect& originalRequestedRect)
{
    RenderObject* renderer = referencedRenderer();
    if (!m_image && !renderer)
        return FloatRect();

    FloatRect requestedRect = originalRequestedRect;
    if (clipsToBounds())
        requestedRect.intersect(maxEffectRect());

    FloatRect destRect = filter()->mapLocalRectToAbsoluteRect(filterPrimitiveSubregion());
    FloatRect srcRect;
    if (renderer) {
        srcRect = getRendererRepaintRect(renderer);
        SVGElement* contextNode = toSVGElement(renderer->node());

        if (contextNode->hasRelativeLengths()) {
            // FIXME: This fixes relative lengths but breaks non-relative ones (see crbug/260709).
            SVGLengthContext lengthContext(contextNode);
            FloatSize viewportSize;
            if (lengthContext.determineViewport(viewportSize)) {
                srcRect = makeMapBetweenRects(FloatRect(FloatPoint(), viewportSize), destRect).mapRect(srcRect);
            }
        } else {
            srcRect = filter()->mapLocalRectToAbsoluteRect(srcRect);
            srcRect.move(destRect.x(), destRect.y());
        }
        destRect.intersect(srcRect);
    } else {
        srcRect = FloatRect(FloatPoint(), m_image->size());
        m_preserveAspectRatio->transformRect(destRect, srcRect);
    }

    destRect.intersect(requestedRect);
    addAbsolutePaintRect(destRect);
    return destRect;
}

RenderObject* FEImage::referencedRenderer() const
{
    if (!m_treeScope)
        return 0;
    Element* hrefElement = SVGURIReference::targetElementFromIRIString(m_href, *m_treeScope);
    if (!hrefElement || !hrefElement->isSVGElement())
        return 0;
    return hrefElement->renderer();
}

void FEImage::applySoftware()
{
    RenderObject* renderer = referencedRenderer();
    if (!m_image && !renderer)
        return;

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;
    IntPoint paintLocation = absolutePaintRect().location();
    resultImage->context()->translate(-paintLocation.x(), -paintLocation.y());

    // FEImage results are always in ColorSpaceDeviceRGB
    setResultColorSpace(ColorSpaceDeviceRGB);

    FloatRect destRect = filter()->mapLocalRectToAbsoluteRect(filterPrimitiveSubregion());
    FloatRect srcRect;

    if (!renderer) {
        srcRect = FloatRect(FloatPoint(), m_image->size());
        m_preserveAspectRatio->transformRect(destRect, srcRect);

        resultImage->context()->drawImage(m_image.get(), destRect, srcRect);
        return;
    }

    SVGElement* contextNode = toSVGElement(renderer->node());
    if (contextNode->hasRelativeLengths()) {
        // FIXME: This fixes relative lengths but breaks non-relative ones (see crbug/260709).
        SVGLengthContext lengthContext(contextNode);
        FloatSize viewportSize;

        // If we're referencing an element with percentage units, eg. <rect with="30%"> those values were resolved against the viewport.
        // Build up a transformation that maps from the viewport space to the filter primitive subregion.
        if (lengthContext.determineViewport(viewportSize))
            resultImage->context()->concatCTM(makeMapBetweenRects(FloatRect(FloatPoint(), viewportSize), destRect));
    } else {
        resultImage->context()->translate(destRect.x(), destRect.y());
        resultImage->context()->concatCTM(filter()->absoluteTransform());
    }

    AffineTransform contentTransformation;
    SVGRenderingContext::renderSubtree(resultImage->context(), renderer, contentTransformation);
}

TextStream& FEImage::externalRepresentation(TextStream& ts, int indent) const
{
    IntSize imageSize;
    if (m_image)
        imageSize = m_image->size();
    else if (RenderObject* renderer = referencedRenderer())
        imageSize = enclosingIntRect(getRendererRepaintRect(renderer)).size();
    writeIndent(ts, indent);
    ts << "[feImage";
    FilterEffect::externalRepresentation(ts);
    ts << " image-size=\"" << imageSize.width() << "x" << imageSize.height() << "\"]\n";
    // FIXME: should this dump also object returned by SVGFEImage::image() ?
    return ts;
}

PassRefPtr<SkImageFilter> FEImage::createImageFilterForRenderer(RenderObject* renderer, SkiaImageFilterBuilder* builder)
{
    FloatRect dstRect = filterPrimitiveSubregion();

    AffineTransform transform;
    SVGElement* contextNode = toSVGElement(renderer->node());

    if (contextNode->hasRelativeLengths()) {
        SVGLengthContext lengthContext(contextNode);
        FloatSize viewportSize;

        // If we're referencing an element with percentage units, eg. <rect with="30%"> those values were resolved against the viewport.
        // Build up a transformation that maps from the viewport space to the filter primitive subregion.
        if (lengthContext.determineViewport(viewportSize))
            transform = makeMapBetweenRects(FloatRect(FloatPoint(), viewportSize), dstRect);
    } else {
        transform.translate(dstRect.x(), dstRect.y());
    }

    GraphicsContext* context = builder->context();
    if (!context)
        return adoptRef(SkBitmapSource::Create(SkBitmap()));
    AffineTransform contentTransformation;
    context->save();
    context->beginRecording(FloatRect(FloatPoint(), dstRect.size()));
    context->concatCTM(transform);
    SVGRenderingContext::renderSubtree(context, renderer, contentTransformation);
    RefPtr<DisplayList> displayList = context->endRecording();
    context->restore();
    RefPtr<SkImageFilter> result = adoptRef(SkPictureImageFilter::Create(displayList->picture(), dstRect));
    return result.release();
}

PassRefPtr<SkImageFilter> FEImage::createImageFilter(SkiaImageFilterBuilder* builder)
{
    RenderObject* renderer = referencedRenderer();
    if (!m_image && !renderer)
        return adoptRef(SkBitmapSource::Create(SkBitmap()));

    setOperatingColorSpace(ColorSpaceDeviceRGB);

    if (renderer)
        return createImageFilterForRenderer(renderer, builder);

    FloatRect srcRect = FloatRect(FloatPoint(), m_image->size());
    FloatRect dstRect = filterPrimitiveSubregion();

    // FIXME: CSS image filters currently do not seem to set filter primitive
    // subregion correctly if unspecified. So default to srcRect size if so.
    if (dstRect.isEmpty())
        dstRect = srcRect;

    m_preserveAspectRatio->transformRect(dstRect, srcRect);

    if (!m_image->nativeImageForCurrentFrame())
        return adoptRef(SkBitmapSource::Create(SkBitmap()));

    RefPtr<SkImageFilter> result = adoptRef(SkBitmapSource::Create(m_image->nativeImageForCurrentFrame()->bitmap(), srcRect, dstRect));
    return result.release();
}

} // namespace WebCore
