// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLPreloadScanner.h"

#include "core/MediaTypeNames.h"
#include "core/css/MediaValuesCached.h"
#include "core/fetch/ClientHintsPreferences.h"
#include "core/frame/Settings.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/parser/HTMLParserOptions.h"
#include "core/html/parser/HTMLResourcePreloader.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

struct TestCase {
    const char* baseURL;
    const char* inputHTML;
    const char* preloadedURL; // Or nullptr if no preload is expected.
    const char* outputBaseURL;
    Resource::Type type;
    int resourceWidth;
    ClientHintsPreferences preferences;
};

struct PreconnectTestCase {
    const char* baseURL;
    const char* inputHTML;
    const char* preconnectedHost;
    CrossOriginAttributeValue crossOrigin;
};

struct ReferrerPolicyTestCase {
    const char* baseURL;
    const char* inputHTML;
    const char* preloadedURL; // Or nullptr if no preload is expected.
    const char* outputBaseURL;
    Resource::Type type;
    int resourceWidth;
    ReferrerPolicy referrerPolicy;
};

struct NonceTestCase {
    const char* baseURL;
    const char* inputHTML;
    const char* nonce;
};

class MockHTMLResourcePreloader : public ResourcePreloader {
public:
    void preloadRequestVerification(Resource::Type type, const char* url, const char* baseURL, int width, const ClientHintsPreferences& preferences)
    {
        if (!url) {
            EXPECT_FALSE(m_preloadRequest);
            return;
        }
        EXPECT_NE(nullptr, m_preloadRequest.get());
        if (m_preloadRequest) {
            EXPECT_FALSE(m_preloadRequest->isPreconnect());
            EXPECT_EQ(type, m_preloadRequest->resourceType());
            EXPECT_STREQ(url, m_preloadRequest->resourceURL().ascii().data());
            EXPECT_STREQ(baseURL, m_preloadRequest->baseURL().getString().ascii().data());
            EXPECT_EQ(width, m_preloadRequest->resourceWidth());
            EXPECT_EQ(preferences.shouldSendDPR(), m_preloadRequest->preferences().shouldSendDPR());
            EXPECT_EQ(preferences.shouldSendResourceWidth(), m_preloadRequest->preferences().shouldSendResourceWidth());
            EXPECT_EQ(preferences.shouldSendViewportWidth(), m_preloadRequest->preferences().shouldSendViewportWidth());
        }
    }

    void preloadRequestVerification(Resource::Type type, const char* url, const char* baseURL, int width, ReferrerPolicy referrerPolicy)
    {
        preloadRequestVerification(type, url, baseURL, width, ClientHintsPreferences());
        EXPECT_EQ(referrerPolicy, m_preloadRequest->getReferrerPolicy());
    }

    void preconnectRequestVerification(const String& host, CrossOriginAttributeValue crossOrigin)
    {
        if (!host.isNull()) {
            EXPECT_TRUE(m_preloadRequest->isPreconnect());
            EXPECT_STREQ(m_preloadRequest->resourceURL().ascii().data(), host.ascii().data());
            EXPECT_EQ(m_preloadRequest->crossOrigin(), crossOrigin);
        }
    }

    void nonceRequestVerification(const char* nonce)
    {
        ASSERT_TRUE(m_preloadRequest.get());
        if (strlen(nonce))
            EXPECT_EQ(nonce, m_preloadRequest->nonce());
        else
            EXPECT_TRUE(m_preloadRequest->nonce().isEmpty());
    }

protected:
    void preload(std::unique_ptr<PreloadRequest> preloadRequest, const NetworkHintsInterface&) override
    {
        m_preloadRequest = std::move(preloadRequest);
    }

private:
    std::unique_ptr<PreloadRequest> m_preloadRequest;
};

class HTMLPreloadScannerTest : public testing::Test {
protected:
    enum ViewportState {
        ViewportEnabled,
        ViewportDisabled,
    };

    enum PreloadState {
        PreloadEnabled,
        PreloadDisabled,
    };

    HTMLPreloadScannerTest()
        : m_dummyPageHolder(DummyPageHolder::create())
    {
    }

    MediaValuesCached::MediaValuesCachedData createMediaValuesData()
    {
        MediaValuesCached::MediaValuesCachedData data;
        data.viewportWidth = 500;
        data.viewportHeight = 600;
        data.deviceWidth = 700;
        data.deviceHeight = 800;
        data.devicePixelRatio = 2.0;
        data.colorBitsPerComponent = 24;
        data.monochromeBitsPerComponent = 0;
        data.primaryPointerType = PointerTypeFine;
        data.defaultFontSize = 16;
        data.threeDEnabled = true;
        data.mediaType = MediaTypeNames::screen;
        data.strictMode = true;
        data.displayMode = WebDisplayModeBrowser;
        return data;
    }

    void runSetUp(ViewportState viewportState, PreloadState preloadState = PreloadEnabled, ReferrerPolicy documentReferrerPolicy = ReferrerPolicyDefault)
    {
        HTMLParserOptions options(&m_dummyPageHolder->document());
        KURL documentURL(ParsedURLString, "http://whatever.test/");
        m_dummyPageHolder->document().settings()->setViewportEnabled(viewportState == ViewportEnabled);
        m_dummyPageHolder->document().settings()->setViewportMetaEnabled(viewportState == ViewportEnabled);
        m_dummyPageHolder->document().settings()->setDoHtmlPreloadScanning(preloadState == PreloadEnabled);
        m_dummyPageHolder->document().setReferrerPolicy(documentReferrerPolicy);
        m_scanner = HTMLPreloadScanner::create(options, documentURL, CachedDocumentParameters::create(&m_dummyPageHolder->document()), createMediaValuesData());
    }

    void SetUp() override
    {
        runSetUp(ViewportEnabled);
    }

    void test(TestCase testCase)
    {
        MockHTMLResourcePreloader preloader;
        KURL baseURL(ParsedURLString, testCase.baseURL);
        m_scanner->appendToEnd(String(testCase.inputHTML));
        m_scanner->scanAndPreload(&preloader, baseURL, nullptr);

        preloader.preloadRequestVerification(testCase.type, testCase.preloadedURL, testCase.outputBaseURL, testCase.resourceWidth, testCase.preferences);
    }

    void test(PreconnectTestCase testCase)
    {
        MockHTMLResourcePreloader preloader;
        KURL baseURL(ParsedURLString, testCase.baseURL);
        m_scanner->appendToEnd(String(testCase.inputHTML));
        m_scanner->scanAndPreload(&preloader, baseURL, nullptr);
        preloader.preconnectRequestVerification(testCase.preconnectedHost, testCase.crossOrigin);
    }

    void test(ReferrerPolicyTestCase testCase)
    {
        MockHTMLResourcePreloader preloader;
        KURL baseURL(ParsedURLString, testCase.baseURL);
        m_scanner->appendToEnd(String(testCase.inputHTML));
        m_scanner->scanAndPreload(&preloader, baseURL, nullptr);

        preloader.preloadRequestVerification(testCase.type, testCase.preloadedURL, testCase.outputBaseURL, testCase.resourceWidth, testCase.referrerPolicy);
    }

    void test(NonceTestCase testCase)
    {
        MockHTMLResourcePreloader preloader;
        KURL baseURL(ParsedURLString, testCase.baseURL);
        m_scanner->appendToEnd(String(testCase.inputHTML));
        m_scanner->scanAndPreload(&preloader, baseURL, nullptr);

        preloader.nonceRequestVerification(testCase.nonce);
    }

private:
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
    std::unique_ptr<HTMLPreloadScanner> m_scanner;
};

TEST_F(HTMLPreloadScannerTest, testImages)
{
    TestCase testCases[] = {
        {"http://example.test", "<img src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w'>", "bla2.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w'>", "bla3.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' src='bla.gif'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' src='bla.gif' sizes='50vw'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' src='bla.gif'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w'>", "bla4.gif", "http://example.test/", Resource::Image, 0},
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testImagesWithViewport)
{
    TestCase testCases[] = {
        {"http://example.test", "<meta name=viewport content='width=160'><img srcset='bla.gif 320w, blabla.gif 640w'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' sizes='50vw'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' src='bla.gif'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' src='bla.gif' sizes='50vw'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' src='bla.gif'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testImagesWithViewportDeviceWidth)
{
    TestCase testCases[] = {
        {"http://example.test", "<meta name=viewport content='width=device-width'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 350},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif", "http://example.test/", Resource::Image, 350},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif", "http://example.test/", Resource::Image, 350},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w'>", "bla2.gif", "http://example.test/", Resource::Image, 350},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w'>", "bla3.gif", "http://example.test/", Resource::Image, 350},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w'>", "bla4.gif", "http://example.test/", Resource::Image, 350},
        {"http://example.test", "<img src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' sizes='50vw'>", "bla4.gif", "http://example.test/", Resource::Image, 350},
        {"http://example.test", "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w'>", "bla4.gif", "http://example.test/", Resource::Image, 350},
        {"http://example.test", "<img sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' src='bla.gif'>", "bla4.gif", "http://example.test/", Resource::Image, 350},
        {"http://example.test", "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' src='bla.gif' sizes='50vw'>", "bla4.gif", "http://example.test/", Resource::Image, 350},
        {"http://example.test", "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' src='bla.gif'>", "bla4.gif", "http://example.test/", Resource::Image, 350},
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testImagesWithViewportDisabled)
{
    runSetUp(ViewportDisabled);
    TestCase testCases[] = {
        {"http://example.test", "<meta name=viewport content='width=160'><img src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w'>", "bla2.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w'>", "bla3.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' src='bla.gif'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' src='bla.gif' sizes='50vw'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' src='bla.gif'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testViewportNoContent)
{
    TestCase testCases[] = {
        {"http://example.test", "<meta name=viewport><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<meta name=viewport content=sdkbsdkjnejjha><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testMetaAcceptCH)
{
    ClientHintsPreferences dpr;
    ClientHintsPreferences resourceWidth;
    ClientHintsPreferences all;
    ClientHintsPreferences viewportWidth;
    dpr.setShouldSendDPR(true);
    all.setShouldSendDPR(true);
    resourceWidth.setShouldSendResourceWidth(true);
    all.setShouldSendResourceWidth(true);
    viewportWidth.setShouldSendViewportWidth(true);
    all.setShouldSendViewportWidth(true);
    TestCase testCases[] = {
        {"http://example.test", "<meta http-equiv='accept-ch' content='bla'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<meta http-equiv='accept-ch' content='dprw'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<meta http-equiv='accept-ch'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<meta http-equiv='accept-ch' content='dpr \t'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0, dpr},
        {"http://example.test", "<meta http-equiv='accept-ch' content='bla,dpr \t'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0, dpr},
        {"http://example.test", "<meta http-equiv='accept-ch' content='  width  '><img sizes='100vw' srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 500, resourceWidth},
        {"http://example.test", "<meta http-equiv='accept-ch' content='  width  , wutever'><img sizes='300px' srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 300, resourceWidth},
        {"http://example.test", "<meta http-equiv='accept-ch' content='  viewport-width  '><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0, viewportWidth},
        {"http://example.test", "<meta http-equiv='accept-ch' content='  viewport-width  , wutever'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0, viewportWidth},
        {"http://example.test", "<meta http-equiv='accept-ch' content='  viewport-width  ,width, wutever, dpr \t'><img sizes='90vw' srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 450, all},
    };

    for (const auto& testCase : testCases) {
        runSetUp(ViewportDisabled);
        test(testCase);
    }
}

TEST_F(HTMLPreloadScannerTest, testPreconnect)
{
    PreconnectTestCase testCases[] = {
        {"http://example.test", "<link rel=preconnect href=http://example2.test>", "http://example2.test", CrossOriginAttributeNotSet},
        {"http://example.test", "<link rel=preconnect href=http://example2.test crossorigin=anonymous>", "http://example2.test", CrossOriginAttributeAnonymous},
        {"http://example.test", "<link rel=preconnect href=http://example2.test crossorigin='use-credentials'>", "http://example2.test", CrossOriginAttributeUseCredentials},
        {"http://example.test", "<link rel=preconnected href=http://example2.test crossorigin='use-credentials'>", nullptr, CrossOriginAttributeNotSet},
        {"http://example.test", "<link rel=preconnect href=ws://example2.test crossorigin='use-credentials'>", nullptr, CrossOriginAttributeNotSet},
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testDisables)
{
    runSetUp(ViewportEnabled, PreloadDisabled);

    TestCase testCases[] = {
        {"http://example.test", "<img src='bla.gif'>"},
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testPicture)
{
    TestCase testCases[] = {
        {"http://example.test", "<picture><source srcset='srcset_bla.gif'><img src='bla.gif'></picture>", "srcset_bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<picture><source sizes='50vw' srcset='srcset_bla.gif'><img src='bla.gif'></picture>", "srcset_bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<picture><source sizes='50vw' srcset='srcset_bla.gif'><img sizes='50vw' src='bla.gif'></picture>", "srcset_bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<picture><source srcset='srcset_bla.gif' sizes='50vw'><img sizes='50vw' src='bla.gif'></picture>", "srcset_bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<picture><source srcset='srcset_bla.gif'><img sizes='50vw' src='bla.gif'></picture>", "srcset_bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<picture><source media='(max-width: 900px)' srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif 500w'></picture>", "srcset_bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<picture><source media='(max-width: 400px)' srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif 500w'></picture>", "bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<picture><source type='image/webp' srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif 500w'></picture>", "srcset_bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<picture><source type='image/jp2' srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif 500w'></picture>", "bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<picture><source media='(max-width: 900px)' type='image/jp2' srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif 500w'></picture>", "bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<picture><source type='image/webp' media='(max-width: 400px)' srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif 500w'></picture>", "bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<picture><source type='image/jp2' media='(max-width: 900px)' srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif 500w'></picture>", "bla.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<picture><source media='(max-width: 400px)' type='image/webp' srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif 500w'></picture>", "bla.gif", "http://example.test/", Resource::Image, 250},
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testReferrerPolicy)
{
    ReferrerPolicyTestCase testCases[] = {
        { "http://example.test", "<img src='bla.gif'/>", "bla.gif", "http://example.test/", Resource::Image, 0, ReferrerPolicyDefault },
        { "http://example.test", "<img referrerpolicy='origin' src='bla.gif'/>", "bla.gif", "http://example.test/", Resource::Image, 0, ReferrerPolicyOrigin },
        { "http://example.test", "<meta name='referrer' content='not-a-valid-policy'><img src='bla.gif'/>", "bla.gif", "http://example.test/", Resource::Image, 0, ReferrerPolicyDefault },
        { "http://example.test", "<img referrerpolicy='origin' referrerpolicy='origin-when-crossorigin' src='bla.gif'/>", "bla.gif", "http://example.test/", Resource::Image, 0, ReferrerPolicyOrigin },
        { "http://example.test", "<img referrerpolicy='not-a-valid-policy' src='bla.gif'/>", "bla.gif", "http://example.test/", Resource::Image, 0, ReferrerPolicyDefault },
        { "http://example.test", "<meta name='referrer' content='no-referrer'><img referrerpolicy='origin' src='bla.gif'/>", "bla.gif", "http://example.test/", Resource::Image, 0, ReferrerPolicyOrigin },
        // The scanner's state is not reset between test cases, so all subsequent test cases have a document referrer policy of no-referrer.
        { "http://example.test", "<img referrerpolicy='not-a-valid-policy' src='bla.gif'/>", "bla.gif", "http://example.test/", Resource::Image, 0, ReferrerPolicyNever },
        { "http://example.test", "<img src='bla.gif'/>", "bla.gif", "http://example.test/", Resource::Image, 0, ReferrerPolicyNever }
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testNonce)
{
    NonceTestCase testCases[] = {
        { "http://example.test", "<script src='/script'></script>", "" },
        { "http://example.test", "<script src='/script' nonce=''></script>", "" },
        { "http://example.test", "<script src='/script' nonce='abc'></script>", "abc" },
        { "http://example.test", "<link rel='import' href='/import'>", "" },
        { "http://example.test", "<link rel='import' href='/import' nonce=''>", "" },
        { "http://example.test", "<link rel='import' href='/import' nonce='abc'>", "abc" },
        { "http://example.test", "<link rel='stylesheet' href='/style'>", "" },
        { "http://example.test", "<link rel='stylesheet' href='/style' nonce=''>", "" },
        { "http://example.test", "<link rel='stylesheet' href='/style' nonce='abc'>", "abc" },

        // <img> doesn't support nonces:
        { "http://example.test", "<img src='/image'>", "" },
        { "http://example.test", "<img src='/image' nonce=''>", "" },
        { "http://example.test", "<img src='/image' nonce='abc'>", "" },
    };

    for (const auto& testCase : testCases) {
        SCOPED_TRACE(testCase.inputHTML);
        test(testCase);
    }
}

// Tests that a document-level referrer policy (e.g. one set by HTTP
// header) is applied for preload requests.
TEST_F(HTMLPreloadScannerTest, testReferrerPolicyOnDocument)
{
    runSetUp(ViewportEnabled, PreloadEnabled, ReferrerPolicyOrigin);
    ReferrerPolicyTestCase testCases[] = {
        { "http://example.test", "<img src='blah.gif'/>", "blah.gif", "http://example.test/", Resource::Image, 0, ReferrerPolicyOrigin },
        { "http://example.test", "<style>@import url('blah.css');</style>", "blah.css", "http://example.test/", Resource::CSSStyleSheet, 0, ReferrerPolicyOrigin },
        // Tests that a meta-delivered referrer policy with an
        // unrecognized policy value does not override the document's
        // referrer policy.
        { "http://example.test", "<meta name='referrer' content='not-a-valid-policy'><img src='bla.gif'/>", "bla.gif", "http://example.test/", Resource::Image, 0, ReferrerPolicyOrigin },
        // Tests that a meta-delivered referrer policy with a
        // valid policy value does override the document's
        // referrer policy.
        { "http://example.test", "<meta name='referrer' content='unsafe-url'><img src='bla.gif'/>", "bla.gif", "http://example.test/", Resource::Image, 0, ReferrerPolicyAlways },
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testLinkRelPreload)
{
    TestCase testCases[] = {
        {"http://example.test", "<link rel=preload href=bla>", "bla", "http://example.test/", Resource::LinkPreload, 0},
        {"http://example.test", "<link rel=preload href=bla as=script>", "bla", "http://example.test/", Resource::Script, 0},
        {"http://example.test", "<link rel=preload href=bla as=style>", "bla", "http://example.test/", Resource::CSSStyleSheet, 0},
        {"http://example.test", "<link rel=preload href=bla as=image>", "bla", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<link rel=preload href=bla as=font>", "bla", "http://example.test/", Resource::Font, 0},
        {"http://example.test", "<link rel=preload href=bla as=media>", "bla", "http://example.test/", Resource::Media, 0},
        {"http://example.test", "<link rel=preload href=bla as=track>", "bla", "http://example.test/", Resource::TextTrack, 0},
        {"http://example.test", "<link rel=preload href=bla as=image media=\"(max-width: 800px)\">", "bla", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<link rel=preload href=bla as=image media=\"(max-width: 400px)\">", nullptr, "http://example.test/", Resource::Image, 0},
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

// The preload scanner should follow the same policy that the ScriptLoader does
// with regard to the type and language attribute.
TEST_F(HTMLPreloadScannerTest, testScriptTypeAndLanguage)
{
    TestCase testCases[] = {
        // Allow empty src and language attributes.
        {"http://example.test", "<script src='test.js'></script>", "test.js", "http://example.test/", Resource::Script, 0},
        {"http://example.test", "<script type='' language='' src='test.js'></script>", "test.js", "http://example.test/", Resource::Script, 0},
        // Allow standard language and type attributes.
        {"http://example.test", "<script type='text/javascript' src='test.js'></script>", "test.js", "http://example.test/", Resource::Script, 0},
        {"http://example.test", "<script type='text/javascript' language='javascript' src='test.js'></script>", "test.js", "http://example.test/", Resource::Script, 0},
        // Allow legacy languages in the "language" attribute with an empty
        // type.
        {"http://example.test", "<script language='javascript1.1' src='test.js'></script>", "test.js", "http://example.test/", Resource::Script, 0},
        // Allow legacy languages in the "type" attribute.
        {"http://example.test", "<script type='javascript' src='test.js'></script>", "test.js", "http://example.test/", Resource::Script, 0},
        {"http://example.test", "<script type='javascript1.7' src='test.js'></script>", "test.js", "http://example.test/", Resource::Script, 0},
        // Do not allow invalid types in the "type" attribute.
        {"http://example.test", "<script type='invalid' src='test.js'></script>", nullptr, "http://example.test/", Resource::Script, 0},
        {"http://example.test", "<script type='asdf' src='test.js'></script>", nullptr, "http://example.test/", Resource::Script, 0},
        // Do not allow invalid languages.
        {"http://example.test", "<script language='french' src='test.js'></script>", nullptr, "http://example.test/", Resource::Script, 0},
        {"http://example.test", "<script language='python' src='test.js'></script>", nullptr, "http://example.test/", Resource::Script, 0},
    };

    for (const auto& testCase : testCases)
        test(testCase);
}

} // namespace blink
