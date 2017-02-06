// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebEmbeddedWorker.h"

#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "public/web/WebCache.h"
#include "public/web/WebEmbeddedWorkerStartData.h"
#include "public/web/WebSettings.h"
#include "public/web/modules/serviceworker/WebServiceWorkerContextClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {
namespace {

class MockServiceWorkerContextClient
    : public WebServiceWorkerContextClient {
public:
    MockServiceWorkerContextClient()
        : m_hasAssociatedRegistration(true)
    {
    }
    ~MockServiceWorkerContextClient() override { }
    MOCK_METHOD0(workerReadyForInspection, void());
    MOCK_METHOD0(workerContextFailedToStart, void());
    MOCK_METHOD0(workerScriptLoaded, void());
    MOCK_METHOD1(createServiceWorkerNetworkProvider, WebServiceWorkerNetworkProvider*(WebDataSource*));
    MOCK_METHOD0(createServiceWorkerProvider, WebServiceWorkerProvider*());
    bool hasAssociatedRegistration() override
    {
        return m_hasAssociatedRegistration;
    }
    void setHasAssociatedRegistration(bool hasAssociatedRegistration)
    {
        m_hasAssociatedRegistration = hasAssociatedRegistration;
    }
    void getClient(const WebString&, WebServiceWorkerClientCallbacks*) override { NOTREACHED(); }
    void getClients(const WebServiceWorkerClientQueryOptions&, WebServiceWorkerClientsCallbacks*) override { NOTREACHED(); }
    void openWindow(const WebURL&, WebServiceWorkerClientCallbacks*) override { NOTREACHED(); }
    void postMessageToClient(const WebString& uuid, const WebString&, WebMessagePortChannelArray*) override { NOTREACHED(); }
    void postMessageToCrossOriginClient(const WebCrossOriginServiceWorkerClient&, const WebString&, WebMessagePortChannelArray*) override { NOTREACHED(); }
    void skipWaiting(WebServiceWorkerSkipWaitingCallbacks*) override { NOTREACHED(); }
    void claim(WebServiceWorkerClientsClaimCallbacks*) override { NOTREACHED(); }
    void focus(const WebString& uuid, WebServiceWorkerClientCallbacks*) override { NOTREACHED(); }
    void navigate(const WebString& uuid, const WebURL&, WebServiceWorkerClientCallbacks*) override { NOTREACHED(); }
    void registerForeignFetchScopes(const WebVector<WebURL>& subScopes, const WebVector<WebSecurityOrigin>& origins) override { NOTREACHED(); }

private:
    bool m_hasAssociatedRegistration;
};

class WebEmbeddedWorkerImplTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_mockClient = new MockServiceWorkerContextClient();
        m_worker = wrapUnique(WebEmbeddedWorker::create(m_mockClient, nullptr));

        WebURL scriptURL = URLTestHelpers::toKURL("https://www.example.com/sw.js");
        WebURLResponse response;
        response.initialize();
        response.setMIMEType("text/javascript");
        response.setHTTPStatusCode(200);
        Platform::current()->getURLLoaderMockFactory()->registerURL(scriptURL, response, "");

        m_startData.scriptURL = scriptURL;
        m_startData.userAgent = WebString("dummy user agent");
        m_startData.pauseAfterDownloadMode = WebEmbeddedWorkerStartData::DontPauseAfterDownload;
        m_startData.waitForDebuggerMode = WebEmbeddedWorkerStartData::DontWaitForDebugger;
        m_startData.v8CacheOptions = WebSettings::V8CacheOptionsDefault;
    }

    void TearDown() override
    {
        Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
        WebCache::clear();
    }

    WebEmbeddedWorkerStartData m_startData;
    MockServiceWorkerContextClient* m_mockClient;
    std::unique_ptr<WebEmbeddedWorker> m_worker;
};

} // namespace

TEST_F(WebEmbeddedWorkerImplTest, TerminateSoonAfterStart)
{
    EXPECT_CALL(*m_mockClient, workerReadyForInspection()).Times(1);
    m_worker->startWorkerContext(m_startData);
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    EXPECT_CALL(*m_mockClient, workerContextFailedToStart()).Times(1);
    m_worker->terminateWorkerContext();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileWaitingForDebugger)
{
    EXPECT_CALL(*m_mockClient, workerReadyForInspection()).Times(1);
    m_startData.waitForDebuggerMode = WebEmbeddedWorkerStartData::WaitForDebugger;
    m_worker->startWorkerContext(m_startData);
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    EXPECT_CALL(*m_mockClient, workerContextFailedToStart()).Times(1);
    m_worker->terminateWorkerContext();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileLoadingScript)
{
    EXPECT_CALL(*m_mockClient, workerReadyForInspection()).Times(1);
    m_worker->startWorkerContext(m_startData);
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Load the shadow page.
    EXPECT_CALL(*m_mockClient, createServiceWorkerNetworkProvider(::testing::_)).WillOnce(::testing::Return(nullptr));
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Terminate before loading the script.
    EXPECT_CALL(*m_mockClient, workerContextFailedToStart()).Times(1);
    m_worker->terminateWorkerContext();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhilePausedAfterDownload)
{
    EXPECT_CALL(*m_mockClient, workerReadyForInspection())
        .Times(1);
    m_startData.pauseAfterDownloadMode = WebEmbeddedWorkerStartData::PauseAfterDownload;
    m_worker->startWorkerContext(m_startData);
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Load the shadow page.
    EXPECT_CALL(*m_mockClient, createServiceWorkerNetworkProvider(::testing::_))
        .WillOnce(::testing::Return(nullptr));
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Load the script.
    EXPECT_CALL(*m_mockClient, workerScriptLoaded())
        .Times(1);
    EXPECT_CALL(*m_mockClient, createServiceWorkerProvider())
        .Times(0);
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Terminate before resuming after download.
    EXPECT_CALL(*m_mockClient, createServiceWorkerProvider())
        .Times(0);
    EXPECT_CALL(*m_mockClient, workerContextFailedToStart())
        .Times(1);
    m_worker->terminateWorkerContext();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);
}

TEST_F(WebEmbeddedWorkerImplTest, ScriptNotFound)
{
    WebURL scriptURL = URLTestHelpers::toKURL("https://www.example.com/sw-404.js");
    WebURLResponse response;
    response.initialize();
    response.setMIMEType("text/javascript");
    response.setHTTPStatusCode(404);
    WebURLError error;
    error.reason = 1010;
    error.domain = "WebEmbeddedWorkerImplTest";
    Platform::current()->getURLLoaderMockFactory()->registerErrorURL(scriptURL, response, error);
    m_startData.scriptURL = scriptURL;

    EXPECT_CALL(*m_mockClient, workerReadyForInspection())
        .Times(1);
    m_worker->startWorkerContext(m_startData);
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Load the shadow page.
    EXPECT_CALL(*m_mockClient, createServiceWorkerNetworkProvider(::testing::_))
        .WillOnce(::testing::Return(nullptr));
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Load the script.
    EXPECT_CALL(*m_mockClient, workerScriptLoaded())
        .Times(0);
    EXPECT_CALL(*m_mockClient, createServiceWorkerProvider())
        .Times(0);
    EXPECT_CALL(*m_mockClient, workerContextFailedToStart())
        .Times(1);
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);
}

TEST_F(WebEmbeddedWorkerImplTest, NoRegistration)
{
    EXPECT_CALL(*m_mockClient, workerReadyForInspection())
        .Times(1);
    m_startData.pauseAfterDownloadMode = WebEmbeddedWorkerStartData::PauseAfterDownload;
    m_worker->startWorkerContext(m_startData);
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Load the shadow page.
    EXPECT_CALL(*m_mockClient, createServiceWorkerNetworkProvider(::testing::_))
        .WillOnce(::testing::Return(nullptr));
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Load the script.
    m_mockClient->setHasAssociatedRegistration(false);
    EXPECT_CALL(*m_mockClient, workerScriptLoaded())
        .Times(0);
    EXPECT_CALL(*m_mockClient, createServiceWorkerProvider())
        .Times(0);
    EXPECT_CALL(*m_mockClient, workerContextFailedToStart())
        .Times(1);
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);
}

// The running worker is detected as a memory leak. crbug.com/586897
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DontPauseAfterDownload DISABLED_DontPauseAfterDownload
#else
#define MAYBE_DontPauseAfterDownload DontPauseAfterDownload
#endif

TEST_F(WebEmbeddedWorkerImplTest, MAYBE_DontPauseAfterDownload)
{
    EXPECT_CALL(*m_mockClient, workerReadyForInspection())
        .Times(1);
    m_worker->startWorkerContext(m_startData);
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Load the shadow page.
    EXPECT_CALL(*m_mockClient, createServiceWorkerNetworkProvider(::testing::_))
        .WillOnce(::testing::Return(nullptr));
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Load the script.
    EXPECT_CALL(*m_mockClient, workerScriptLoaded())
        .Times(1);
    EXPECT_CALL(*m_mockClient, createServiceWorkerProvider())
        .WillOnce(::testing::Return(nullptr));
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);
}

// The running worker is detected as a memory leak. crbug.com/586897
#if defined(ADDRESS_SANITIZER)
#define MAYBE_PauseAfterDownload DISABLED_PauseAfterDownload
#else
#define MAYBE_PauseAfterDownload PauseAfterDownload
#endif

TEST_F(WebEmbeddedWorkerImplTest, MAYBE_PauseAfterDownload)
{
    EXPECT_CALL(*m_mockClient, workerReadyForInspection())
        .Times(1);
    m_startData.pauseAfterDownloadMode = WebEmbeddedWorkerStartData::PauseAfterDownload;
    m_worker->startWorkerContext(m_startData);
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Load the shadow page.
    EXPECT_CALL(*m_mockClient, createServiceWorkerNetworkProvider(::testing::_))
        .WillOnce(::testing::Return(nullptr));
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Load the script.
    EXPECT_CALL(*m_mockClient, workerScriptLoaded())
        .Times(1);
    EXPECT_CALL(*m_mockClient, createServiceWorkerProvider())
        .Times(0);
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);

    // Resume after download.
    EXPECT_CALL(*m_mockClient, createServiceWorkerProvider())
        .WillOnce(::testing::Return(nullptr));
    m_worker->resumeAfterDownload();
    ::testing::Mock::VerifyAndClearExpectations(m_mockClient);
}

} // namespace blink
