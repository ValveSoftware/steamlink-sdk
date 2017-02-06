/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "core/fetch/CSSStyleSheetResource.h"

#include "core/css/StyleSheetContents.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceClientOrObserverWalker.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "platform/SharedBuffer.h"
#include "wtf/CurrentTime.h"

namespace blink {

CSSStyleSheetResource* CSSStyleSheetResource::fetch(FetchRequest& request, ResourceFetcher* fetcher)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextStyle);
    return toCSSStyleSheetResource(fetcher->requestResource(request, CSSStyleSheetResourceFactory()));
}

CSSStyleSheetResource* CSSStyleSheetResource::createForTest(const ResourceRequest& request, const String& charset)
{
    return new CSSStyleSheetResource(request, ResourceLoaderOptions(), charset);
}

CSSStyleSheetResource::CSSStyleSheetResource(const ResourceRequest& resourceRequest, const ResourceLoaderOptions& options, const String& charset)
    : StyleSheetResource(resourceRequest, CSSStyleSheet, options, "text/css", charset)
{
}

CSSStyleSheetResource::~CSSStyleSheetResource()
{
}

void CSSStyleSheetResource::setParsedStyleSheetCache(StyleSheetContents* newSheet)
{
    if (m_parsedStyleSheetCache)
        m_parsedStyleSheetCache->clearReferencedFromResource();
    m_parsedStyleSheetCache = newSheet;
    if (m_parsedStyleSheetCache)
        m_parsedStyleSheetCache->setReferencedFromResource(this);
}

DEFINE_TRACE(CSSStyleSheetResource)
{
    visitor->trace(m_parsedStyleSheetCache);
    StyleSheetResource::trace(visitor);
}

void CSSStyleSheetResource::didAddClient(ResourceClient* c)
{
    ASSERT(StyleSheetResourceClient::isExpectedType(c));
    // Resource::didAddClient() must be before setCSSStyleSheet(),
    // because setCSSStyleSheet() may cause scripts to be executed, which could destroy 'c' if it is an instance of HTMLLinkElement.
    // see the comment of HTMLLinkElement::setCSSStyleSheet.
    Resource::didAddClient(c);

    if (!isLoading())
        static_cast<StyleSheetResourceClient*>(c)->setCSSStyleSheet(m_resourceRequest.url(), m_response.url(), encoding(), this);
}

const String CSSStyleSheetResource::sheetText(MIMETypeCheck mimeTypeCheck) const
{
    ASSERT(!isPurgeable());

    if (!m_data || m_data->isEmpty() || !canUseSheet(mimeTypeCheck))
        return String();

    if (!m_decodedSheetText.isNull())
        return m_decodedSheetText;

    // Don't cache the decoded text, regenerating is cheap and it can use quite a bit of memory
    return decodedText();
}

void CSSStyleSheetResource::checkNotify()
{
    // Decode the data to find out the encoding and keep the sheet text around during checkNotify()
    if (m_data)
        m_decodedSheetText = decodedText();

    ResourceClientWalker<StyleSheetResourceClient> w(m_clients);
    while (StyleSheetResourceClient* c = w.next()) {
        markClientFinished(c);
        c->setCSSStyleSheet(resourceRequest().url(), response().url(), encoding(), this);
    }
    // Clear the decoded text as it is unlikely to be needed immediately again and is cheap to regenerate.
    m_decodedSheetText = String();
}

bool CSSStyleSheetResource::isSafeToUnlock() const
{
    return m_data->hasOneRef();
}

void CSSStyleSheetResource::destroyDecodedDataIfPossible()
{
    if (!m_parsedStyleSheetCache)
        return;

    setParsedStyleSheetCache(nullptr);
    setDecodedSize(0);
}

bool CSSStyleSheetResource::canUseSheet(MIMETypeCheck mimeTypeCheck) const
{
    if (errorOccurred())
        return false;

    // This check exactly matches Firefox. Note that we grab the Content-Type
    // header directly because we want to see what the value is BEFORE content
    // sniffing. Firefox does this by setting a "type hint" on the channel.
    // This implementation should be observationally equivalent.
    //
    // This code defaults to allowing the stylesheet for non-HTTP protocols so
    // folks can use standards mode for local HTML documents.
    if (mimeTypeCheck == MIMETypeCheck::Lax)
        return true;
    AtomicString contentType = httpContentType();
    return contentType.isEmpty() || equalIgnoringCase(contentType, "text/css") || equalIgnoringCase(contentType, "application/x-unknown-content-type");
}

StyleSheetContents* CSSStyleSheetResource::restoreParsedStyleSheet(const CSSParserContext& context)
{
    if (!m_parsedStyleSheetCache)
        return nullptr;
    if (m_parsedStyleSheetCache->hasFailedOrCanceledSubresources()) {
        setParsedStyleSheetCache(nullptr);
        return nullptr;
    }

    ASSERT(m_parsedStyleSheetCache->isCacheableForResource());
    ASSERT(m_parsedStyleSheetCache->isReferencedFromResource());

    // Contexts must be identical so we know we would get the same exact result if we parsed again.
    if (m_parsedStyleSheetCache->parserContext() != context)
        return nullptr;

    didAccessDecodedData();

    return m_parsedStyleSheetCache;
}

void CSSStyleSheetResource::saveParsedStyleSheet(StyleSheetContents* sheet)
{
    ASSERT(sheet && sheet->isCacheableForResource());

    if (!memoryCache()->contains(this)) {
        // This stylesheet resource did conflict with another resource and was
        // not added to the cache.
        setParsedStyleSheetCache(nullptr);
        return;
    }
    setParsedStyleSheetCache(sheet);
    setDecodedSize(m_parsedStyleSheetCache->estimatedSizeInBytes());
}

} // namespace blink
