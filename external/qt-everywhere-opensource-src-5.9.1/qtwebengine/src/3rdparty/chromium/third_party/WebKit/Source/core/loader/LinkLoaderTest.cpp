// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/LinkLoader.h"

#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/Settings.h"
#include "core/html/LinkRelAttribute.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/LinkLoaderClient.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <base/macros.h>
#include <memory>

namespace blink {

class MockLinkLoaderClient final
    : public GarbageCollectedFinalized<MockLinkLoaderClient>,
      public LinkLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockLinkLoaderClient);

 public:
  static MockLinkLoaderClient* create(bool shouldLoad) {
    return new MockLinkLoaderClient(shouldLoad);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { LinkLoaderClient::trace(visitor); }

  bool shouldLoadLink() override { return m_shouldLoad; }

  void linkLoaded() override {}
  void linkLoadingErrored() override {}
  void didStartLinkPrerender() override {}
  void didStopLinkPrerender() override {}
  void didSendLoadForLinkPrerender() override {}
  void didSendDOMContentLoadedForLinkPrerender() override {}

 private:
  explicit MockLinkLoaderClient(bool shouldLoad) : m_shouldLoad(shouldLoad) {}

  bool m_shouldLoad;
};

class NetworkHintsMock : public NetworkHintsInterface {
 public:
  NetworkHintsMock() : m_didDnsPrefetch(false), m_didPreconnect(false) {}

  void dnsPrefetchHost(const String& host) const override {
    m_didDnsPrefetch = true;
  }

  void preconnectHost(
      const KURL& host,
      const CrossOriginAttributeValue crossOrigin) const override {
    m_didPreconnect = true;
    m_isHTTPS = host.protocolIs("https");
    m_isCrossOrigin = (crossOrigin == CrossOriginAttributeAnonymous);
  }

  bool didDnsPrefetch() { return m_didDnsPrefetch; }
  bool didPreconnect() { return m_didPreconnect; }
  bool isHTTPS() { return m_isHTTPS; }
  bool isCrossOrigin() { return m_isCrossOrigin; }

 private:
  mutable bool m_didDnsPrefetch;
  mutable bool m_didPreconnect;
  mutable bool m_isHTTPS;
  mutable bool m_isCrossOrigin;
};

TEST(LinkLoaderTest, Preload) {
  struct TestCase {
    const char* href;
    const char* as;
    const char* type;
    const char* media;
    const ReferrerPolicy referrerPolicy;
    const ResourceLoadPriority priority;
    const WebURLRequest::RequestContext context;
    const bool linkLoaderShouldLoadValue;
    const bool expectingLoad;
    const ReferrerPolicy expectedReferrerPolicy;
  } cases[] = {
      {"http://example.test/cat.jpg", "image", "", "", ReferrerPolicyDefault,
       ResourceLoadPriorityLow, WebURLRequest::RequestContextImage, true, true,
       ReferrerPolicyDefault},
      {"http://example.test/cat.js", "script", "", "", ReferrerPolicyDefault,
       ResourceLoadPriorityHigh, WebURLRequest::RequestContextScript, true,
       true, ReferrerPolicyDefault},
      {"http://example.test/cat.css", "style", "", "", ReferrerPolicyDefault,
       ResourceLoadPriorityVeryHigh, WebURLRequest::RequestContextStyle, true,
       true, ReferrerPolicyDefault},
      // TODO(yoav): It doesn't seem like the audio context is ever used. That
      // should probably be fixed (or we can consolidate audio and video).
      {"http://example.test/cat.wav", "media", "", "", ReferrerPolicyDefault,
       ResourceLoadPriorityLow, WebURLRequest::RequestContextVideo, true, true,
       ReferrerPolicyDefault},
      {"http://example.test/cat.mp4", "media", "", "", ReferrerPolicyDefault,
       ResourceLoadPriorityLow, WebURLRequest::RequestContextVideo, true, true,
       ReferrerPolicyDefault},
      {"http://example.test/cat.vtt", "track", "", "", ReferrerPolicyDefault,
       ResourceLoadPriorityLow, WebURLRequest::RequestContextTrack, true, true,
       ReferrerPolicyDefault},
      {"http://example.test/cat.woff", "font", "", "", ReferrerPolicyDefault,
       ResourceLoadPriorityVeryHigh, WebURLRequest::RequestContextFont, true,
       true, ReferrerPolicyDefault},
      // TODO(yoav): subresource should be *very* low priority (rather than
      // low).
      {"http://example.test/cat.empty", "", "", "", ReferrerPolicyDefault,
       ResourceLoadPriorityHigh, WebURLRequest::RequestContextSubresource, true,
       true, ReferrerPolicyDefault},
      {"http://example.test/cat.blob", "blabla", "", "", ReferrerPolicyDefault,
       ResourceLoadPriorityLow, WebURLRequest::RequestContextSubresource, false,
       false, ReferrerPolicyDefault},
      {"bla://example.test/cat.gif", "image", "", "", ReferrerPolicyDefault,
       ResourceLoadPriorityUnresolved, WebURLRequest::RequestContextImage,
       false, false, ReferrerPolicyDefault},
      // MIME type tests
      {"http://example.test/cat.webp", "image", "image/webp", "",
       ReferrerPolicyDefault, ResourceLoadPriorityLow,
       WebURLRequest::RequestContextImage, true, true, ReferrerPolicyDefault},
      {"http://example.test/cat.svg", "image", "image/svg+xml", "",
       ReferrerPolicyDefault, ResourceLoadPriorityLow,
       WebURLRequest::RequestContextImage, true, true, ReferrerPolicyDefault},
      {"http://example.test/cat.jxr", "image", "image/jxr", "",
       ReferrerPolicyDefault, ResourceLoadPriorityUnresolved,
       WebURLRequest::RequestContextImage, false, false, ReferrerPolicyDefault},
      {"http://example.test/cat.js", "script", "text/javascript", "",
       ReferrerPolicyDefault, ResourceLoadPriorityHigh,
       WebURLRequest::RequestContextScript, true, true, ReferrerPolicyDefault},
      {"http://example.test/cat.js", "script", "text/coffeescript", "",
       ReferrerPolicyDefault, ResourceLoadPriorityUnresolved,
       WebURLRequest::RequestContextScript, false, false,
       ReferrerPolicyDefault},
      {"http://example.test/cat.css", "style", "text/css", "",
       ReferrerPolicyDefault, ResourceLoadPriorityVeryHigh,
       WebURLRequest::RequestContextStyle, true, true, ReferrerPolicyDefault},
      {"http://example.test/cat.css", "style", "text/sass", "",
       ReferrerPolicyDefault, ResourceLoadPriorityUnresolved,
       WebURLRequest::RequestContextStyle, false, false, ReferrerPolicyDefault},
      {"http://example.test/cat.wav", "media", "audio/wav", "",
       ReferrerPolicyDefault, ResourceLoadPriorityLow,
       WebURLRequest::RequestContextVideo, true, true, ReferrerPolicyDefault},
      {"http://example.test/cat.wav", "media", "audio/mp57", "",
       ReferrerPolicyDefault, ResourceLoadPriorityUnresolved,
       WebURLRequest::RequestContextVideo, false, false, ReferrerPolicyDefault},
      {"http://example.test/cat.webm", "media", "video/webm", "",
       ReferrerPolicyDefault, ResourceLoadPriorityLow,
       WebURLRequest::RequestContextVideo, true, true, ReferrerPolicyDefault},
      {"http://example.test/cat.mp199", "media", "video/mp199", "",
       ReferrerPolicyDefault, ResourceLoadPriorityUnresolved,
       WebURLRequest::RequestContextVideo, false, false, ReferrerPolicyDefault},
      {"http://example.test/cat.vtt", "track", "text/vtt", "",
       ReferrerPolicyDefault, ResourceLoadPriorityLow,
       WebURLRequest::RequestContextTrack, true, true, ReferrerPolicyDefault},
      {"http://example.test/cat.vtt", "track", "text/subtitlething", "",
       ReferrerPolicyDefault, ResourceLoadPriorityUnresolved,
       WebURLRequest::RequestContextTrack, false, false, ReferrerPolicyDefault},
      {"http://example.test/cat.woff", "font", "font/woff2", "",
       ReferrerPolicyDefault, ResourceLoadPriorityVeryHigh,
       WebURLRequest::RequestContextFont, true, true, ReferrerPolicyDefault},
      {"http://example.test/cat.woff", "font", "font/woff84", "",
       ReferrerPolicyDefault, ResourceLoadPriorityUnresolved,
       WebURLRequest::RequestContextFont, false, false, ReferrerPolicyDefault},
      {"http://example.test/cat.empty", "", "foo/bar", "",
       ReferrerPolicyDefault, ResourceLoadPriorityHigh,
       WebURLRequest::RequestContextSubresource, true, true,
       ReferrerPolicyDefault},
      {"http://example.test/cat.blob", "blabla", "foo/bar", "",
       ReferrerPolicyDefault, ResourceLoadPriorityLow,
       WebURLRequest::RequestContextSubresource, false, false,
       ReferrerPolicyDefault},
      // Media tests
      {"http://example.test/cat.gif", "image", "image/gif",
       "(max-width: 600px)", ReferrerPolicyDefault, ResourceLoadPriorityLow,
       WebURLRequest::RequestContextImage, true, true, ReferrerPolicyDefault},
      {"http://example.test/cat.gif", "image", "image/gif",
       "(max-width: 400px)", ReferrerPolicyDefault,
       ResourceLoadPriorityUnresolved, WebURLRequest::RequestContextImage, true,
       false, ReferrerPolicyDefault},
      {"http://example.test/cat.gif", "image", "image/gif",
       "(max-width: 600px)", ReferrerPolicyDefault, ResourceLoadPriorityLow,
       WebURLRequest::RequestContextImage, false, false, ReferrerPolicyDefault},
      // Referrer Policy
      {"http://example.test/cat.gif", "image", "image/gif", "",
       ReferrerPolicyOrigin, ResourceLoadPriorityLow,
       WebURLRequest::RequestContextImage, true, true, ReferrerPolicyOrigin},
      {"http://example.test/cat.gif", "image", "image/gif", "",
       ReferrerPolicyOriginWhenCrossOrigin, ResourceLoadPriorityLow,
       WebURLRequest::RequestContextImage, true, true,
       ReferrerPolicyOriginWhenCrossOrigin},
      {"http://example.test/cat.gif", "image", "image/gif", "",
       ReferrerPolicyNever, ResourceLoadPriorityLow,
       WebURLRequest::RequestContextImage, true, true, ReferrerPolicyNever},
  };

  // Test the cases with a single header
  for (const auto& testCase : cases) {
    std::unique_ptr<DummyPageHolder> dummyPageHolder =
        DummyPageHolder::create(IntSize(500, 500));
    dummyPageHolder->frame().settings()->setScriptEnabled(true);
    Persistent<MockLinkLoaderClient> loaderClient =
        MockLinkLoaderClient::create(testCase.linkLoaderShouldLoadValue);
    LinkLoader* loader = LinkLoader::create(loaderClient.get());
    KURL hrefURL = KURL(KURL(), testCase.href);
    URLTestHelpers::registerMockedErrorURLLoad(hrefURL);
    loader->loadLink(LinkRelAttribute("preload"), CrossOriginAttributeNotSet,
                     testCase.type, testCase.as, testCase.media,
                     testCase.referrerPolicy, hrefURL,
                     dummyPageHolder->document(), NetworkHintsMock());
    ASSERT_TRUE(dummyPageHolder->document().fetcher());
    HeapListHashSet<Member<Resource>>* preloads =
        dummyPageHolder->document().fetcher()->preloads();
    if (testCase.expectingLoad) {
      if (!preloads) {
        fprintf(stderr, "Unexpected result %s %s %s\n", testCase.href,
                testCase.as, testCase.type);
      }
      EXPECT_TRUE(preloads);
    } else {
      EXPECT_FALSE(preloads);
    }
    if (preloads) {
      if (testCase.priority == ResourceLoadPriorityUnresolved) {
        EXPECT_EQ(0u, preloads->size());
      } else {
        EXPECT_EQ(1u, preloads->size());
        if (preloads->size() > 0) {
          Resource* resource = preloads->begin().get()->get();
          EXPECT_EQ(testCase.priority, resource->resourceRequest().priority());
          EXPECT_EQ(testCase.context,
                    resource->resourceRequest().requestContext());
          if (testCase.expectedReferrerPolicy != ReferrerPolicyDefault) {
            EXPECT_EQ(testCase.expectedReferrerPolicy,
                      resource->resourceRequest().getReferrerPolicy());
          }
        }
      }
      dummyPageHolder->document().fetcher()->clearPreloads();
    }
    memoryCache()->evictResources();
    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
  }
}

TEST(LinkLoaderTest, Prefetch) {
  struct TestCase {
    const char* href;
    // TODO(yoav): Add support for type and media crbug.com/662687
    const char* type;
    const char* media;
    const ReferrerPolicy referrerPolicy;
    const bool linkLoaderShouldLoadValue;
    const bool expectingLoad;
    const ReferrerPolicy expectedReferrerPolicy;
  } cases[] = {
      // Referrer Policy
      {"http://example.test/cat.jpg", "image/jpg", "", ReferrerPolicyOrigin,
       true, true, ReferrerPolicyOrigin},
      {"http://example.test/cat.jpg", "image/jpg", "",
       ReferrerPolicyOriginWhenCrossOrigin, true, true,
       ReferrerPolicyOriginWhenCrossOrigin},
      {"http://example.test/cat.jpg", "image/jpg", "", ReferrerPolicyNever,
       true, true, ReferrerPolicyNever},
  };

  // Test the cases with a single header
  for (const auto& testCase : cases) {
    std::unique_ptr<DummyPageHolder> dummyPageHolder =
        DummyPageHolder::create(IntSize(500, 500));
    dummyPageHolder->frame().settings()->setScriptEnabled(true);
    Persistent<MockLinkLoaderClient> loaderClient =
        MockLinkLoaderClient::create(testCase.linkLoaderShouldLoadValue);
    LinkLoader* loader = LinkLoader::create(loaderClient.get());
    KURL hrefURL = KURL(KURL(), testCase.href);
    URLTestHelpers::registerMockedErrorURLLoad(hrefURL);
    loader->loadLink(LinkRelAttribute("prefetch"), CrossOriginAttributeNotSet,
                     testCase.type, "", testCase.media, testCase.referrerPolicy,
                     hrefURL, dummyPageHolder->document(), NetworkHintsMock());
    ASSERT_TRUE(dummyPageHolder->document().fetcher());
    Resource* resource = loader->resource();
    if (testCase.expectingLoad) {
      EXPECT_TRUE(resource);
    } else {
      EXPECT_FALSE(resource);
    }
    if (resource) {
      if (testCase.expectedReferrerPolicy != ReferrerPolicyDefault) {
        EXPECT_EQ(testCase.expectedReferrerPolicy,
                  resource->resourceRequest().getReferrerPolicy());
      }
    }
    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
  }
}

TEST(LinkLoaderTest, DNSPrefetch) {
  struct {
    const char* href;
    const bool shouldLoad;
  } cases[] = {
      {"http://example.com/", true},
      {"https://example.com/", true},
      {"//example.com/", true},
      {"//example.com/", false},
  };

  // Test the cases with a single header
  for (const auto& testCase : cases) {
    std::unique_ptr<DummyPageHolder> dummyPageHolder =
        DummyPageHolder::create(IntSize(500, 500));
    dummyPageHolder->document().settings()->setDNSPrefetchingEnabled(true);
    Persistent<MockLinkLoaderClient> loaderClient =
        MockLinkLoaderClient::create(testCase.shouldLoad);
    LinkLoader* loader = LinkLoader::create(loaderClient.get());
    KURL hrefURL =
        KURL(KURL(ParsedURLStringTag(), String("http://example.com")),
             testCase.href);
    NetworkHintsMock networkHints;
    loader->loadLink(LinkRelAttribute("dns-prefetch"),
                     CrossOriginAttributeNotSet, String(), String(), String(),
                     ReferrerPolicyDefault, hrefURL,
                     dummyPageHolder->document(), networkHints);
    EXPECT_FALSE(networkHints.didPreconnect());
    EXPECT_EQ(testCase.shouldLoad, networkHints.didDnsPrefetch());
  }
}

TEST(LinkLoaderTest, Preconnect) {
  struct {
    const char* href;
    CrossOriginAttributeValue crossOrigin;
    const bool shouldLoad;
    const bool isHTTPS;
    const bool isCrossOrigin;
  } cases[] = {
      {"http://example.com/", CrossOriginAttributeNotSet, true, false, false},
      {"https://example.com/", CrossOriginAttributeNotSet, true, true, false},
      {"http://example.com/", CrossOriginAttributeAnonymous, true, false, true},
      {"//example.com/", CrossOriginAttributeNotSet, true, false, false},
      {"http://example.com/", CrossOriginAttributeNotSet, false, false, false},
  };

  // Test the cases with a single header
  for (const auto& testCase : cases) {
    std::unique_ptr<DummyPageHolder> dummyPageHolder =
        DummyPageHolder::create(IntSize(500, 500));
    Persistent<MockLinkLoaderClient> loaderClient =
        MockLinkLoaderClient::create(testCase.shouldLoad);
    LinkLoader* loader = LinkLoader::create(loaderClient.get());
    KURL hrefURL =
        KURL(KURL(ParsedURLStringTag(), String("http://example.com")),
             testCase.href);
    NetworkHintsMock networkHints;
    loader->loadLink(LinkRelAttribute("preconnect"), testCase.crossOrigin,
                     String(), String(), String(), ReferrerPolicyDefault,
                     hrefURL, dummyPageHolder->document(), networkHints);
    EXPECT_EQ(testCase.shouldLoad, networkHints.didPreconnect());
    EXPECT_EQ(testCase.isHTTPS, networkHints.isHTTPS());
    EXPECT_EQ(testCase.isCrossOrigin, networkHints.isCrossOrigin());
  }
}

}  // namespace blink
