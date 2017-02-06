// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLResourcePreloader.h"

#include "core/html/parser/PreloadRequest.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

struct PreconnectTestCase {
    const char* baseURL;
    const char* url;
    bool isCORS;
    bool isHTTPS;
};

class PreloaderNetworkHintsMock : public NetworkHintsInterface {
public:
    PreloaderNetworkHintsMock()
        : m_didPreconnect(false)
    {
    }

    void dnsPrefetchHost(const String& host) const { }
    void preconnectHost(const KURL& host, const CrossOriginAttributeValue crossOrigin) const override
    {
        m_didPreconnect = true;
        m_isHTTPS = host.protocolIs("https");
        m_isCrossOrigin = (crossOrigin == CrossOriginAttributeAnonymous);
    }

    bool didPreconnect() { return m_didPreconnect; }
    bool isHTTPS() { return m_isHTTPS; }
    bool isCrossOrigin() { return m_isCrossOrigin; }

private:
    mutable bool m_didPreconnect;
    mutable bool m_isHTTPS;
    mutable bool m_isCrossOrigin;
};

class HTMLResourcePreloaderTest : public testing::Test {
protected:
    HTMLResourcePreloaderTest()
        : m_dummyPageHolder(DummyPageHolder::create())
    {
    }

    void test(PreconnectTestCase testCase)
    {
        // TODO(yoav): Need a mock loader here to verify things are happenning beyond preconnect.
        PreloaderNetworkHintsMock networkHints;
        std::unique_ptr<PreloadRequest> preloadRequest = PreloadRequest::create(String(),
            TextPosition(),
            testCase.url,
            KURL(ParsedURLStringTag(), testCase.baseURL),
            Resource::Image,
            ReferrerPolicy(),
            FetchRequest::ResourceWidth(),
            ClientHintsPreferences(),
            PreloadRequest::RequestTypePreconnect);
        if (testCase.isCORS)
            preloadRequest->setCrossOrigin(CrossOriginAttributeAnonymous);
        HTMLResourcePreloader* preloader = HTMLResourcePreloader::create(m_dummyPageHolder->document());
        preloader->preload(std::move(preloadRequest), networkHints);
        ASSERT_TRUE(networkHints.didPreconnect());
        ASSERT_EQ(testCase.isCORS, networkHints.isCrossOrigin());
        ASSERT_EQ(testCase.isHTTPS, networkHints.isHTTPS());
    }

private:
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

TEST_F(HTMLResourcePreloaderTest, testPreconnect)
{
    PreconnectTestCase testCases[] = {
        { "http://example.test", "http://example.com", false, false },
        { "http://example.test", "http://example.com", true, false },
        { "http://example.test", "https://example.com", true, true },
        { "http://example.test", "https://example.com", false, true },
        { "http://example.test", "//example.com", false, false },
        { "http://example.test", "//example.com", true, false },
        { "https://example.test", "//example.com", false, true },
        { "https://example.test", "//example.com", true, true },
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

} // namespace blink
