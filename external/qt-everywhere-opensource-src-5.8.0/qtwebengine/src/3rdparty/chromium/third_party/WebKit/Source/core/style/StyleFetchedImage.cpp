/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "core/style/StyleFetchedImage.h"

#include "core/css/CSSImageValue.h"
#include "core/fetch/ImageResource.h"
#include "core/layout/LayoutObject.h"
#include "core/svg/graphics/SVGImage.h"
#include "core/svg/graphics/SVGImageForContainer.h"

namespace blink {

StyleFetchedImage::StyleFetchedImage(ImageResource* image, Document* document, const KURL& url)
    : m_image(image)
    , m_document(document)
    , m_url(url)
{
    m_isImageResource = true;
    m_image->addClient(this);
    ThreadState::current()->registerPreFinalizer(this);
}

StyleFetchedImage::~StyleFetchedImage()
{
}

void StyleFetchedImage::dispose()
{
    m_image->removeClient(this);
    m_image = nullptr;
}

WrappedImagePtr StyleFetchedImage::data() const
{
    return m_image.get();
}

ImageResource* StyleFetchedImage::cachedImage() const
{
    return m_image.get();
}

CSSValue* StyleFetchedImage::cssValue() const
{
    return CSSImageValue::create(m_image->url(), const_cast<StyleFetchedImage*>(this));
}

CSSValue* StyleFetchedImage::computedCSSValue() const
{
    return cssValue();
}

bool StyleFetchedImage::canRender() const
{
    return !m_image->errorOccurred() && !m_image->getImage()->isNull();
}

bool StyleFetchedImage::isLoaded() const
{
    return m_image->isLoaded();
}

bool StyleFetchedImage::errorOccurred() const
{
    return m_image->errorOccurred();
}

LayoutSize StyleFetchedImage::imageSize(const LayoutObject&, float multiplier, const LayoutSize& defaultObjectSize) const
{
    if (m_image->getImage() && m_image->getImage()->isSVGImage())
        return imageSizeForSVGImage(toSVGImage(m_image->getImage()), multiplier, defaultObjectSize);

    // Image orientation should only be respected for content images,
    // not decorative images such as StyleImage (backgrounds,
    // border-image, etc.)
    //
    // https://drafts.csswg.org/css-images-3/#the-image-orientation
    return m_image->imageSize(DoNotRespectImageOrientation, multiplier);
}

bool StyleFetchedImage::imageHasRelativeSize() const
{
    return m_image->imageHasRelativeSize();
}

bool StyleFetchedImage::usesImageContainerSize() const
{
    return m_image->usesImageContainerSize();
}

void StyleFetchedImage::addClient(LayoutObject* layoutObject)
{
    m_image->addObserver(layoutObject);
}

void StyleFetchedImage::removeClient(LayoutObject* layoutObject)
{
    m_image->removeObserver(layoutObject);
}

void StyleFetchedImage::notifyFinished(Resource* resource)
{
    if (m_document && m_image && m_image->getImage() && m_image->getImage()->isSVGImage())
        toSVGImage(m_image->getImage())->updateUseCounters(*m_document);
    // Oilpan: do not prolong the Document's lifetime.
    m_document.clear();
}

PassRefPtr<Image> StyleFetchedImage::image(const LayoutObject&, const IntSize& containerSize, float zoom) const
{
    if (!m_image->getImage()->isSVGImage())
        return m_image->getImage();

    return SVGImageForContainer::create(toSVGImage(m_image->getImage()), containerSize, zoom, m_url);
}

bool StyleFetchedImage::knownToBeOpaque(const LayoutObject& layoutObject) const
{
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage", "data", InspectorPaintImageEvent::data(&layoutObject, *m_image.get()));
    return m_image->getImage()->currentFrameKnownToBeOpaque(Image::PreCacheMetadata);
}

DEFINE_TRACE(StyleFetchedImage)
{
    visitor->trace(m_image);
    visitor->trace(m_document);
    StyleImage::trace(visitor);
}

} // namespace blink
