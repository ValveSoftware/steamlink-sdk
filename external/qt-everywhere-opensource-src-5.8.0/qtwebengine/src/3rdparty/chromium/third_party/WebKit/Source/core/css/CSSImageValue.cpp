/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "core/css/CSSImageValue.h"

#include "core/css/CSSMarkup.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/loader/MixedContentChecker.h"
#include "core/style/StyleFetchedImage.h"
#include "core/style/StyleInvalidImage.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace blink {

CSSImageValue::CSSImageValue(const AtomicString& rawValue, const KURL& url, StyleImage* image)
    : CSSValue(ImageClass)
    , m_relativeURL(rawValue)
    , m_absoluteURL(url.getString())
    , m_isCachePending(!image)
    , m_cachedImage(image)
{
}

CSSImageValue::CSSImageValue(const AtomicString& absoluteURL)
    : CSSValue(ImageClass)
    , m_relativeURL(absoluteURL)
    , m_absoluteURL(absoluteURL)
    , m_isCachePending(true)
{
}

CSSImageValue::~CSSImageValue()
{
}

StyleImage* CSSImageValue::cacheImage(Document* document, CrossOriginAttributeValue crossOrigin)
{
    ASSERT(document);

    if (m_isCachePending) {
        m_isCachePending = false;

        FetchRequest request(ResourceRequest(m_absoluteURL), m_initiatorName.isEmpty() ? FetchInitiatorTypeNames::css : m_initiatorName);
        request.mutableResourceRequest().setHTTPReferrer(SecurityPolicy::generateReferrer(m_referrer.referrerPolicy, request.url(), m_referrer.referrer));

        if (crossOrigin != CrossOriginAttributeNotSet)
            request.setCrossOriginAccessControl(document->getSecurityOrigin(), crossOrigin);

        if (ImageResource* cachedImage = ImageResource::fetch(request, document->fetcher()))
            m_cachedImage = StyleFetchedImage::create(cachedImage, document, request.url());
        else
            m_cachedImage = StyleInvalidImage::create(url());
    }

    return m_cachedImage.get();
}

void CSSImageValue::restoreCachedResourceIfNeeded(Document& document) const
{
    if (m_isCachePending || !m_cachedImage || !document.fetcher())
        return;
    if (document.fetcher()->cachedResource(KURL(ParsedURLString, m_absoluteURL)))
        return;

    ImageResource* resource = m_cachedImage->cachedImage();
    if (!resource)
        return;

    FetchRequest request(ResourceRequest(m_absoluteURL), m_initiatorName.isEmpty() ? FetchInitiatorTypeNames::css : m_initiatorName, resource->options());
    MixedContentChecker::shouldBlockFetch(document.frame(), resource->lastResourceRequest(),
        resource->lastResourceRequest().url(), MixedContentChecker::SendReport);
    document.fetcher()->requestLoadStarted(resource->identifier(), resource, request, ResourceFetcher::ResourceLoadingFromCache);
}

bool CSSImageValue::hasFailedOrCanceledSubresources() const
{
    if (!m_cachedImage)
        return false;
    if (Resource* cachedResource = m_cachedImage->cachedImage())
        return cachedResource->loadFailedOrCanceled();
    return true;
}

bool CSSImageValue::equals(const CSSImageValue& other) const
{
    return m_absoluteURL == other.m_absoluteURL;
}

String CSSImageValue::customCSSText() const
{
    return serializeURI(m_relativeURL);
}

bool CSSImageValue::knownToBeOpaque(const LayoutObject& layoutObject) const
{
    return m_cachedImage ? m_cachedImage->knownToBeOpaque(layoutObject) : false;
}

DEFINE_TRACE_AFTER_DISPATCH(CSSImageValue)
{
    visitor->trace(m_cachedImage);
    CSSValue::traceAfterDispatch(visitor);
}

void CSSImageValue::reResolveURL(const Document& document) const
{
    KURL url = document.completeURL(m_relativeURL);
    AtomicString urlString(url.getString());
    if (urlString == m_absoluteURL)
        return;
    m_absoluteURL = urlString;
    m_isCachePending = true;
    m_cachedImage.clear();
}

} // namespace blink
