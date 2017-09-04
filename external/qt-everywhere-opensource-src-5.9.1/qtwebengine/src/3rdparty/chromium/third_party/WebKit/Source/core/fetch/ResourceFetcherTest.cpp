/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/fetch/ResourceFetcher.h"

#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/MockFetchContext.h"
#include "core/fetch/MockResourceClients.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/ResourceLoader.h"
#include "platform/WebTaskRunner.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/heap/Handle.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/Member.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/weburl_loader_mock.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

namespace {
const char testImageFilename[] = "white-1x1.png";
const int testImageSize = 103;  // size of web/tests/data/white-1x1.png
}

class ResourceFetcherTest : public ::testing::Test {};

TEST_F(ResourceFetcherTest, StartLoadAfterFrameDetach) {
  KURL secureURL(ParsedURLString, "https://secureorigin.test/image.png");
  // Try to request a url. The request should fail, and a resource in an error
  // state should be returned, and no resource should be present in the cache.
  ResourceFetcher* fetcher = ResourceFetcher::create(nullptr);
  ResourceRequest resourceRequest(secureURL);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest =
      FetchRequest(resourceRequest, FetchInitiatorInfo());
  Resource* resource = RawResource::fetch(fetchRequest, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->errorOccurred());
  EXPECT_TRUE(resource->resourceError().isAccessCheck());
  EXPECT_FALSE(memoryCache()->resourceForURL(secureURL));

  // Start by calling startLoad() directly, rather than via requestResource().
  // This shouldn't crash.
  fetcher->startLoad(RawResource::create(secureURL, Resource::Raw));
}

TEST_F(ResourceFetcherTest, UseExistingResource) {
  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  URLTestHelpers::registerMockedURLLoadWithCustomResponse(
      url, testImageFilename, WebString::fromUTF8(""),
      WrappedResourceResponse(response));

  FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
  Resource* resource = ImageResource::fetch(fetchRequest, fetcher);
  ASSERT_TRUE(resource);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  EXPECT_TRUE(resource->isLoaded());
  EXPECT_TRUE(memoryCache()->contains(resource));

  Resource* newResource = ImageResource::fetch(fetchRequest, fetcher);
  EXPECT_EQ(resource, newResource);
  memoryCache()->remove(resource);
}

TEST_F(ResourceFetcherTest, Vary) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  Resource* resource = RawResource::create(url, Resource::Raw);
  memoryCache()->add(resource);
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.setHTTPHeaderField(HTTPNames::Vary, "*");
  resource->responseReceived(response, nullptr);
  resource->finish();
  ASSERT_TRUE(resource->hasVaryHeader());

  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));
  ResourceRequest resourceRequest(url);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest =
      FetchRequest(resourceRequest, FetchInitiatorInfo());
  Platform::current()->getURLLoaderMockFactory()->registerURL(
      url, WebURLResponse(), "");
  Resource* newResource = RawResource::fetch(fetchRequest, fetcher);
  EXPECT_NE(resource, newResource);
  newResource->loader()->cancel();
  memoryCache()->remove(newResource);
  Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);

  memoryCache()->remove(resource);
}

TEST_F(ResourceFetcherTest, VaryOnBack) {
  MockFetchContext* context =
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource);
  context->setCachePolicy(CachePolicyHistoryBuffer);
  ResourceFetcher* fetcher = ResourceFetcher::create(context);

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  Resource* resource = RawResource::create(url, Resource::Raw);
  memoryCache()->add(resource);
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.setHTTPHeaderField(HTTPNames::Vary, "*");
  resource->responseReceived(response, nullptr);
  resource->finish();
  ASSERT_TRUE(resource->hasVaryHeader());

  ResourceRequest resourceRequest(url);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest =
      FetchRequest(resourceRequest, FetchInitiatorInfo());
  Resource* newResource = RawResource::fetch(fetchRequest, fetcher);
  EXPECT_EQ(resource, newResource);

  memoryCache()->remove(newResource);
}

TEST_F(ResourceFetcherTest, VaryImage) {
  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.setHTTPHeaderField(HTTPNames::Vary, "*");
  URLTestHelpers::registerMockedURLLoadWithCustomResponse(
      url, testImageFilename, WebString::fromUTF8(""),
      WrappedResourceResponse(response));

  FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
  Resource* resource = ImageResource::fetch(fetchRequestOriginal, fetcher);
  ASSERT_TRUE(resource);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  ASSERT_TRUE(resource->hasVaryHeader());

  FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource = ImageResource::fetch(fetchRequest, fetcher);
  EXPECT_EQ(resource, newResource);

  memoryCache()->remove(newResource);
  Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
}

class RequestSameResourceOnComplete
    : public GarbageCollectedFinalized<RequestSameResourceOnComplete>,
      public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(RequestSameResourceOnComplete);

 public:
  explicit RequestSameResourceOnComplete(Resource* resource)
      : m_resource(resource), m_notifyFinishedCalled(false) {}

  void notifyFinished(Resource* resource) override {
    EXPECT_EQ(m_resource, resource);
    MockFetchContext* context =
        MockFetchContext::create(MockFetchContext::kShouldLoadNewResource);
    context->setCachePolicy(CachePolicyRevalidate);
    ResourceFetcher* fetcher2 = ResourceFetcher::create(context);
    FetchRequest fetchRequest2(m_resource->url(), FetchInitiatorInfo());
    Resource* resource2 = ImageResource::fetch(fetchRequest2, fetcher2);
    EXPECT_EQ(m_resource, resource2);
    m_notifyFinishedCalled = true;
  }
  bool notifyFinishedCalled() const { return m_notifyFinishedCalled; }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_resource);
    RawResourceClient::trace(visitor);
  }

  String debugName() const override { return "RequestSameResourceOnComplete"; }

 private:
  Member<Resource> m_resource;
  bool m_notifyFinishedCalled;
};

TEST_F(ResourceFetcherTest, RevalidateWhileFinishingLoading) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.setHTTPHeaderField(HTTPNames::ETag, "1234567890");
  URLTestHelpers::registerMockedURLLoadWithCustomResponse(
      url, testImageFilename, WebString::fromUTF8(""),
      WrappedResourceResponse(response));
  ResourceFetcher* fetcher1 = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));
  ResourceRequest request1(url);
  request1.setHTTPHeaderField(HTTPNames::Cache_Control, "no-cache");
  FetchRequest fetchRequest1 = FetchRequest(request1, FetchInitiatorInfo());
  Resource* resource1 = ImageResource::fetch(fetchRequest1, fetcher1);
  Persistent<RequestSameResourceOnComplete> client =
      new RequestSameResourceOnComplete(resource1);
  resource1->addClient(client);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
  EXPECT_TRUE(client->notifyFinishedCalled());
  resource1->removeClient(client);
  memoryCache()->remove(resource1);
}

TEST_F(ResourceFetcherTest, DontReuseMediaDataUrl) {
  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));
  ResourceRequest request(KURL(ParsedURLString, "data:text/html,foo"));
  request.setRequestContext(WebURLRequest::RequestContextVideo);
  ResourceLoaderOptions options;
  options.dataBufferingPolicy = DoNotBufferData;
  FetchRequest fetchRequest =
      FetchRequest(request, FetchInitiatorTypeNames::internal, options);
  Resource* resource1 = RawResource::fetchMedia(fetchRequest, fetcher);
  Resource* resource2 = RawResource::fetchMedia(fetchRequest, fetcher);
  EXPECT_NE(resource1, resource2);
  memoryCache()->remove(resource2);
}

class ServeRequestsOnCompleteClient final
    : public GarbageCollectedFinalized<ServeRequestsOnCompleteClient>,
      public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(ServeRequestsOnCompleteClient);

 public:
  void notifyFinished(Resource*) override {
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  }

  // No callbacks should be received except for the notifyFinished() triggered
  // by ResourceLoader::cancel().
  void dataSent(Resource*, unsigned long long, unsigned long long) override {
    ASSERT_TRUE(false);
  }
  void responseReceived(Resource*,
                        const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) override {
    ASSERT_TRUE(false);
  }
  void setSerializedCachedMetadata(Resource*, const char*, size_t) override {
    ASSERT_TRUE(false);
  }
  void dataReceived(Resource*, const char*, size_t) override {
    ASSERT_TRUE(false);
  }
  bool redirectReceived(Resource*,
                        const ResourceRequest&,
                        const ResourceResponse&) override {
    ADD_FAILURE();
    return true;
  }
  void dataDownloaded(Resource*, int) override { ASSERT_TRUE(false); }
  void didReceiveResourceTiming(Resource*, const ResourceTimingInfo&) override {
    ASSERT_TRUE(false);
  }

  DEFINE_INLINE_TRACE() { RawResourceClient::trace(visitor); }

  String debugName() const override { return "ServeRequestsOnCompleteClient"; }
};

// Regression test for http://crbug.com/594072.
// This emulates a modal dialog triggering a nested run loop inside
// ResourceLoader::cancel(). If the ResourceLoader doesn't promptly cancel its
// WebURLLoader before notifying its clients, a nested run loop  may send a
// network response, leading to an invalid state transition in ResourceLoader.
TEST_F(ResourceFetcherTest, ResponseOnCancel) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  URLTestHelpers::registerMockedURLLoad(url, testImageFilename, "image/png");

  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));
  ResourceRequest resourceRequest(url);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest =
      FetchRequest(resourceRequest, FetchInitiatorInfo());
  Resource* resource = RawResource::fetch(fetchRequest, fetcher);
  Persistent<ServeRequestsOnCompleteClient> client =
      new ServeRequestsOnCompleteClient();
  resource->addClient(client);
  resource->loader()->cancel();
  resource->removeClient(client);
  Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
}

class ScopedMockRedirectRequester {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ScopedMockRedirectRequester);

 public:
  ScopedMockRedirectRequester() : m_context(nullptr) {}

  ~ScopedMockRedirectRequester() { cleanUp(); }

  void registerRedirect(const WebString& fromURL, const WebString& toURL) {
    KURL redirectURL(ParsedURLString, fromURL);
    WebURLResponse redirectResponse;
    redirectResponse.setURL(redirectURL);
    redirectResponse.setHTTPStatusCode(301);
    redirectResponse.setHTTPHeaderField(HTTPNames::Location, toURL);
    redirectResponse.addToEncodedDataLength(kRedirectResponseOverheadBytes);
    Platform::current()->getURLLoaderMockFactory()->registerURL(
        redirectURL, redirectResponse, "");
  }

  void registerFinalResource(const WebString& url) {
    KURL finalURL(ParsedURLString, url);
    URLTestHelpers::registerMockedURLLoad(finalURL, testImageFilename);
  }

  void request(const WebString& url) {
    DCHECK(!m_context);
    m_context =
        MockFetchContext::create(MockFetchContext::kShouldLoadNewResource);
    ResourceFetcher* fetcher = ResourceFetcher::create(m_context);
    ResourceRequest resourceRequest(url);
    resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
    FetchRequest fetchRequest =
        FetchRequest(resourceRequest, FetchInitiatorInfo());
    RawResource::fetch(fetchRequest, fetcher);
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  }

  void cleanUp() {
    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
    memoryCache()->evictResources();
  }

  MockFetchContext* context() const { return m_context; }

 private:
  Member<MockFetchContext> m_context;
};

TEST_F(ResourceFetcherTest, SameOriginRedirect) {
  const char redirectURL[] = "http://127.0.0.1:8000/redirect.html";
  const char finalURL[] = "http://127.0.0.1:8000/final.html";
  ScopedMockRedirectRequester requester;
  requester.registerRedirect(redirectURL, finalURL);
  requester.registerFinalResource(finalURL);
  requester.request(redirectURL);

  EXPECT_EQ(kRedirectResponseOverheadBytes + testImageSize,
            requester.context()->getTransferSize());
}

TEST_F(ResourceFetcherTest, CrossOriginRedirect) {
  const char redirectURL[] = "http://otherorigin.test/redirect.html";
  const char finalURL[] = "http://127.0.0.1:8000/final.html";
  ScopedMockRedirectRequester requester;
  requester.registerRedirect(redirectURL, finalURL);
  requester.registerFinalResource(finalURL);
  requester.request(redirectURL);

  EXPECT_EQ(testImageSize, requester.context()->getTransferSize());
}

TEST_F(ResourceFetcherTest, ComplexCrossOriginRedirect) {
  const char redirectURL1[] = "http://127.0.0.1:8000/redirect1.html";
  const char redirectURL2[] = "http://otherorigin.test/redirect2.html";
  const char redirectURL3[] = "http://127.0.0.1:8000/redirect3.html";
  const char finalURL[] = "http://127.0.0.1:8000/final.html";
  ScopedMockRedirectRequester requester;
  requester.registerRedirect(redirectURL1, redirectURL2);
  requester.registerRedirect(redirectURL2, redirectURL3);
  requester.registerRedirect(redirectURL3, finalURL);
  requester.registerFinalResource(finalURL);
  requester.request(redirectURL1);

  EXPECT_EQ(testImageSize, requester.context()->getTransferSize());
}

TEST_F(ResourceFetcherTest, SynchronousRequest) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  URLTestHelpers::registerMockedURLLoad(url, testImageFilename, "image/png");

  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));
  ResourceRequest resourceRequest(url);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest(resourceRequest, FetchInitiatorInfo());
  fetchRequest.makeSynchronous();
  Resource* resource = RawResource::fetch(fetchRequest, fetcher);
  EXPECT_TRUE(resource->isLoaded());
  EXPECT_EQ(ResourceLoadPriorityHighest,
            resource->resourceRequest().priority());

  Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
  memoryCache()->remove(resource);
}

TEST_F(ResourceFetcherTest, PreloadImageTwice) {
  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  URLTestHelpers::registerMockedURLLoad(url, testImageFilename, "image/png");

  FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
  Resource* resource = ImageResource::fetch(fetchRequestOriginal, fetcher);
  ASSERT_TRUE(resource);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  fetcher->preloadStarted(resource);

  FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource = ImageResource::fetch(fetchRequest, fetcher);
  EXPECT_EQ(resource, newResource);
  fetcher->preloadStarted(resource);

  fetcher->clearPreloads(ResourceFetcher::ClearAllPreloads);
  Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
  EXPECT_FALSE(memoryCache()->contains(resource));
  EXPECT_FALSE(resource->isPreloaded());
}

TEST_F(ResourceFetcherTest, LinkPreloadImageAndUse) {
  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  URLTestHelpers::registerMockedURLLoad(url, testImageFilename, "image/png");

  // Link preload preload scanner
  FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
  fetchRequestOriginal.setLinkPreload(true);
  Resource* resource = ImageResource::fetch(fetchRequestOriginal, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->isLinkPreload());
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  fetcher->preloadStarted(resource);

  // Image preload scanner
  FetchRequest fetchRequestPreloadScanner =
      FetchRequest(url, FetchInitiatorInfo());
  Resource* imgPreloadScannerResource =
      ImageResource::fetch(fetchRequestPreloadScanner, fetcher);
  EXPECT_EQ(resource, imgPreloadScannerResource);
  EXPECT_FALSE(resource->isLinkPreload());
  fetcher->preloadStarted(resource);

  // Image created by parser
  FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource = ImageResource::fetch(fetchRequest, fetcher);
  Persistent<MockResourceClient> client = new MockResourceClient(newResource);
  EXPECT_EQ(resource, newResource);
  EXPECT_FALSE(resource->isLinkPreload());

  // DCL reached
  fetcher->clearPreloads(ResourceFetcher::ClearSpeculativeMarkupPreloads);
  Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
  EXPECT_TRUE(memoryCache()->contains(resource));
  EXPECT_FALSE(resource->isPreloaded());
}

TEST_F(ResourceFetcherTest, LinkPreloadImageMultipleFetchersAndUse) {
  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));
  ResourceFetcher* fetcher2 = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  URLTestHelpers::registerMockedURLLoad(url, testImageFilename, "image/png");

  FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
  fetchRequestOriginal.setLinkPreload(true);
  Resource* resource = ImageResource::fetch(fetchRequestOriginal, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->isLinkPreload());
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  fetcher->preloadStarted(resource);

  FetchRequest fetchRequestSecond = FetchRequest(url, FetchInitiatorInfo());
  fetchRequestSecond.setLinkPreload(true);
  Resource* secondResource = ImageResource::fetch(fetchRequestSecond, fetcher2);
  ASSERT_TRUE(secondResource);
  EXPECT_TRUE(secondResource->isLinkPreload());
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  fetcher2->preloadStarted(secondResource);

  // Link rel preload scanner
  FetchRequest fetchRequestLinkPreloadScanner =
      FetchRequest(url, FetchInitiatorInfo());
  fetchRequestLinkPreloadScanner.setLinkPreload(true);
  Resource* linkPreloadScannerResource =
      ImageResource::fetch(fetchRequestLinkPreloadScanner, fetcher);
  EXPECT_EQ(resource, linkPreloadScannerResource);
  EXPECT_TRUE(resource->isLinkPreload());
  fetcher->preloadStarted(resource);

  // Image preload scanner
  FetchRequest fetchRequestPreloadScanner =
      FetchRequest(url, FetchInitiatorInfo());
  Resource* imgPreloadScannerResource =
      ImageResource::fetch(fetchRequestPreloadScanner, fetcher);
  EXPECT_EQ(resource, imgPreloadScannerResource);
  EXPECT_FALSE(resource->isLinkPreload());
  fetcher->preloadStarted(resource);

  // Image preload scanner on the second fetcher
  FetchRequest fetchRequestPreloadScanner2 =
      FetchRequest(url, FetchInitiatorInfo());
  Resource* imgPreloadScannerResource2 =
      ImageResource::fetch(fetchRequestPreloadScanner2, fetcher2);
  EXPECT_EQ(resource, imgPreloadScannerResource2);
  EXPECT_FALSE(resource->isLinkPreload());
  fetcher2->preloadStarted(resource);

  // Image created by parser
  FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource = ImageResource::fetch(fetchRequest, fetcher);
  Persistent<MockResourceClient> client = new MockResourceClient(newResource);
  EXPECT_EQ(resource, newResource);
  EXPECT_FALSE(resource->isLinkPreload());

  // Image created by parser on the second fetcher
  FetchRequest fetchRequest2 = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource2 = ImageResource::fetch(fetchRequest, fetcher2);
  Persistent<MockResourceClient> client2 = new MockResourceClient(newResource2);
  EXPECT_EQ(resource, newResource2);
  EXPECT_FALSE(resource->isLinkPreload());
  // DCL reached on first fetcher
  EXPECT_TRUE(resource->isPreloaded());
  fetcher->clearPreloads(ResourceFetcher::ClearSpeculativeMarkupPreloads);
  Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
  EXPECT_TRUE(memoryCache()->contains(resource));
  EXPECT_TRUE(resource->isPreloaded());

  // DCL reached on second fetcher
  fetcher2->clearPreloads(ResourceFetcher::ClearSpeculativeMarkupPreloads);
  EXPECT_TRUE(memoryCache()->contains(resource));
  EXPECT_FALSE(resource->isPreloaded());
}

TEST_F(ResourceFetcherTest, Revalidate304) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  Resource* resource = RawResource::create(url, Resource::Raw);
  memoryCache()->add(resource);
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(304);
  response.setHTTPHeaderField("etag", "1234567890");
  resource->responseReceived(response, nullptr);
  resource->finish();

  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));
  ResourceRequest resourceRequest(url);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest =
      FetchRequest(resourceRequest, FetchInitiatorInfo());
  Platform::current()->getURLLoaderMockFactory()->registerURL(
      url, WebURLResponse(), "");
  Resource* newResource = RawResource::fetch(fetchRequest, fetcher);
  fetcher->stopFetching();
  Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);

  EXPECT_NE(resource, newResource);
}

TEST_F(ResourceFetcherTest, LinkPreloadImageMultipleFetchersAndMove) {
  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));
  ResourceFetcher* fetcher2 = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  URLTestHelpers::registerMockedURLLoad(url, testImageFilename, "image/png");

  FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
  fetchRequestOriginal.setLinkPreload(true);
  Resource* resource = ImageResource::fetch(fetchRequestOriginal, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->isLinkPreload());
  fetcher->preloadStarted(resource);

  // Image created by parser on the second fetcher
  FetchRequest fetchRequest2 = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource2 = ImageResource::fetch(fetchRequest2, fetcher2);
  Persistent<MockResourceClient> client2 = new MockResourceClient(newResource2);
  EXPECT_EQ(resource, newResource2);
  EXPECT_FALSE(fetcher2->isFetching());
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
}

}  // namespace blink
