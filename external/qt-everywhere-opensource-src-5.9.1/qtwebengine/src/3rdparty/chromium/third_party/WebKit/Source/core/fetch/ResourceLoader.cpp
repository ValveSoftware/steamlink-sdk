/*
 * Copyright (C) 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/fetch/ResourceLoader.h"

#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/SharedBuffer.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/network/ResourceError.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebData.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

ResourceLoader* ResourceLoader::create(ResourceFetcher* fetcher,
                                       Resource* resource) {
  return new ResourceLoader(fetcher, resource);
}

ResourceLoader::ResourceLoader(ResourceFetcher* fetcher, Resource* resource)
    : m_fetcher(fetcher),
      m_resource(resource),
      m_isCacheAwareLoadingActivated(false) {
  DCHECK(m_resource);
  DCHECK(m_fetcher);
  m_resource->setLoader(this);
}

ResourceLoader::~ResourceLoader() {}

DEFINE_TRACE(ResourceLoader) {
  visitor->trace(m_fetcher);
  visitor->trace(m_resource);
}

void ResourceLoader::start(const ResourceRequest& request,
                           WebTaskRunner* loadingTaskRunner,
                           bool defersLoading) {
  DCHECK(!m_loader);
  if (m_resource->options().synchronousPolicy == RequestSynchronously &&
      defersLoading) {
    cancel();
    return;
  }

  m_loader = wrapUnique(Platform::current()->createURLLoader());
  DCHECK(m_loader);
  m_loader->setDefersLoading(defersLoading);
  m_loader->setLoadingTaskRunner(loadingTaskRunner);

  if (m_isCacheAwareLoadingActivated) {
    // Override cache policy for cache-aware loading. If this request fails, a
    // reload with original request will be triggered in didFail().
    ResourceRequest cacheAwareRequest(request);
    cacheAwareRequest.setCachePolicy(WebCachePolicy::ReturnCacheDataIfValid);
    m_loader->loadAsynchronously(WrappedResourceRequest(cacheAwareRequest),
                                 this);
    return;
  }

  if (m_resource->options().synchronousPolicy == RequestSynchronously)
    requestSynchronously(request);
  else
    m_loader->loadAsynchronously(WrappedResourceRequest(request), this);
}

void ResourceLoader::restart(const ResourceRequest& request,
                             WebTaskRunner* loadingTaskRunner,
                             bool defersLoading) {
  CHECK_EQ(m_resource->options().synchronousPolicy, RequestAsynchronously);
  m_loader.reset();
  start(request, loadingTaskRunner, defersLoading);
}

void ResourceLoader::setDefersLoading(bool defers) {
  DCHECK(m_loader);
  m_loader->setDefersLoading(defers);
}

void ResourceLoader::didDownloadData(WebURLLoader*,
                                     int length,
                                     int encodedDataLength) {
  m_fetcher->didDownloadData(m_resource.get(), length, encodedDataLength);
  m_resource->didDownloadData(length);
}

void ResourceLoader::didChangePriority(ResourceLoadPriority loadPriority,
                                       int intraPriorityValue) {
  if (m_loader) {
    m_loader->didChangePriority(
        static_cast<WebURLRequest::Priority>(loadPriority), intraPriorityValue);
  }
}

void ResourceLoader::cancel() {
  didFail(
      ResourceError::cancelledError(m_resource->lastResourceRequest().url()));
}

void ResourceLoader::cancelForRedirectAccessCheckError(const KURL& newURL) {
  m_resource->willNotFollowRedirect();

  if (m_loader)
    didFail(ResourceError::cancelledDueToAccessCheckError(newURL));
}

bool ResourceLoader::willFollowRedirect(
    WebURLLoader*,
    WebURLRequest& passedNewRequest,
    const WebURLResponse& passedRedirectResponse) {
  DCHECK(!passedNewRequest.isNull());
  DCHECK(!passedRedirectResponse.isNull());

  if (m_isCacheAwareLoadingActivated) {
    // Fail as cache miss if cached response is a redirect.
    didFail(
        ResourceError::cacheMissError(m_resource->lastResourceRequest().url()));
    return false;
  }

  ResourceRequest& newRequest(passedNewRequest.toMutableResourceRequest());
  const ResourceResponse& redirectResponse(
      passedRedirectResponse.toResourceResponse());
  newRequest.setRedirectStatus(
      ResourceRequest::RedirectStatus::FollowedRedirect);

  const KURL originalURL = newRequest.url();

  if (!m_fetcher->willFollowRedirect(m_resource.get(), newRequest,
                                     redirectResponse)) {
    cancelForRedirectAccessCheckError(newRequest.url());
    return false;
  }

  // ResourceFetcher::willFollowRedirect() may rewrite the URL to
  // something else not for rejecting redirect but for other reasons.
  // E.g. WebFrameTestClient::willSendRequest() and
  // RenderFrameImpl::willSendRequest(). We should reflect the
  // rewriting but currently we cannot. So, return false to make the
  // redirect fail.
  if (newRequest.url() != originalURL) {
    cancelForRedirectAccessCheckError(newRequest.url());
    return false;
  }

  if (!m_resource->willFollowRedirect(newRequest, redirectResponse)) {
    cancelForRedirectAccessCheckError(newRequest.url());
    return false;
  }

  return true;
}

void ResourceLoader::didReceiveCachedMetadata(WebURLLoader*,
                                              const char* data,
                                              int length) {
  m_resource->setSerializedCachedMetadata(data, length);
}

void ResourceLoader::didSendData(WebURLLoader*,
                                 unsigned long long bytesSent,
                                 unsigned long long totalBytesToBeSent) {
  m_resource->didSendData(bytesSent, totalBytesToBeSent);
}

void ResourceLoader::didReceiveResponse(WebURLLoader*,
                                        const WebURLResponse& response,
                                        WebDataConsumerHandle* handle) {
  DCHECK(!response.isNull());
  m_fetcher->didReceiveResponse(m_resource.get(), response.toResourceResponse(),
                                handle);
}

void ResourceLoader::didReceiveResponse(WebURLLoader* loader,
                                        const WebURLResponse& response) {
  didReceiveResponse(loader, response, nullptr);
}

void ResourceLoader::didReceiveData(WebURLLoader*,
                                    const char* data,
                                    int length,
                                    int encodedDataLength,
                                    int encodedBodyLength) {
  CHECK_GE(length, 0);
  m_fetcher->didReceiveData(m_resource.get(), data, length, encodedDataLength);
  m_resource->addToEncodedBodyLength(encodedBodyLength);
  m_resource->addToDecodedBodyLength(length);
  m_resource->appendData(data, length);
}

void ResourceLoader::didFinishLoadingFirstPartInMultipart() {
  m_fetcher->didFinishLoading(m_resource.get(), 0,
                              WebURLLoaderClient::kUnknownEncodedDataLength,
                              ResourceFetcher::DidFinishFirstPartInMultipart);
}

void ResourceLoader::didFinishLoading(WebURLLoader*,
                                      double finishTime,
                                      int64_t encodedDataLength) {
  m_loader.reset();
  m_fetcher->didFinishLoading(m_resource.get(), finishTime, encodedDataLength,
                              ResourceFetcher::DidFinishLoading);
}

void ResourceLoader::didFail(WebURLLoader*, const WebURLError& error) {
  didFail(error);
}

void ResourceLoader::didFail(const ResourceError& error) {
  if (m_isCacheAwareLoadingActivated && error.isCacheMiss() &&
      m_fetcher->context().shouldLoadNewResource(m_resource->getType())) {
    m_resource->willReloadAfterDiskCacheMiss();
    m_isCacheAwareLoadingActivated = false;
    restart(m_resource->resourceRequest(),
            m_fetcher->context().loadingTaskRunner(),
            m_fetcher->context().defersLoading());
    return;
  }

  m_loader.reset();
  m_fetcher->didFailLoading(m_resource.get(), error);
}

void ResourceLoader::requestSynchronously(const ResourceRequest& request) {
  // downloadToFile is not supported for synchronous requests.
  DCHECK(!request.downloadToFile());
  DCHECK(m_loader);
  DCHECK_EQ(request.priority(), ResourceLoadPriorityHighest);

  WrappedResourceRequest requestIn(request);
  WebURLResponse responseOut;
  WebURLError errorOut;
  WebData dataOut;
  int64_t encodedDataLength = WebURLLoaderClient::kUnknownEncodedDataLength;
  m_loader->loadSynchronously(requestIn, responseOut, errorOut, dataOut,
                              encodedDataLength);

  // A message dispatched while synchronously fetching the resource
  // can bring about the cancellation of this load.
  if (!m_loader)
    return;
  if (errorOut.reason) {
    didFail(0, errorOut);
    return;
  }
  didReceiveResponse(0, responseOut);
  if (!m_loader)
    return;
  DCHECK_GE(responseOut.toResourceResponse().encodedBodyLength(), 0);

  // Follow the async case convention of not calling didReceiveData or
  // appending data to m_resource if the response body is empty. Copying the
  // empty buffer is a noop in most cases, but is destructive in the case of
  // a 304, where it will overwrite the cached data we should be reusing.
  if (dataOut.size()) {
    m_fetcher->didReceiveData(m_resource.get(), dataOut.data(), dataOut.size(),
                              encodedDataLength);
    m_resource->setResourceBuffer(dataOut);
  }
  didFinishLoading(0, monotonicallyIncreasingTime(), encodedDataLength);
}

void ResourceLoader::dispose() {
  m_loader = nullptr;
}

void ResourceLoader::activateCacheAwareLoadingIfNeeded(
    const ResourceRequest& request) {
  DCHECK(!m_isCacheAwareLoadingActivated);

  if (m_resource->options().cacheAwareLoadingEnabled !=
      IsCacheAwareLoadingEnabled)
    return;

  // Synchronous requests are not supported.
  if (m_resource->options().synchronousPolicy == RequestSynchronously)
    return;

  // Don't activate on Resource revalidation.
  if (m_resource->isCacheValidator())
    return;

  // Don't activate if cache policy is explicitly set.
  if (request.getCachePolicy() != WebCachePolicy::UseProtocolCachePolicy)
    return;

  m_isCacheAwareLoadingActivated = true;
}

}  // namespace blink
