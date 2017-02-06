/*
 * Copyright (C) 2007, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSFontFaceSrcValue.h"

#include "core/css/CSSMarkup.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/loader/MixedContentChecker.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

bool CSSFontFaceSrcValue::isSupportedFormat() const
{
    // Normally we would just check the format, but in order to avoid conflicts with the old WinIE style of font-face,
    // we will also check to see if the URL ends with .eot.  If so, we'll go ahead and assume that we shouldn't load it.
    if (m_format.isEmpty())
        return m_absoluteResource.startsWith("data:", TextCaseInsensitive) || !m_absoluteResource.endsWith(".eot", TextCaseInsensitive);

    return FontCustomPlatformData::supportsFormat(m_format);
}

String CSSFontFaceSrcValue::customCSSText() const
{
    StringBuilder result;
    if (isLocal()) {
        result.append("local(");
        result.append(serializeString(m_absoluteResource));
        result.append(")");
    } else {
        result.append(serializeURI(m_specifiedResource));
    }
    if (!m_format.isEmpty()) {
        result.append(" format(");
        result.append(serializeString(m_format));
        result.append(")");
    }
    return result.toString();
}

bool CSSFontFaceSrcValue::hasFailedOrCanceledSubresources() const
{
    return m_fetched && m_fetched->resource()->loadFailedOrCanceled();
}

static void setCrossOriginAccessControl(FetchRequest& request, SecurityOrigin* securityOrigin)
{
    // Local fonts are accessible from file: URLs even when
    // allowFileAccessFromFileURLs is false.
    if (request.url().isLocalFile())
        return;

    request.setCrossOriginAccessControl(securityOrigin, CrossOriginAttributeAnonymous);
}

FontResource* CSSFontFaceSrcValue::fetch(Document* document) const
{
    if (!m_fetched) {
        FetchRequest request(ResourceRequest(m_absoluteResource), FetchInitiatorTypeNames::css);
        request.setContentSecurityCheck(m_shouldCheckContentSecurityPolicy);
        SecurityOrigin* securityOrigin = document->getSecurityOrigin();
        setCrossOriginAccessControl(request, securityOrigin);
        request.mutableResourceRequest().setHTTPReferrer(SecurityPolicy::generateReferrer(m_referrer.referrerPolicy, request.url(), m_referrer.referrer));
        FontResource* resource = FontResource::fetch(request, document->fetcher());
        if (!resource)
            return nullptr;
        m_fetched = FontResourceHelper::create(resource);
    } else {
        // FIXME: CSSFontFaceSrcValue::fetch is invoked when @font-face rule
        // is processed by StyleResolver / StyleEngine.
        restoreCachedResourceIfNeeded(document);
    }
    return m_fetched->resource();
}

void CSSFontFaceSrcValue::restoreCachedResourceIfNeeded(Document* document) const
{
    ASSERT(m_fetched);
    ASSERT(document && document->fetcher());

    const String resourceURL = document->completeURL(m_absoluteResource);
    if (document->fetcher()->cachedResource(KURL(ParsedURLString, resourceURL)))
        return;

    FetchRequest request(ResourceRequest(resourceURL), FetchInitiatorTypeNames::css);
    request.setContentSecurityCheck(m_shouldCheckContentSecurityPolicy);
    MixedContentChecker::shouldBlockFetch(document->frame(), m_fetched->resource()->lastResourceRequest(),
        m_fetched->resource()->lastResourceRequest().url(), MixedContentChecker::SendReport);
    document->fetcher()->requestLoadStarted(m_fetched->resource()->identifier(), m_fetched->resource(), request, ResourceFetcher::ResourceLoadingFromCache);
}

bool CSSFontFaceSrcValue::equals(const CSSFontFaceSrcValue& other) const
{
    return m_isLocal == other.m_isLocal && m_format == other.m_format && m_specifiedResource == other.m_specifiedResource && m_absoluteResource == other.m_absoluteResource;
}

} // namespace blink
