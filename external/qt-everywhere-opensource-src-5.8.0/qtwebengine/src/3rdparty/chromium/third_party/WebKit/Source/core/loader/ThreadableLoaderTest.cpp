// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/ThreadableLoader.h"

#include "core/dom/CrossThreadTask.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/loader/DocumentThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/loader/WorkerThreadableLoader.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThreadTestHelper.h"
#include "platform/WaitableEvent.h"
#include "platform/geometry/IntSize.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoadTiming.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::StrEq;
using ::testing::Truly;
using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;

class MockThreadableLoaderClient : public ThreadableLoaderClient {
public:
    static std::unique_ptr<MockThreadableLoaderClient> create()
    {
        return wrapUnique(new ::testing::StrictMock<MockThreadableLoaderClient>);
    }
    MOCK_METHOD2(didSendData, void(unsigned long long, unsigned long long));
    MOCK_METHOD3(didReceiveResponseMock, void(unsigned long, const ResourceResponse&, WebDataConsumerHandle*));
    void didReceiveResponse(unsigned long identifier, const ResourceResponse& response, std::unique_ptr<WebDataConsumerHandle> handle)
    {
        didReceiveResponseMock(identifier, response, handle.get());
    }
    MOCK_METHOD2(didReceiveData, void(const char*, unsigned));
    MOCK_METHOD2(didReceiveCachedMetadata, void(const char*, int));
    MOCK_METHOD2(didFinishLoading, void(unsigned long, double));
    MOCK_METHOD1(didFail, void(const ResourceError&));
    MOCK_METHOD1(didFailAccessControlCheck, void(const ResourceError&));
    MOCK_METHOD0(didFailRedirectCheck, void());
    MOCK_METHOD1(didReceiveResourceTiming, void(const ResourceTimingInfo&));
    MOCK_METHOD1(didDownloadData, void(int));

protected:
    MockThreadableLoaderClient() = default;
};

bool isCancellation(const ResourceError& error)
{
    return error.isCancellation();
}

bool isNotCancellation(const ResourceError& error)
{
    return !error.isCancellation();
}

KURL successURL() { return KURL(KURL(), "http://example.com/success"); }
KURL errorURL() { return KURL(KURL(), "http://example.com/error"); }
KURL redirectURL() { return KURL(KURL(), "http://example.com/redirect"); }
KURL redirectLoopURL() { return KURL(KURL(), "http://example.com/loop"); }

enum ThreadableLoaderToTest {
    DocumentThreadableLoaderTest,
    WorkerThreadableLoaderTest
};

class ThreadableLoaderTestHelper {
public:
    virtual ~ThreadableLoaderTestHelper() { }

    virtual void createLoader(ThreadableLoaderClient*, CrossOriginRequestPolicy) = 0;
    virtual void startLoader(const ResourceRequest&) = 0;
    virtual void cancelLoader() = 0;
    virtual void clearLoader() = 0;
    virtual Checkpoint& checkpoint() = 0;
    virtual void callCheckpoint(int) = 0;
    virtual void onSetUp() = 0;
    virtual void onServeRequests() = 0;
    virtual void onTearDown() = 0;
};

class DocumentThreadableLoaderTestHelper : public ThreadableLoaderTestHelper {
public:
    DocumentThreadableLoaderTestHelper()
        : m_dummyPageHolder(DummyPageHolder::create(IntSize(1, 1)))
    {
    }

    void createLoader(ThreadableLoaderClient* client, CrossOriginRequestPolicy crossOriginRequestPolicy) override
    {
        ThreadableLoaderOptions options;
        options.crossOriginRequestPolicy = crossOriginRequestPolicy;
        ResourceLoaderOptions resourceLoaderOptions;
        m_loader = DocumentThreadableLoader::create(document(), client, options, resourceLoaderOptions);
    }

    void startLoader(const ResourceRequest& request) override
    {
        m_loader->start(request);
    }

    void cancelLoader() override { m_loader->cancel(); }
    void clearLoader() override { m_loader.reset(); }
    Checkpoint& checkpoint() override { return m_checkpoint; }
    void callCheckpoint(int n) override { m_checkpoint.Call(n); }

    void onSetUp() override
    {
    }

    void onServeRequests() override
    {
    }

    void onTearDown() override
    {
        m_loader.reset();
    }

private:
    Document& document() { return m_dummyPageHolder->document(); }

    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
    Checkpoint m_checkpoint;
    std::unique_ptr<DocumentThreadableLoader> m_loader;
};

class WorkerThreadableLoaderTestHelper : public ThreadableLoaderTestHelper, public WorkerLoaderProxyProvider {
public:
    WorkerThreadableLoaderTestHelper()
        : m_dummyPageHolder(DummyPageHolder::create(IntSize(1, 1)))
    {
    }

    void createLoader(ThreadableLoaderClient* client, CrossOriginRequestPolicy crossOriginRequestPolicy) override
    {
        std::unique_ptr<WaitableEvent> completionEvent = wrapUnique(new WaitableEvent());
        postTaskToWorkerGlobalScope(createCrossThreadTask(
            &WorkerThreadableLoaderTestHelper::workerCreateLoader,
            crossThreadUnretained(this),
            crossThreadUnretained(client),
            crossThreadUnretained(completionEvent.get()),
            crossOriginRequestPolicy));
        completionEvent->wait();
    }

    void startLoader(const ResourceRequest& request) override
    {
        std::unique_ptr<WaitableEvent> completionEvent = wrapUnique(new WaitableEvent());
        postTaskToWorkerGlobalScope(createCrossThreadTask(
            &WorkerThreadableLoaderTestHelper::workerStartLoader,
            crossThreadUnretained(this),
            crossThreadUnretained(completionEvent.get()),
            request));
        completionEvent->wait();
    }

    // Must be called on the worker thread.
    void cancelLoader() override
    {
        ASSERT(m_workerThread);
        ASSERT(m_workerThread->isCurrentThread());
        m_loader->cancel();
    }

    // Must be called on the worker thread.
    void clearLoader() override
    {
        ASSERT(m_workerThread);
        ASSERT(m_workerThread->isCurrentThread());
        m_loader.reset();
    }

    Checkpoint& checkpoint() override
    {
        return m_checkpoint;
    }

    void callCheckpoint(int n) override
    {
        testing::runPendingTasks();

        std::unique_ptr<WaitableEvent> completionEvent = wrapUnique(new WaitableEvent());
        postTaskToWorkerGlobalScope(createCrossThreadTask(
            &WorkerThreadableLoaderTestHelper::workerCallCheckpoint,
            crossThreadUnretained(this),
            crossThreadUnretained(completionEvent.get()),
            n));
        completionEvent->wait();
    }

    void onSetUp() override
    {
        m_mockWorkerReportingProxy = wrapUnique(new MockWorkerReportingProxy());
        m_securityOrigin = document().getSecurityOrigin();
        m_workerThread = wrapUnique(new WorkerThreadForTest(
            this,
            *m_mockWorkerReportingProxy));

        expectWorkerLifetimeReportingCalls();
        m_workerThread->startWithSourceCode(m_securityOrigin.get(), "//fake source code");
        m_workerThread->waitForInit();
    }

    void onServeRequests() override
    {
        testing::runPendingTasks();
    }

    void onTearDown() override
    {
        postTaskToWorkerGlobalScope(createCrossThreadTask(&WorkerThreadableLoaderTestHelper::clearLoader, crossThreadUnretained(this)));
        m_workerThread->terminateAndWait();

        // Needed to clean up the things on the main thread side and
        // avoid Resource leaks.
        testing::runPendingTasks();

        m_workerThread->workerLoaderProxy()->detachProvider(this);
    }

private:
    Document& document() { return m_dummyPageHolder->document(); }

    void expectWorkerLifetimeReportingCalls()
    {
        EXPECT_CALL(*m_mockWorkerReportingProxy, workerGlobalScopeStarted(_)).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, didEvaluateWorkerScript(true)).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, workerThreadTerminated()).Times(1);
        EXPECT_CALL(*m_mockWorkerReportingProxy, willDestroyWorkerGlobalScope()).Times(1);
    }

    void workerCreateLoader(ThreadableLoaderClient* client, WaitableEvent* event, CrossOriginRequestPolicy crossOriginRequestPolicy)
    {
        ASSERT(m_workerThread);
        ASSERT(m_workerThread->isCurrentThread());

        ThreadableLoaderOptions options;
        options.crossOriginRequestPolicy = crossOriginRequestPolicy;
        ResourceLoaderOptions resourceLoaderOptions;

        // Ensure that WorkerThreadableLoader is created.
        // ThreadableLoader::create() determines whether it should create
        // a DocumentThreadableLoader or WorkerThreadableLoader based on
        // isWorkerGlobalScope().
        ASSERT(m_workerThread->workerGlobalScope()->isWorkerGlobalScope());

        m_loader = ThreadableLoader::create(*m_workerThread->workerGlobalScope(), client, options, resourceLoaderOptions);
        ASSERT(m_loader);
        event->signal();
    }

    void workerStartLoader(WaitableEvent* event, std::unique_ptr<CrossThreadResourceRequestData> requestData)
    {
        ASSERT(m_workerThread);
        ASSERT(m_workerThread->isCurrentThread());

        ResourceRequest request(requestData.get());
        m_loader->start(request);
        event->signal();
    }

    void workerCallCheckpoint(WaitableEvent* event, int n)
    {
        ASSERT(m_workerThread);
        ASSERT(m_workerThread->isCurrentThread());
        m_checkpoint.Call(n);
        event->signal();
    }

    // WorkerLoaderProxyProvider methods.
    void postTaskToLoader(std::unique_ptr<ExecutionContextTask> task) override
    {
        ASSERT(m_workerThread);
        ASSERT(m_workerThread->isCurrentThread());
        document().postTask(BLINK_FROM_HERE, std::move(task));
    }

    bool postTaskToWorkerGlobalScope(std::unique_ptr<ExecutionContextTask> task) override
    {
        ASSERT(m_workerThread);
        m_workerThread->postTask(BLINK_FROM_HERE, std::move(task));
        return true;
    }

    RefPtr<SecurityOrigin> m_securityOrigin;
    std::unique_ptr<MockWorkerReportingProxy> m_mockWorkerReportingProxy;
    std::unique_ptr<WorkerThreadForTest> m_workerThread;

    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
    Checkpoint m_checkpoint;
    // |m_loader| must be touched only from the worker thread only.
    std::unique_ptr<ThreadableLoader> m_loader;
};

class ThreadableLoaderTest : public ::testing::TestWithParam<ThreadableLoaderToTest> {
public:
    ThreadableLoaderTest()
    {
        switch (GetParam()) {
        case DocumentThreadableLoaderTest:
            m_helper = wrapUnique(new DocumentThreadableLoaderTestHelper);
            break;
        case WorkerThreadableLoaderTest:
            m_helper = wrapUnique(new WorkerThreadableLoaderTestHelper);
            break;
        }
    }

    void startLoader(const KURL& url)
    {
        ResourceRequest request(url);
        request.setRequestContext(WebURLRequest::RequestContextObject);
        m_helper->startLoader(request);
    }

    void cancelLoader() { m_helper->cancelLoader(); }
    void clearLoader() { m_helper->clearLoader(); }
    Checkpoint& checkpoint() { return m_helper->checkpoint(); }
    void callCheckpoint(int n) { m_helper->callCheckpoint(n); }

    void serveRequests()
    {
        m_helper->onServeRequests();
        Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    }

    void createLoader(CrossOriginRequestPolicy crossOriginRequestPolicy = AllowCrossOriginRequests)
    {
        m_helper->createLoader(client(), crossOriginRequestPolicy);
    }

    MockThreadableLoaderClient* client() const { return m_client.get(); }

private:
    void SetUp() override
    {
        setUpSuccessURL();
        setUpErrorURL();
        setUpRedirectURL();
        setUpRedirectLoopURL();

        m_client = MockThreadableLoaderClient::create();
        m_helper->onSetUp();
    }

    void TearDown() override
    {
        m_helper->onTearDown();
        Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
        memoryCache()->evictResources();
        m_client.reset();
    }

    void setUpSuccessURL()
    {
        URLTestHelpers::registerMockedURLLoad(successURL(), "fox-null-terminated.html", "text/html");
    }

    void setUpErrorURL()
    {
        URLTestHelpers::registerMockedErrorURLLoad(errorURL());
    }

    void setUpRedirectURL()
    {
        KURL url = redirectURL();

        WebURLLoadTiming timing;
        timing.initialize();

        WebURLResponse response;
        response.initialize();
        response.setURL(url);
        response.setHTTPStatusCode(301);
        response.setLoadTiming(timing);
        response.addHTTPHeaderField("Location", successURL().getString());
        response.addHTTPHeaderField("Access-Control-Allow-Origin", "null");

        URLTestHelpers::registerMockedURLLoadWithCustomResponse(url, "fox-null-terminated.html", "", response);
    }

    void setUpRedirectLoopURL()
    {
        KURL url = redirectLoopURL();

        WebURLLoadTiming timing;
        timing.initialize();

        WebURLResponse response;
        response.initialize();
        response.setURL(url);
        response.setHTTPStatusCode(301);
        response.setLoadTiming(timing);
        response.addHTTPHeaderField("Location", redirectLoopURL().getString());
        response.addHTTPHeaderField("Access-Control-Allow-Origin", "null");

        URLTestHelpers::registerMockedURLLoadWithCustomResponse(url, "fox-null-terminated.html", "", response);
    }

    std::unique_ptr<MockThreadableLoaderClient> m_client;
    std::unique_ptr<ThreadableLoaderTestHelper> m_helper;
};

INSTANTIATE_TEST_CASE_P(Document,
    ThreadableLoaderTest,
    ::testing::Values(DocumentThreadableLoaderTest));

INSTANTIATE_TEST_CASE_P(Worker,
    ThreadableLoaderTest,
    ::testing::Values(WorkerThreadableLoaderTest));

TEST_P(ThreadableLoaderTest, StartAndStop)
{
}

TEST_P(ThreadableLoaderTest, CancelAfterStart)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::cancelLoader));
    EXPECT_CALL(*client(), didFail(Truly(isCancellation)));
    EXPECT_CALL(checkpoint(), Call(3));

    startLoader(successURL());
    callCheckpoint(2);
    callCheckpoint(3);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, ClearAfterStart)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::clearLoader));
    EXPECT_CALL(checkpoint(), Call(3));

    startLoader(successURL());
    callCheckpoint(2);
    callCheckpoint(3);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidReceiveResponse)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::cancelLoader));
    EXPECT_CALL(*client(), didFail(Truly(isCancellation)));

    startLoader(successURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, ClearInDidReceiveResponse)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::clearLoader));

    startLoader(successURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidReceiveData)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _));
    EXPECT_CALL(*client(), didReceiveData(_, _)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::cancelLoader));
    EXPECT_CALL(*client(), didFail(Truly(isCancellation)));

    startLoader(successURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, ClearInDidReceiveData)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _));
    EXPECT_CALL(*client(), didReceiveData(_, _)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::clearLoader));

    startLoader(successURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, DidFinishLoading)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _));
    EXPECT_CALL(*client(), didReceiveData(StrEq("fox"), 4));
    // We expect didReceiveResourceTiming() calls in DocumentThreadableLoader;
    // it's used to connect DocumentThreadableLoader to WorkerThreadableLoader,
    // not to ThreadableLoaderClient.
    if (GetParam() == DocumentThreadableLoaderTest)
        EXPECT_CALL(*client(), didReceiveResourceTiming(_));
    EXPECT_CALL(*client(), didFinishLoading(_, _));

    startLoader(successURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidFinishLoading)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _));
    EXPECT_CALL(*client(), didReceiveData(_, _));
    if (GetParam() == DocumentThreadableLoaderTest)
        EXPECT_CALL(*client(), didReceiveResourceTiming(_));
    EXPECT_CALL(*client(), didFinishLoading(_, _)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::cancelLoader));

    startLoader(successURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, ClearInDidFinishLoading)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _));
    EXPECT_CALL(*client(), didReceiveData(_, _));
    if (GetParam() == DocumentThreadableLoaderTest)
        EXPECT_CALL(*client(), didReceiveResourceTiming(_));
    EXPECT_CALL(*client(), didFinishLoading(_, _)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::clearLoader));

    startLoader(successURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, DidFail)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _));
    EXPECT_CALL(*client(), didFail(Truly(isNotCancellation)));

    startLoader(errorURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidFail)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _));
    EXPECT_CALL(*client(), didFail(_)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::cancelLoader));

    startLoader(errorURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, ClearInDidFail)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _));
    EXPECT_CALL(*client(), didFail(_)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::clearLoader));

    startLoader(errorURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, DidFailInStart)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader(DenyCrossOriginRequests);
    callCheckpoint(1);

    EXPECT_CALL(*client(), didFail(ResourceError(errorDomainBlinkInternal, 0, errorURL().getString(), "Cross origin requests are not supported.")));
    EXPECT_CALL(checkpoint(), Call(2));

    startLoader(errorURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidFailInStart)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader(DenyCrossOriginRequests);
    callCheckpoint(1);

    EXPECT_CALL(*client(), didFail(_)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::cancelLoader));
    EXPECT_CALL(checkpoint(), Call(2));

    startLoader(errorURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, ClearInDidFailInStart)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader(DenyCrossOriginRequests);
    callCheckpoint(1);

    EXPECT_CALL(*client(), didFail(_)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::clearLoader));
    EXPECT_CALL(checkpoint(), Call(2));

    startLoader(errorURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, DidFailAccessControlCheck)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader(UseAccessControl);
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didFailAccessControlCheck(ResourceError(errorDomainBlinkInternal, 0, successURL().getString(), "No 'Access-Control-Allow-Origin' header is present on the requested resource. Origin 'null' is therefore not allowed access.")));

    startLoader(successURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidFailAccessControlCheck)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader(UseAccessControl);
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didFailAccessControlCheck(_)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::cancelLoader));

    startLoader(successURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, ClearInDidFailAccessControlCheck)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader(UseAccessControl);
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didFailAccessControlCheck(_)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::clearLoader));

    startLoader(successURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, RedirectDidFinishLoading)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _));
    EXPECT_CALL(*client(), didReceiveData(StrEq("fox"), 4));
    if (GetParam() == DocumentThreadableLoaderTest)
        EXPECT_CALL(*client(), didReceiveResourceTiming(_));
    EXPECT_CALL(*client(), didFinishLoading(_, _));

    startLoader(redirectURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, CancelInRedirectDidFinishLoading)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _));
    EXPECT_CALL(*client(), didReceiveData(StrEq("fox"), 4));
    if (GetParam() == DocumentThreadableLoaderTest)
        EXPECT_CALL(*client(), didReceiveResourceTiming(_));
    EXPECT_CALL(*client(), didFinishLoading(_, _)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::cancelLoader));

    startLoader(redirectURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, ClearInRedirectDidFinishLoading)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader();
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didReceiveResponseMock(_, _, _));
    EXPECT_CALL(*client(), didReceiveData(StrEq("fox"), 4));
    if (GetParam() == DocumentThreadableLoaderTest)
        EXPECT_CALL(*client(), didReceiveResourceTiming(_));
    EXPECT_CALL(*client(), didFinishLoading(_, _)).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::clearLoader));

    startLoader(redirectURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, DidFailRedirectCheck)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader(UseAccessControl);
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didFailRedirectCheck());

    startLoader(redirectLoopURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidFailRedirectCheck)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader(UseAccessControl);
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didFailRedirectCheck()).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::cancelLoader));

    startLoader(redirectLoopURL());
    callCheckpoint(2);
    serveRequests();
}

TEST_P(ThreadableLoaderTest, ClearInDidFailRedirectCheck)
{
    InSequence s;
    EXPECT_CALL(checkpoint(), Call(1));
    createLoader(UseAccessControl);
    callCheckpoint(1);

    EXPECT_CALL(checkpoint(), Call(2));
    EXPECT_CALL(*client(), didFailRedirectCheck()).WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::clearLoader));

    startLoader(redirectLoopURL());
    callCheckpoint(2);
    serveRequests();
}

} // namespace

} // namespace blink
