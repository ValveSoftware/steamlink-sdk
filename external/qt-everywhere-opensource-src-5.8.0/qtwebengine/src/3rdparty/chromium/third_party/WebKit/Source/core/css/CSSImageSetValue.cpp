/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSImageSetValue.h"

#include "core/css/CSSImageValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/style/StyleFetchedImageSet.h"
#include "core/style/StyleInvalidImage.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>

namespace blink {

CSSImageSetValue::CSSImageSetValue()
    : CSSValueList(ImageSetClass, CommaSeparator)
    , m_cachedScaleFactor(1)
{
}

CSSImageSetValue::~CSSImageSetValue()
{
}

void CSSImageSetValue::fillImageSet()
{
    size_t length = this->length();
    size_t i = 0;
    while (i < length) {
        const CSSImageValue& imageValue = toCSSImageValue(item(i));
        String imageURL = imageValue.url();

        ++i;
        ASSERT_WITH_SECURITY_IMPLICATION(i < length);
        const CSSValue& scaleFactorValue = item(i);
        float scaleFactor = toCSSPrimitiveValue(scaleFactorValue).getFloatValue();

        ImageWithScale image;
        image.imageURL = imageURL;
        image.referrer = SecurityPolicy::generateReferrer(imageValue.referrer().referrerPolicy, KURL(ParsedURLString, imageURL), imageValue.referrer().referrer);
        image.scaleFactor = scaleFactor;
        m_imagesInSet.append(image);
        ++i;
    }

    // Sort the images so that they are stored in order from lowest resolution to highest.
    std::sort(m_imagesInSet.begin(), m_imagesInSet.end(), CSSImageSetValue::compareByScaleFactor);
}

CSSImageSetValue::ImageWithScale CSSImageSetValue::bestImageForScaleFactor(float scaleFactor)
{
    ImageWithScale image;
    size_t numberOfImages = m_imagesInSet.size();
    for (size_t i = 0; i < numberOfImages; ++i) {
        image = m_imagesInSet.at(i);
        if (image.scaleFactor >= scaleFactor)
            return image;
    }
    return image;
}

bool CSSImageSetValue::isCachePending(float deviceScaleFactor) const
{
    return !m_cachedImage || deviceScaleFactor != m_cachedScaleFactor;
}

StyleImage* CSSImageSetValue::cachedImage(float deviceScaleFactor) const
{
    ASSERT(!isCachePending(deviceScaleFactor));
    return m_cachedImage.get();
}

StyleImage* CSSImageSetValue::cacheImage(Document* document, float deviceScaleFactor, CrossOriginAttributeValue crossOrigin)
{
    ASSERT(document);

    if (!m_imagesInSet.size())
        fillImageSet();

    if (isCachePending(deviceScaleFactor)) {
        // FIXME: In the future, we want to take much more than deviceScaleFactor into acount here.
        // All forms of scale should be included: Page::pageScaleFactor(), LocalFrame::pageZoomFactor(),
        // and any CSS transforms. https://bugs.webkit.org/show_bug.cgi?id=81698
        ImageWithScale image = bestImageForScaleFactor(deviceScaleFactor);
        FetchRequest request(ResourceRequest(document->completeURL(image.imageURL)), FetchInitiatorTypeNames::css);
        request.mutableResourceRequest().setHTTPReferrer(image.referrer);

        if (crossOrigin != CrossOriginAttributeNotSet)
            request.setCrossOriginAccessControl(document->getSecurityOrigin(), crossOrigin);

        if (ImageResource* cachedImage = ImageResource::fetch(request, document->fetcher()))
            m_cachedImage = StyleFetchedImageSet::create(cachedImage, image.scaleFactor, this, request.url());
        else
            m_cachedImage = StyleInvalidImage::create(image.imageURL);
        m_cachedScaleFactor = deviceScaleFactor;
    }

    return m_cachedImage.get();
}

String CSSImageSetValue::customCSSText() const
{
    StringBuilder result;
    result.append("-webkit-image-set(");

    size_t length = this->length();
    size_t i = 0;
    while (i < length) {
        if (i > 0)
            result.append(", ");

        const CSSValue& imageValue = item(i);
        result.append(imageValue.cssText());
        result.append(' ');

        ++i;
        ASSERT_WITH_SECURITY_IMPLICATION(i < length);
        const CSSValue& scaleFactorValue = item(i);
        result.append(scaleFactorValue.cssText());
        // FIXME: Eventually the scale factor should contain it's own unit http://wkb.ug/100120.
        // For now 'x' is hard-coded in the parser, so we hard-code it here too.
        result.append('x');

        ++i;
    }

    result.append(')');
    return result.toString();
}

bool CSSImageSetValue::hasFailedOrCanceledSubresources() const
{
    if (!m_cachedImage)
        return false;
    if (Resource* cachedResource = m_cachedImage->cachedImage())
        return cachedResource->loadFailedOrCanceled();
    return true;
}

DEFINE_TRACE_AFTER_DISPATCH(CSSImageSetValue)
{
    visitor->trace(m_cachedImage);
    CSSValueList::traceAfterDispatch(visitor);
}

CSSImageSetValue* CSSImageSetValue::valueWithURLsMadeAbsolute()
{
    CSSImageSetValue* value = CSSImageSetValue::create();
    for (auto& item : *this)
        item->isImageValue() ? value->append(*toCSSImageValue(*item).valueWithURLMadeAbsolute()) : value->append(*item);
    return value;
}


} // namespace blink
