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
#include "core/fetch/ResourceClientWalker.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "platform/HTTPNames.h"
#include <memory>

namespace blink {

Resource* RawResource::fetchSynchronously(FetchRequest& request,
                                          ResourceFetcher* fetcher) {
  request.makeSynchronous();
  return fetcher->requestResource(request, RawResourceFactory(Resource::Raw));
}

RawResource* RawResource::fetchImport(FetchRequest& request,
                                      ResourceFetcher* fetcher) {
  DCHECK_EQ(request.resourceRequest().frameType(),
            WebURLRequest::FrameTypeNone);
  request.mutableResourceRequest().setRequestContext(
      WebURLRequest::RequestContextImport);
  return toRawResource(fetcher->requestResource(
      request, RawResourceFactory(Resource::ImportResource)));
}

RawResource* RawResource::fetch(FetchRequest& request,
                                ResourceFetcher* fetcher) {
  DCHECK_EQ(request.resourceRequest().frameType(),
            WebURLRequest::FrameTypeNone);
  DCHECK_NE(request.resourceRequest().requestContext(),
            WebURLRequest::RequestContextUnspecified);
  return toRawResource(
      fetcher->requestResource(request, RawResourceFactory(Resource::Raw)));
}

RawResource* RawResource::fetchMainResource(
    FetchRequest& request,
    ResourceFetcher* fetcher,
    const SubstituteData& substituteData) {
  DCHECK_NE(request.resourceRequest().frameType(),
            WebURLRequest::FrameTypeNone);
  DCHECK(request.resourceRequest().requestContext() ==
             WebURLRequest::RequestContextForm ||
         request.resourceRequest().requestContext() ==
             WebURLRequest::RequestContextFrame ||
         request.resourceRequest().requestContext() ==
             WebURLRequest::RequestContextHyperlink ||
         request.resourceRequest().requestContext() ==
             WebURLRequest::RequestContextIframe ||
         request.resourceRequest().requestContext() ==
             WebURLRequest::RequestContextInternal ||
         request.resourceRequest().requestContext() ==
             WebURLRequest::RequestContextLocation);

  return toRawResource(fetcher->requestResource(
      request, RawResourceFactory(Resource::MainResource), substituteData));
}

RawResource* RawResource::fetchMedia(FetchRequest& request,
                                     ResourceFetcher* fetcher) {
  DCHECK_EQ(request.resourceRequest().frameType(),
            WebURLRequest::FrameTypeNone);
  DCHECK(request.resourceRequest().requestContext() ==
             WebURLRequest::RequestContextAudio ||
         request.resourceRequest().requestContext() ==
             WebURLRequest::RequestContextVideo);
  return toRawResource(
      fetcher->requestResource(request, RawResourceFactory(Resource::Media)));
}

RawResource* RawResource::fetchTextTrack(FetchRequest& request,
                                         ResourceFetcher* fetcher) {
  DCHECK_EQ(request.resourceRequest().frameType(),
            WebURLRequest::FrameTypeNone);
  request.mutableResourceRequest().setRequestContext(
      WebURLRequest::RequestContextTrack);
  return toRawResource(fetcher->requestResource(
      request, RawResourceFactory(Resource::TextTrack)));
}

RawResource* RawResource::fetchManifest(FetchRequest& request,
                                        ResourceFetcher* fetcher) {
  DCHECK_EQ(request.resourceRequest().frameType(),
            WebURLRequest::FrameTypeNone);
  DCHECK_EQ(request.resourceRequest().requestContext(),
            WebURLRequest::RequestContextManifest);
  return toRawResource(fetcher->requestResource(
      request, RawResourceFactory(Resource::Manifest)));
}

RawResource::RawResource(const ResourceRequest& resourceRequest,
                         Type type,
                         const ResourceLoaderOptions& options)
    : Resource(resourceRequest, type, options) {}

void RawResource::appendData(const char* data, size_t length) {
  Resource::appendData(data, length);

  ResourceClientWalker<RawResourceClient> w(clients());
  while (RawResourceClient* c = w.next())
    c->dataReceived(this, data, length);
}

void RawResource::didAddClient(ResourceClient* c) {
  // CHECK()s for isCacheValidator() are for https://crbug.com/640960#c24.
  CHECK(!isCacheValidator());
  if (!hasClient(c))
    return;
  DCHECK(RawResourceClient::isExpectedType(c));
  RawResourceClient* client = static_cast<RawResourceClient*>(c);
  for (const auto& redirect : redirectChain()) {
    ResourceRequest request(redirect.m_request);
    client->redirectReceived(this, request, redirect.m_redirectResponse);
    CHECK(!isCacheValidator());
    if (!hasClient(c))
      return;
  }

  if (!response().isNull())
    client->responseReceived(this, response(), nullptr);
  CHECK(!isCacheValidator());
  if (!hasClient(c))
    return;
  if (data())
    client->dataReceived(this, data()->data(), data()->size());
  CHECK(!isCacheValidator());
  if (!hasClient(c))
    return;
  Resource::didAddClient(client);
}

bool RawResource::willFollowRedirect(const ResourceRequest& newRequest,
                                     const ResourceResponse& redirectResponse) {
  bool follow = Resource::willFollowRedirect(newRequest, redirectResponse);
  // The base class method takes a non const reference of a ResourceRequest
  // and returns bool just for allowing RawResource to reject redirect. It
  // must always return true.
  DCHECK(follow);

  DCHECK(!redirectResponse.isNull());
  ResourceClientWalker<RawResourceClient> w(clients());
  while (RawResourceClient* c = w.next()) {
    if (!c->redirectReceived(this, newRequest, redirectResponse))
      follow = false;
  }

  return follow;
}

void RawResource::willNotFollowRedirect() {
  ResourceClientWalker<RawResourceClient> w(clients());
  while (RawResourceClient* c = w.next())
    c->redirectBlocked();
}

void RawResource::responseReceived(
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  bool isSuccessfulRevalidation =
      isCacheValidator() && response.httpStatusCode() == 304;
  Resource::responseReceived(response, nullptr);

  ResourceClientWalker<RawResourceClient> w(clients());
  DCHECK(clients().size() <= 1 || !handle);
  while (RawResourceClient* c = w.next()) {
    // |handle| is cleared when passed, but it's not a problem because |handle|
    // is null when there are two or more clients, as asserted.
    c->responseReceived(this, this->response(), std::move(handle));
  }

  // If we successfully revalidated, we won't get appendData() calls. Forward
  // the data to clients now instead. Note: |m_data| can be null when no data is
  // appended to the original resource.
  if (isSuccessfulRevalidation && data()) {
    ResourceClientWalker<RawResourceClient> w(clients());
    while (RawResourceClient* c = w.next())
      c->dataReceived(this, data()->data(), data()->size());
  }
}

void RawResource::setSerializedCachedMetadata(const char* data, size_t size) {
  Resource::setSerializedCachedMetadata(data, size);
  ResourceClientWalker<RawResourceClient> w(clients());
  while (RawResourceClient* c = w.next())
    c->setSerializedCachedMetadata(this, data, size);
}

void RawResource::didSendData(unsigned long long bytesSent,
                              unsigned long long totalBytesToBeSent) {
  ResourceClientWalker<RawResourceClient> w(clients());
  while (RawResourceClient* c = w.next())
    c->dataSent(this, bytesSent, totalBytesToBeSent);
}

void RawResource::didDownloadData(int dataLength) {
  ResourceClientWalker<RawResourceClient> w(clients());
  while (RawResourceClient* c = w.next())
    c->dataDownloaded(this, dataLength);
}

void RawResource::reportResourceTimingToClients(
    const ResourceTimingInfo& info) {
  ResourceClientWalker<RawResourceClient> w(clients());
  while (RawResourceClient* c = w.next())
    c->didReceiveResourceTiming(this, info);
}

void RawResource::setDefersLoading(bool defers) {
  if (loader())
    loader()->setDefersLoading(defers);
}

static bool shouldIgnoreHeaderForCacheReuse(AtomicString headerName) {
  // FIXME: This list of headers that don't affect cache policy almost certainly
  // isn't complete.
  DEFINE_STATIC_LOCAL(
      HashSet<AtomicString>, headers,
      ({
          "Cache-Control", "If-Modified-Since", "If-None-Match", "Origin",
          "Pragma", "Purpose", "Referer", "User-Agent",
          HTTPNames::X_DevTools_Emulate_Network_Conditions_Client_Id,
      }));
  return headers.contains(headerName);
}

static bool isCacheableHTTPMethod(const AtomicString& method) {
  // Per http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html#sec13.10,
  // these methods always invalidate the cache entry.
  return method != "POST" && method != "PUT" && method != "DELETE";
}

bool RawResource::canReuse(const ResourceRequest& newRequest) const {
  if (dataBufferingPolicy() == DoNotBufferData)
    return false;

  if (!isCacheableHTTPMethod(resourceRequest().httpMethod()))
    return false;
  if (resourceRequest().httpMethod() != newRequest.httpMethod())
    return false;

  if (resourceRequest().httpBody() != newRequest.httpBody())
    return false;

  if (resourceRequest().allowStoredCredentials() !=
      newRequest.allowStoredCredentials())
    return false;

  // Ensure most headers match the existing headers before continuing. Note that
  // the list of ignored headers includes some headers explicitly related to
  // caching. A more detailed check of caching policy will be performed later,
  // this is simply a list of headers that we might permit to be different and
  // still reuse the existing Resource.
  const HTTPHeaderMap& newHeaders = newRequest.httpHeaderFields();
  const HTTPHeaderMap& oldHeaders = resourceRequest().httpHeaderFields();

  for (const auto& header : newHeaders) {
    AtomicString headerName = header.key;
    if (!shouldIgnoreHeaderForCacheReuse(headerName) &&
        header.value != oldHeaders.get(headerName))
      return false;
  }

  for (const auto& header : oldHeaders) {
    AtomicString headerName = header.key;
    if (!shouldIgnoreHeaderForCacheReuse(headerName) &&
        header.value != newHeaders.get(headerName))
      return false;
  }

  return true;
}

RawResourceClientStateChecker::RawResourceClientStateChecker()
    : m_state(NotAddedAsClient) {}

RawResourceClientStateChecker::~RawResourceClientStateChecker() {}

NEVER_INLINE void RawResourceClientStateChecker::willAddClient() {
  SECURITY_CHECK(m_state == NotAddedAsClient);
  m_state = Started;
}

NEVER_INLINE void RawResourceClientStateChecker::willRemoveClient() {
  SECURITY_CHECK(m_state != NotAddedAsClient);
  m_state = NotAddedAsClient;
}

NEVER_INLINE void RawResourceClientStateChecker::redirectReceived() {
  SECURITY_CHECK(m_state == Started);
}

NEVER_INLINE void RawResourceClientStateChecker::redirectBlocked() {
  SECURITY_CHECK(m_state == Started);
  m_state = RedirectBlocked;
}

NEVER_INLINE void RawResourceClientStateChecker::dataSent() {
  SECURITY_CHECK(m_state == Started);
}

NEVER_INLINE void RawResourceClientStateChecker::responseReceived() {
  SECURITY_CHECK(m_state == Started);
  m_state = ResponseReceived;
}

NEVER_INLINE void RawResourceClientStateChecker::setSerializedCachedMetadata() {
  SECURITY_CHECK(m_state == ResponseReceived);
  m_state = SetSerializedCachedMetadata;
}

NEVER_INLINE void RawResourceClientStateChecker::dataReceived() {
  SECURITY_CHECK(m_state == ResponseReceived ||
                 m_state == SetSerializedCachedMetadata ||
                 m_state == DataReceived);
  m_state = DataReceived;
}

NEVER_INLINE void RawResourceClientStateChecker::dataDownloaded() {
  SECURITY_CHECK(m_state == ResponseReceived ||
                 m_state == SetSerializedCachedMetadata ||
                 m_state == DataDownloaded);
  m_state = DataDownloaded;
}

NEVER_INLINE void RawResourceClientStateChecker::notifyFinished(
    Resource* resource) {
  SECURITY_CHECK(m_state != NotAddedAsClient);
  SECURITY_CHECK(m_state != NotifyFinished);
  SECURITY_CHECK(resource->errorOccurred() ||
                 (m_state == ResponseReceived ||
                  m_state == SetSerializedCachedMetadata ||
                  m_state == DataReceived || m_state == DataDownloaded));
  m_state = NotifyFinished;
}

}  // namespace blink
