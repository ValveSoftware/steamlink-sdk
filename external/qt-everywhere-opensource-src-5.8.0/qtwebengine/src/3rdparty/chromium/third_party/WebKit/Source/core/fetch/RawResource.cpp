/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/fetch/RawResource.h"

#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceClientOrObserverWalker.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "platform/HTTPNames.h"
#include <memory>

namespace blink {

Resource* RawResource::fetchSynchronously(FetchRequest& request, ResourceFetcher* fetcher)
{
    request.mutableResourceRequest().setTimeoutInterval(10);
    ResourceLoaderOptions options(request.options());
    options.synchronousPolicy = RequestSynchronously;
    request.setOptions(options);
    return fetcher->requestResource(request, RawResourceFactory(Resource::Raw));
}

RawResource* RawResource::fetchImport(FetchRequest& request, ResourceFetcher* fetcher)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextImport);
    return toRawResource(fetcher->requestResource(request, RawResourceFactory(Resource::ImportResource)));
}

RawResource* RawResource::fetch(FetchRequest& request, ResourceFetcher* fetcher)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    ASSERT(request.resourceRequest().requestContext() != WebURLRequest::RequestContextUnspecified);
    return toRawResource(fetcher->requestResource(request, RawResourceFactory(Resource::Raw)));
}

RawResource* RawResource::fetchMainResource(FetchRequest& request, ResourceFetcher* fetcher, const SubstituteData& substituteData)
{
    ASSERT(request.resourceRequest().frameType() != WebURLRequest::FrameTypeNone);
    ASSERT(request.resourceRequest().requestContext() == WebURLRequest::RequestContextForm || request.resourceRequest().requestContext() == WebURLRequest::RequestContextFrame || request.resourceRequest().requestContext() == WebURLRequest::RequestContextHyperlink || request.resourceRequest().requestContext() == WebURLRequest::RequestContextIframe || request.resourceRequest().requestContext() == WebURLRequest::RequestContextInternal || request.resourceRequest().requestContext() == WebURLRequest::RequestContextLocation);

    return toRawResource(fetcher->requestResource(request, RawResourceFactory(Resource::MainResource), substituteData));
}

RawResource* RawResource::fetchMedia(FetchRequest& request, ResourceFetcher* fetcher)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    ASSERT(request.resourceRequest().requestContext() == WebURLRequest::RequestContextAudio || request.resourceRequest().requestContext() == WebURLRequest::RequestContextVideo);
    return toRawResource(fetcher->requestResource(request, RawResourceFactory(Resource::Media)));
}

RawResource* RawResource::fetchTextTrack(FetchRequest& request, ResourceFetcher* fetcher)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextTrack);
    return toRawResource(fetcher->requestResource(request, RawResourceFactory(Resource::TextTrack)));
}

RawResource* RawResource::fetchManifest(FetchRequest& request, ResourceFetcher* fetcher)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    ASSERT(request.resourceRequest().requestContext() == WebURLRequest::RequestContextManifest);
    return toRawResource(fetcher->requestResource(request, RawResourceFactory(Resource::Manifest)));
}

RawResource::RawResource(const ResourceRequest& resourceRequest, Type type, const ResourceLoaderOptions& options)
    : Resource(resourceRequest, type, options)
{
}

void RawResource::appendData(const char* data, size_t length)
{
    Resource::appendData(data, length);

    ResourceClientWalker<RawResourceClient> w(m_clients);
    while (RawResourceClient* c = w.next())
        c->dataReceived(this, data, length);
}

void RawResource::didAddClient(ResourceClient* c)
{
    if (!hasClient(c))
        return;
    ASSERT(RawResourceClient::isExpectedType(c));
    RawResourceClient* client = static_cast<RawResourceClient*>(c);
    WeakPtr<RawResourceClient> clientWeak = client->createWeakPtr();
    for (const auto& redirect : redirectChain()) {
        ResourceRequest request(redirect.m_request);
        client->redirectReceived(this, request, redirect.m_redirectResponse);
        if (!clientWeak || !hasClient(c))
            return;
    }

    if (!m_response.isNull())
        client->responseReceived(this, m_response, nullptr);
    if (!clientWeak || !hasClient(c))
        return;
    if (m_data)
        client->dataReceived(this, m_data->data(), m_data->size());
    if (!clientWeak || !hasClient(c))
        return;
    Resource::didAddClient(client);
}

void RawResource::willFollowRedirect(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    Resource::willFollowRedirect(newRequest, redirectResponse);

    ASSERT(!redirectResponse.isNull());
    ResourceClientWalker<RawResourceClient> w(m_clients);
    while (RawResourceClient* c = w.next())
        c->redirectReceived(this, newRequest, redirectResponse);
}

void RawResource::willNotFollowRedirect()
{
    ResourceClientWalker<RawResourceClient> w(m_clients);
    while (RawResourceClient* c = w.next())
        c->redirectBlocked();
}

void RawResource::responseReceived(const ResourceResponse& response, std::unique_ptr<WebDataConsumerHandle> handle)
{
    bool isSuccessfulRevalidation = isCacheValidator() && response.httpStatusCode() == 304;
    Resource::responseReceived(response, nullptr);

    ResourceClientWalker<RawResourceClient> w(m_clients);
    ASSERT(count() <= 1 || !handle);
    while (RawResourceClient* c = w.next()) {
        // |handle| is cleared when passed, but it's not a problem because
        // |handle| is null when there are two or more clients, as asserted.
        c->responseReceived(this, m_response, std::move(handle));
    }

    // If we successfully revalidated, we won't get appendData() calls.
    // Forward the data to clients now instead.
    // Note: |m_data| can be null when no data is appended to the original
    // resource.
    if (isSuccessfulRevalidation && m_data) {
        ResourceClientWalker<RawResourceClient> w(m_clients);
        while (RawResourceClient* c = w.next())
            c->dataReceived(this, m_data->data(), m_data->size());
    }
}

void RawResource::setSerializedCachedMetadata(const char* data, size_t size)
{
    Resource::setSerializedCachedMetadata(data, size);
    ResourceClientWalker<RawResourceClient> w(m_clients);
    while (RawResourceClient* c = w.next())
        c->setSerializedCachedMetadata(this, data, size);
}

void RawResource::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ResourceClientWalker<RawResourceClient> w(m_clients);
    while (RawResourceClient* c = w.next())
        c->dataSent(this, bytesSent, totalBytesToBeSent);
}

void RawResource::didDownloadData(int dataLength)
{
    ResourceClientWalker<RawResourceClient> w(m_clients);
    while (RawResourceClient* c = w.next())
        c->dataDownloaded(this, dataLength);
}

void RawResource::reportResourceTimingToClients(const ResourceTimingInfo& info)
{
    ResourceClientWalker<RawResourceClient> w(m_clients);
    while (RawResourceClient* c = w.next())
        c->didReceiveResourceTiming(this, info);
}

void RawResource::setDefersLoading(bool defers)
{
    if (m_loader)
        m_loader->setDefersLoading(defers);
}

static bool shouldIgnoreHeaderForCacheReuse(AtomicString headerName)
{
    // FIXME: This list of headers that don't affect cache policy almost certainly isn't complete.
    DEFINE_STATIC_LOCAL(HashSet<AtomicString>, m_headers, ());
    if (m_headers.isEmpty()) {
        m_headers.add("Cache-Control");
        m_headers.add("If-Modified-Since");
        m_headers.add("If-None-Match");
        m_headers.add("Origin");
        m_headers.add("Pragma");
        m_headers.add("Purpose");
        m_headers.add("Referer");
        m_headers.add("User-Agent");
        m_headers.add(HTTPNames::X_DevTools_Emulate_Network_Conditions_Client_Id);
    }
    return m_headers.contains(headerName);
}

static bool isCacheableHTTPMethod(const AtomicString& method)
{
    // Per http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html#sec13.10,
    // these methods always invalidate the cache entry.
    return method != "POST" && method != "PUT" && method != "DELETE";
}

bool RawResource::canReuse(const ResourceRequest& newRequest) const
{
    if (m_options.dataBufferingPolicy == DoNotBufferData)
        return false;

    if (!isCacheableHTTPMethod(m_resourceRequest.httpMethod()))
        return false;
    if (m_resourceRequest.httpMethod() != newRequest.httpMethod())
        return false;

    if (m_resourceRequest.httpBody() != newRequest.httpBody())
        return false;

    if (m_resourceRequest.allowStoredCredentials() != newRequest.allowStoredCredentials())
        return false;

    // Ensure most headers match the existing headers before continuing.
    // Note that the list of ignored headers includes some headers explicitly related to caching.
    // A more detailed check of caching policy will be performed later, this is simply a list of
    // headers that we might permit to be different and still reuse the existing Resource.
    const HTTPHeaderMap& newHeaders = newRequest.httpHeaderFields();
    const HTTPHeaderMap& oldHeaders = m_resourceRequest.httpHeaderFields();

    for (const auto& header : newHeaders) {
        AtomicString headerName = header.key;
        if (!shouldIgnoreHeaderForCacheReuse(headerName) && header.value != oldHeaders.get(headerName))
            return false;
    }

    for (const auto& header : oldHeaders) {
        AtomicString headerName = header.key;
        if (!shouldIgnoreHeaderForCacheReuse(headerName) && header.value != newHeaders.get(headerName))
            return false;
    }

    return true;
}

} // namespace blink
