// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/CSSPreloadScanner.h"

#include "core/fetch/FetchContext.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/Settings.h"
#include "core/html/parser/HTMLResourcePreloader.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Heap.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

namespace {

class MockHTMLResourcePreloader : public HTMLResourcePreloader {
  WTF_MAKE_NONCOPYABLE(MockHTMLResourcePreloader);

 public:
  MockHTMLResourcePreloader(Document& document)
      : HTMLResourcePreloader(document) {}

  void preload(std::unique_ptr<PreloadRequest> preloadRequest,
               const NetworkHintsInterface&) override {}
};

class PreloadRecordingCSSPreloaderResourceClient final
    : public CSSPreloaderResourceClient {
 public:
  PreloadRecordingCSSPreloaderResourceClient(Resource* resource,
                                             HTMLResourcePreloader* preloader)
      : CSSPreloaderResourceClient(resource, preloader) {}

  void fetchPreloads(PreloadRequestStream& preloads) override {
    for (const auto& it : preloads)
      m_preloadUrls.append(it->resourceURL());
    CSSPreloaderResourceClient::fetchPreloads(preloads);
  }

  Vector<String> m_preloadUrls;
};

class CSSPreloadScannerTest : public ::testing::Test {};

}  // namespace

TEST_F(CSSPreloadScannerTest, ScanFromResourceClient) {
  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(500, 500));
  dummyPageHolder->document().settings()->setCSSExternalScannerNoPreload(true);

  MockHTMLResourcePreloader* preloader =
      new MockHTMLResourcePreloader(dummyPageHolder->document());

  KURL url(ParsedURLString, "http://127.0.0.1/foo.css");
  CSSStyleSheetResource* resource =
      CSSStyleSheetResource::createForTest(ResourceRequest(url), "utf-8");
  resource->setStatus(Resource::Pending);

  PreloadRecordingCSSPreloaderResourceClient* resourceClient =
      new PreloadRecordingCSSPreloaderResourceClient(resource, preloader);

  const char* data = "@import url('http://127.0.0.1/preload.css');";
  resource->appendData(data, strlen(data));

  EXPECT_EQ(1u, resourceClient->m_preloadUrls.size());
  EXPECT_EQ("http://127.0.0.1/preload.css",
            resourceClient->m_preloadUrls.first());
}

// Regression test for crbug.com/608310 where the client is destroyed but was
// not removed from the resource's client list.
TEST_F(CSSPreloadScannerTest, DestroyClientBeforeDataSent) {
  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(500, 500));
  dummyPageHolder->document().settings()->setCSSExternalScannerNoPreload(true);

  Persistent<MockHTMLResourcePreloader> preloader =
      new MockHTMLResourcePreloader(dummyPageHolder->document());

  KURL url(ParsedURLString, "http://127.0.0.1/foo.css");
  Persistent<CSSStyleSheetResource> resource =
      CSSStyleSheetResource::createForTest(ResourceRequest(url), "utf-8");
  resource->setStatus(Resource::Pending);

  new PreloadRecordingCSSPreloaderResourceClient(resource, preloader);

  // Destroys the resourceClient.
  ThreadState::current()->collectAllGarbage();

  const char* data = "@import url('http://127.0.0.1/preload.css');";
  // Should not crash.
  resource->appendData(data, strlen(data));
}

// Regression test for crbug.com/646869 where the client's data is cleared
// before didAppendFirstData is called.
TEST_F(CSSPreloadScannerTest, DontReadFromClearedData) {
  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(500, 500));
  dummyPageHolder->document().settings()->setCSSExternalScannerNoPreload(true);

  MockHTMLResourcePreloader* preloader =
      new MockHTMLResourcePreloader(dummyPageHolder->document());

  KURL url(ParsedURLString, "http://127.0.0.1/foo.css");
  CSSStyleSheetResource* resource =
      CSSStyleSheetResource::createForTest(ResourceRequest(url), "utf-8");

  const char* data = "@import url('http://127.0.0.1/preload.css');";
  resource->appendData(data, strlen(data));
  ResourceError error(errorDomainBlinkInternal, 0, url.getString(), "");
  resource->error(error);

  // Should not crash.
  PreloadRecordingCSSPreloaderResourceClient* resourceClient =
      new PreloadRecordingCSSPreloaderResourceClient(resource, preloader);

  EXPECT_EQ(0u, resourceClient->m_preloadUrls.size());
}

// Regression test for crbug.com/645331, where a resource client gets callbacks
// after the document is shutdown and we have no frame.
TEST_F(CSSPreloadScannerTest, DoNotExpectValidDocument) {
  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(500, 500));
  dummyPageHolder->document().settings()->setCSSExternalScannerNoPreload(true);

  MockHTMLResourcePreloader* preloader =
      new MockHTMLResourcePreloader(dummyPageHolder->document());

  KURL url(ParsedURLString, "http://127.0.0.1/foo.css");
  CSSStyleSheetResource* resource =
      CSSStyleSheetResource::createForTest(ResourceRequest(url), "utf-8");
  resource->setStatus(Resource::Pending);

  PreloadRecordingCSSPreloaderResourceClient* resourceClient =
      new PreloadRecordingCSSPreloaderResourceClient(resource, preloader);

  dummyPageHolder->document().shutdown();

  const char* data = "@import url('http://127.0.0.1/preload.css');";
  resource->appendData(data, strlen(data));

  // Do not expect to gather any preloads, as the document loader is invalid,
  // which means we can't notify WebLoadingBehaviorData of the preloads.
  EXPECT_EQ(0u, resourceClient->m_preloadUrls.size());
}

}  // namespace blink
