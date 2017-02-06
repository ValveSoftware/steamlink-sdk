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

class MockLinkLoaderClient final : public GarbageCollectedFinalized<MockLinkLoaderClient>, public LinkLoaderClient {
    USING_GARBAGE_COLLECTED_MIXIN(MockLinkLoaderClient);
public:
    static MockLinkLoaderClient* create(bool shouldLoad)
    {
        return new MockLinkLoaderClient(shouldLoad);
    }

    DEFINE_INLINE_VIRTUAL_TRACE() { LinkLoaderClient::trace(visitor); }

    bool shouldLoadLink() override
    {
        return m_shouldLoad;
    }

    void linkLoaded() override {}
    void linkLoadingErrored() override {}
    void didStartLinkPrerender() override {}
    void didStopLinkPrerender() override {}
    void didSendLoadForLinkPrerender() override {}
    void didSendDOMContentLoadedForLinkPrerender() override {}

private:
    explicit MockLinkLoaderClient(bool shouldLoad)
        : m_shouldLoad(shouldLoad)
    {
    }

    bool m_shouldLoad;
};

class NetworkHintsMock : public NetworkHintsInterface {
public:
    NetworkHintsMock()
        : m_didDnsPrefetch(false)
        , m_didPreconnect(false)
    {
    }

    void dnsPrefetchHost(const String& host) const override
    {
        m_didDnsPrefetch = true;
    }

    void preconnectHost(const KURL& host, const CrossOriginAttributeValue crossOrigin) const override
    {
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

TEST(LinkLoaderTest, Preload)
{
    struct TestCase {
        const char* href;
        const char* as;
        const char* type;
        const char* media;
        const ResourceLoadPriority priority;
        const WebURLRequest::RequestContext context;
        const bool linkLoaderShouldLoadValue;
        const bool expectingLoad;
    } cases[] = {
        {"http://example.test/cat.jpg", "image", "", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextImage, true, true},
        {"http://example.test/cat.js", "script", "", "", ResourceLoadPriorityHigh, WebURLRequest::RequestContextScript, true, true},
        {"http://example.test/cat.css", "style", "", "", ResourceLoadPriorityVeryHigh, WebURLRequest::RequestContextStyle, true, true},
        // TODO(yoav): It doesn't seem like the audio context is ever used. That should probably be fixed (or we can consolidate audio and video).
        {"http://example.test/cat.wav", "media", "", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextVideo, true, true},
        {"http://example.test/cat.mp4", "media", "", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextVideo, true, true},
        {"http://example.test/cat.vtt", "track", "", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextTrack, true, true},
        {"http://example.test/cat.woff", "font", "", "", ResourceLoadPriorityVeryHigh, WebURLRequest::RequestContextFont, true, true},
        // TODO(yoav): subresource should be *very* low priority (rather than low).
        {"http://example.test/cat.empty", "", "", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextSubresource, true, true},
        {"http://example.test/cat.blob", "blabla", "", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextSubresource, false, false},
        {"bla://example.test/cat.gif", "image", "", "", ResourceLoadPriorityUnresolved, WebURLRequest::RequestContextImage, false, false},
        // MIME type tests
        {"http://example.test/cat.webp", "image", "image/webp", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextImage, true, true},
        {"http://example.test/cat.svg", "image", "image/svg+xml", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextImage, true, true},
        {"http://example.test/cat.jxr", "image", "image/jxr", "", ResourceLoadPriorityUnresolved, WebURLRequest::RequestContextImage, false, false},
        {"http://example.test/cat.js", "script", "text/javascript", "", ResourceLoadPriorityHigh, WebURLRequest::RequestContextScript, true, true},
        {"http://example.test/cat.js", "script", "text/coffeescript", "", ResourceLoadPriorityUnresolved, WebURLRequest::RequestContextScript, false, false},
        {"http://example.test/cat.css", "style", "text/css", "", ResourceLoadPriorityVeryHigh, WebURLRequest::RequestContextStyle, true, true},
        {"http://example.test/cat.css", "style", "text/sass", "", ResourceLoadPriorityUnresolved, WebURLRequest::RequestContextStyle, false, false},
        {"http://example.test/cat.wav", "media", "audio/wav", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextVideo, true, true},
        {"http://example.test/cat.wav", "media", "audio/mp57", "", ResourceLoadPriorityUnresolved, WebURLRequest::RequestContextVideo, false, false},
        {"http://example.test/cat.webm", "media", "video/webm", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextVideo, true, true},
        {"http://example.test/cat.mp199", "media", "video/mp199", "", ResourceLoadPriorityUnresolved, WebURLRequest::RequestContextVideo, false, false},
        {"http://example.test/cat.vtt", "track", "text/vtt", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextTrack, true, true},
        {"http://example.test/cat.vtt", "track", "text/subtitlething", "", ResourceLoadPriorityUnresolved, WebURLRequest::RequestContextTrack, false, false},
        {"http://example.test/cat.woff", "font", "font/woff2", "", ResourceLoadPriorityVeryHigh, WebURLRequest::RequestContextFont, true, true},
        {"http://example.test/cat.woff", "font", "font/woff84", "", ResourceLoadPriorityUnresolved, WebURLRequest::RequestContextFont, false, false},
        {"http://example.test/cat.empty", "", "foo/bar", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextSubresource, true, true},
        {"http://example.test/cat.blob", "blabla", "foo/bar", "", ResourceLoadPriorityLow, WebURLRequest::RequestContextSubresource, false, false},
        // Media tests
        {"http://example.test/cat.gif", "image", "image/gif", "(max-width: 600px)", ResourceLoadPriorityLow, WebURLRequest::RequestContextImage, true, true},
        {"http://example.test/cat.gif", "image", "image/gif", "(max-width: 400px)", ResourceLoadPriorityUnresolved, WebURLRequest::RequestContextImage, true, false},
        {"http://example.test/cat.gif", "image", "image/gif", "(max-width: 600px)", ResourceLoadPriorityLow, WebURLRequest::RequestContextImage, false, false},
    };

    // Test the cases with a single header
    for (const auto& testCase : cases) {
        std::unique_ptr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
        dummyPageHolder->frame().settings()->setScriptEnabled(true);
        Persistent<MockLinkLoaderClient> loaderClient = MockLinkLoaderClient::create(testCase.linkLoaderShouldLoadValue);
        LinkLoader* loader = LinkLoader::create(loaderClient.get());
        KURL hrefURL = KURL(KURL(), testCase.href);
        URLTestHelpers::registerMockedErrorURLLoad(hrefURL);
        loader->loadLink(LinkRelAttribute("preload"),
            CrossOriginAttributeNotSet,
            testCase.type,
            testCase.as,
            testCase.media,
            hrefURL,
            dummyPageHolder->document(),
            NetworkHintsMock());
        ASSERT(dummyPageHolder->document().fetcher());
        HeapListHashSet<Member<Resource>>* preloads = dummyPageHolder->document().fetcher()->preloads();
        if (testCase.expectingLoad) {
            if (!preloads)
                fprintf(stderr, "Unexpected result %s %s %s\n", testCase.href, testCase.as, testCase.type);
            ASSERT_NE(nullptr, preloads);
        } else {
            ASSERT_EQ(nullptr, preloads);
        }
        if (preloads) {
            if (testCase.priority == ResourceLoadPriorityUnresolved) {
                ASSERT_EQ((unsigned)0, preloads->size());
            } else {
                ASSERT_EQ((unsigned)1, preloads->size());
                if (preloads->size() > 0) {
                    Resource* resource = preloads->begin().get()->get();
                    ASSERT_EQ(testCase.priority, resource->resourceRequest().priority());
                    ASSERT_EQ(testCase.context, resource->resourceRequest().requestContext());
                }
            }
            dummyPageHolder->document().fetcher()->clearPreloads();
        }
        memoryCache()->evictResources();
        Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
    }
}

TEST(LinkLoaderTest, DNSPrefetch)
{
    struct {
        const char* href;
        const bool shouldLoad;
    } cases[] = {
        {"http://example.com/", true},
        {"https://example.com/", true},
        {"//example.com/", true},
    };

    // TODO(yoav): Test (and fix) shouldLoad = false

    // Test the cases with a single header
    for (const auto& testCase : cases) {
        std::unique_ptr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
        dummyPageHolder->document().settings()->setDNSPrefetchingEnabled(true);
        Persistent<MockLinkLoaderClient> loaderClient = MockLinkLoaderClient::create(testCase.shouldLoad);
        LinkLoader* loader = LinkLoader::create(loaderClient.get());
        KURL hrefURL = KURL(KURL(ParsedURLStringTag(), String("http://example.com")), testCase.href);
        NetworkHintsMock networkHints;
        loader->loadLink(LinkRelAttribute("dns-prefetch"),
            CrossOriginAttributeNotSet,
            String(),
            String(),
            String(),
            hrefURL,
            dummyPageHolder->document(),
            networkHints);
        ASSERT_FALSE(networkHints.didPreconnect());
        ASSERT_EQ(testCase.shouldLoad, networkHints.didDnsPrefetch());
    }
}

TEST(LinkLoaderTest, Preconnect)
{
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
    };

    // Test the cases with a single header
    for (const auto& testCase : cases) {
        std::unique_ptr<DummyPageHolder> dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
        Persistent<MockLinkLoaderClient> loaderClient = MockLinkLoaderClient::create(testCase.shouldLoad);
        LinkLoader* loader = LinkLoader::create(loaderClient.get());
        KURL hrefURL = KURL(KURL(ParsedURLStringTag(), String("http://example.com")), testCase.href);
        NetworkHintsMock networkHints;
        loader->loadLink(LinkRelAttribute("preconnect"),
            testCase.crossOrigin,
            String(),
            String(),
            String(),
            hrefURL,
            dummyPageHolder->document(),
            networkHints);
        ASSERT_EQ(testCase.shouldLoad, networkHints.didPreconnect());
        ASSERT_EQ(testCase.isHTTPS, networkHints.isHTTPS());
        ASSERT_EQ(testCase.isCrossOrigin, networkHints.isCrossOrigin());
    }
}

} // namespace blink
