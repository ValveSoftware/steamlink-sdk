// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/Cache.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/modules/v8/V8Request.h"
#include "bindings/modules/v8/V8Response.h"
#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FetchFormDataConsumerHandle.h"
#include "modules/fetch/GlobalFetch.h"
#include "modules/fetch/Request.h"
#include "modules/fetch/Response.h"
#include "modules/fetch/ResponseInit.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <algorithm>
#include <memory>
#include <string>

namespace blink {

namespace {

const char kNotImplementedString[] = "NotSupportedError: Method is not implemented.";

class ScopedFetcherForTests final : public GarbageCollectedFinalized<ScopedFetcherForTests>, public GlobalFetch::ScopedFetcher {
    USING_GARBAGE_COLLECTED_MIXIN(ScopedFetcherForTests);
public:
    static ScopedFetcherForTests* create()
    {
        return new ScopedFetcherForTests;
    }

    ScriptPromise fetch(ScriptState* scriptState, const RequestInfo& requestInfo, const Dictionary&, ExceptionState&) override
    {
        ++m_fetchCount;
        if (m_expectedUrl) {
            String fetchedUrl;
            if (requestInfo.isRequest())
                EXPECT_EQ(*m_expectedUrl, requestInfo.getAsRequest()->url());
            else
                EXPECT_EQ(*m_expectedUrl, requestInfo.getAsUSVString());
        }

        if (m_response) {
            ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
            const ScriptPromise promise = resolver->promise();
            resolver->resolve(m_response);
            m_response = nullptr;
            return promise;
        }
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Unexpected call to fetch, no response available."));
    }

    // This does not take ownership of its parameter. The provided sample object is used to check the parameter when called.
    void setExpectedFetchUrl(const String* expectedUrl) { m_expectedUrl = expectedUrl; }
    void setResponse(Response* response) { m_response = response; }

    int fetchCount() const { return m_fetchCount; }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_response);
        GlobalFetch::ScopedFetcher::trace(visitor);
    }

private:
    ScopedFetcherForTests()
        : m_fetchCount(0)
        , m_expectedUrl(nullptr)
    {
    }

    int m_fetchCount;
    const String* m_expectedUrl;
    Member<Response> m_response;
};

// A test implementation of the WebServiceWorkerCache interface which returns a (provided) error for every operation, and optionally checks arguments
// to methods against provided arguments. Also used as a base class for test specific caches.
class ErrorWebCacheForTests : public WebServiceWorkerCache {
public:
    ErrorWebCacheForTests(const WebServiceWorkerCacheError error)
        : m_error(error)
        , m_expectedUrl(0)
        , m_expectedQueryParams(0)
        , m_expectedBatchOperations(0) { }

    std::string getAndClearLastErrorWebCacheMethodCalled()
    {
        std::string old = m_lastErrorWebCacheMethodCalled;
        m_lastErrorWebCacheMethodCalled.clear();
        return old;
    }

    // These methods do not take ownership of their parameter. They provide an optional sample object to check parameters against.
    void setExpectedUrl(const String* expectedUrl) { m_expectedUrl = expectedUrl; }
    void setExpectedQueryParams(const QueryParams* expectedQueryParams) { m_expectedQueryParams = expectedQueryParams; }
    void setExpectedBatchOperations(const WebVector<BatchOperation>* expectedBatchOperations) { m_expectedBatchOperations = expectedBatchOperations; }

    // From WebServiceWorkerCache:
    void dispatchMatch(CacheMatchCallbacks* callbacks, const WebServiceWorkerRequest& webRequest, const QueryParams& queryParams) override
    {
        m_lastErrorWebCacheMethodCalled = "dispatchMatch";
        checkUrlIfProvided(webRequest.url());
        checkQueryParamsIfProvided(queryParams);

        std::unique_ptr<CacheMatchCallbacks> ownedCallbacks(wrapUnique(callbacks));
        return callbacks->onError(m_error);
    }

    void dispatchMatchAll(CacheWithResponsesCallbacks* callbacks, const WebServiceWorkerRequest& webRequest, const QueryParams& queryParams) override
    {
        m_lastErrorWebCacheMethodCalled = "dispatchMatchAll";
        checkUrlIfProvided(webRequest.url());
        checkQueryParamsIfProvided(queryParams);

        std::unique_ptr<CacheWithResponsesCallbacks> ownedCallbacks(wrapUnique(callbacks));
        return callbacks->onError(m_error);
    }

    void dispatchKeys(CacheWithRequestsCallbacks* callbacks, const WebServiceWorkerRequest* webRequest, const QueryParams& queryParams) override
    {
        m_lastErrorWebCacheMethodCalled = "dispatchKeys";
        if (webRequest) {
            checkUrlIfProvided(webRequest->url());
            checkQueryParamsIfProvided(queryParams);
        }

        std::unique_ptr<CacheWithRequestsCallbacks> ownedCallbacks(wrapUnique(callbacks));
        return callbacks->onError(m_error);
    }

    void dispatchBatch(CacheBatchCallbacks* callbacks, const WebVector<BatchOperation>& batchOperations) override
    {
        m_lastErrorWebCacheMethodCalled = "dispatchBatch";
        checkBatchOperationsIfProvided(batchOperations);

        std::unique_ptr<CacheBatchCallbacks> ownedCallbacks(wrapUnique(callbacks));
        return callbacks->onError(m_error);
    }

protected:
    void checkUrlIfProvided(const KURL& url)
    {
        if (!m_expectedUrl)
            return;
        EXPECT_EQ(*m_expectedUrl, url);
    }

    void checkQueryParamsIfProvided(const QueryParams& queryParams)
    {
        if (!m_expectedQueryParams)
            return;
        CompareQueryParamsForTest(*m_expectedQueryParams, queryParams);
    }

    void checkBatchOperationsIfProvided(const WebVector<BatchOperation>& batchOperations)
    {
        if (!m_expectedBatchOperations)
            return;
        const WebVector<BatchOperation> expectedBatchOperations = *m_expectedBatchOperations;
        EXPECT_EQ(expectedBatchOperations.size(), batchOperations.size());
        for (int i = 0, minsize = std::min(expectedBatchOperations.size(), batchOperations.size()); i < minsize; ++i) {
            EXPECT_EQ(expectedBatchOperations[i].operationType, batchOperations[i].operationType);
            const String expectedRequestUrl = KURL(expectedBatchOperations[i].request.url());
            EXPECT_EQ(expectedRequestUrl, KURL(batchOperations[i].request.url()));
            const String expectedResponseUrl = KURL(expectedBatchOperations[i].response.url());
            EXPECT_EQ(expectedResponseUrl, KURL(batchOperations[i].response.url()));
            CompareQueryParamsForTest(expectedBatchOperations[i].matchParams, batchOperations[i].matchParams);
        }
    }

private:
    static void CompareQueryParamsForTest(const QueryParams& expectedQueryParams, const QueryParams& queryParams)
    {
        EXPECT_EQ(expectedQueryParams.ignoreSearch, queryParams.ignoreSearch);
        EXPECT_EQ(expectedQueryParams.ignoreMethod, queryParams.ignoreMethod);
        EXPECT_EQ(expectedQueryParams.ignoreVary, queryParams.ignoreVary);
        EXPECT_EQ(expectedQueryParams.cacheName, queryParams.cacheName);
    }

    const WebServiceWorkerCacheError m_error;

    const String* m_expectedUrl;
    const QueryParams* m_expectedQueryParams;
    const WebVector<BatchOperation>* m_expectedBatchOperations;

    std::string m_lastErrorWebCacheMethodCalled;
};

class NotImplementedErrorCache : public ErrorWebCacheForTests {
public:
    NotImplementedErrorCache() : ErrorWebCacheForTests(WebServiceWorkerCacheErrorNotImplemented) { }
};

class CacheStorageTest : public ::testing::Test {
public:
    CacheStorageTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1))) { }

    Cache* createCache(ScopedFetcherForTests* fetcher, WebServiceWorkerCache* webCache)
    {
        return Cache::create(fetcher, wrapUnique(webCache));
    }

    ScriptState* getScriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    ExecutionContext* getExecutionContext() { return getScriptState()->getExecutionContext(); }
    v8::Isolate* isolate() { return getScriptState()->isolate(); }
    v8::Local<v8::Context> context() { return getScriptState()->context(); }

    Request* newRequestFromUrl(const String& url)
    {
        TrackExceptionState exceptionState;
        Request* request = Request::create(getScriptState(), url, exceptionState);
        EXPECT_FALSE(exceptionState.hadException());
        return exceptionState.hadException() ? 0 : request;
    }

    // Convenience methods for testing the returned promises.
    ScriptValue getRejectValue(ScriptPromise& promise)
    {
        ScriptValue onReject;
        promise.then(UnreachableFunction::create(getScriptState()), TestFunction::create(getScriptState(), &onReject));
        v8::MicrotasksScope::PerformCheckpoint(isolate());
        return onReject;
    }

    std::string getRejectString(ScriptPromise& promise)
    {
        ScriptValue onReject = getRejectValue(promise);
        return toCoreString(onReject.v8Value()->ToString(context()).ToLocalChecked()).ascii().data();
    }

    ScriptValue getResolveValue(ScriptPromise& promise)
    {
        ScriptValue onResolve;
        promise.then(TestFunction::create(getScriptState(), &onResolve), UnreachableFunction::create(getScriptState()));
        v8::MicrotasksScope::PerformCheckpoint(isolate());
        return onResolve;
    }

    std::string getResolveString(ScriptPromise& promise)
    {
        ScriptValue onResolve = getResolveValue(promise);
        return toCoreString(onResolve.v8Value()->ToString(context()).ToLocalChecked()).ascii().data();
    }

    ExceptionState& exceptionState()
    {
        return m_exceptionState;
    }

private:
    // A ScriptFunction that creates a test failure if it is ever called.
    class UnreachableFunction : public ScriptFunction {
    public:
        static v8::Local<v8::Function> create(ScriptState* scriptState)
        {
            UnreachableFunction* self = new UnreachableFunction(scriptState);
            return self->bindToV8Function();
        }

        ScriptValue call(ScriptValue value) override
        {
            ADD_FAILURE() << "Unexpected call to a null ScriptFunction.";
            return value;
        }
    private:
        UnreachableFunction(ScriptState* scriptState) : ScriptFunction(scriptState) { }
    };

    // A ScriptFunction that saves its parameter; used by tests to assert on correct
    // values being passed.
    class TestFunction : public ScriptFunction {
    public:
        static v8::Local<v8::Function> create(ScriptState* scriptState, ScriptValue* outValue)
        {
            TestFunction* self = new TestFunction(scriptState, outValue);
            return self->bindToV8Function();
        }

        ScriptValue call(ScriptValue value) override
        {
            ASSERT(!value.isEmpty());
            *m_value = value;
            return value;
        }

    private:
        TestFunction(ScriptState* scriptState, ScriptValue* outValue) : ScriptFunction(scriptState), m_value(outValue) { }

        ScriptValue* m_value;
    };

    // Lifetime is that of the text fixture.
    std::unique_ptr<DummyPageHolder> m_page;

    NonThrowableExceptionState m_exceptionState;
};

RequestInfo stringToRequestInfo(const String& value)
{
    RequestInfo info;
    info.setUSVString(value);
    return info;
}

RequestInfo requestToRequestInfo(Request* value)
{
    RequestInfo info;
    info.setRequest(value);
    return info;
}

TEST_F(CacheStorageTest, Basics)
{
    ScriptState::Scope scope(getScriptState());
    ScopedFetcherForTests* fetcher = ScopedFetcherForTests::create();
    ErrorWebCacheForTests* testCache;
    Cache* cache = createCache(fetcher, testCache = new NotImplementedErrorCache());
    ASSERT(cache);

    const String url = "http://www.cachetest.org/";

    CacheQueryOptions options;
    ScriptPromise matchPromise = cache->match(getScriptState(), stringToRequestInfo(url), options, exceptionState());
    EXPECT_EQ(kNotImplementedString, getRejectString(matchPromise));

    cache = createCache(fetcher, testCache = new ErrorWebCacheForTests(WebServiceWorkerCacheErrorNotFound));
    matchPromise = cache->match(getScriptState(), stringToRequestInfo(url), options, exceptionState());
    ScriptValue scriptValue = getResolveValue(matchPromise);
    EXPECT_TRUE(scriptValue.isUndefined());

    cache = createCache(fetcher, testCache = new ErrorWebCacheForTests(WebServiceWorkerCacheErrorExists));
    matchPromise = cache->match(getScriptState(), stringToRequestInfo(url), options, exceptionState());
    EXPECT_EQ("InvalidAccessError: Entry already exists.", getRejectString(matchPromise));
}

// Tests that arguments are faithfully passed on calls to Cache methods, except for methods which use batch operations,
// which are tested later.
TEST_F(CacheStorageTest, BasicArguments)
{
    ScriptState::Scope scope(getScriptState());
    ScopedFetcherForTests* fetcher = ScopedFetcherForTests::create();
    ErrorWebCacheForTests* testCache;
    Cache* cache = createCache(fetcher, testCache = new NotImplementedErrorCache());
    ASSERT(cache);

    const String url = "http://www.cache.arguments.test/";
    testCache->setExpectedUrl(&url);

    WebServiceWorkerCache::QueryParams expectedQueryParams;
    expectedQueryParams.ignoreVary = true;
    expectedQueryParams.cacheName = "this is a cache name";
    testCache->setExpectedQueryParams(&expectedQueryParams);

    CacheQueryOptions options;
    options.setIgnoreVary(1);
    options.setCacheName(expectedQueryParams.cacheName);

    Request* request = newRequestFromUrl(url);
    ASSERT(request);
    ScriptPromise matchResult = cache->match(getScriptState(), requestToRequestInfo(request), options, exceptionState());
    EXPECT_EQ("dispatchMatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(matchResult));

    ScriptPromise stringMatchResult = cache->match(getScriptState(), stringToRequestInfo(url), options, exceptionState());
    EXPECT_EQ("dispatchMatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(stringMatchResult));

    request = newRequestFromUrl(url);
    ASSERT(request);
    ScriptPromise matchAllResult = cache->matchAll(getScriptState(), requestToRequestInfo(request), options, exceptionState());
    EXPECT_EQ("dispatchMatchAll", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(matchAllResult));

    ScriptPromise stringMatchAllResult = cache->matchAll(getScriptState(), stringToRequestInfo(url), options, exceptionState());
    EXPECT_EQ("dispatchMatchAll", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(stringMatchAllResult));

    ScriptPromise keysResult1 = cache->keys(getScriptState(), exceptionState());
    EXPECT_EQ("dispatchKeys", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(keysResult1));

    request = newRequestFromUrl(url);
    ASSERT(request);
    ScriptPromise keysResult2 = cache->keys(getScriptState(), requestToRequestInfo(request), options, exceptionState());
    EXPECT_EQ("dispatchKeys", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(keysResult2));

    ScriptPromise stringKeysResult2 = cache->keys(getScriptState(), stringToRequestInfo(url), options, exceptionState());
    EXPECT_EQ("dispatchKeys", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(stringKeysResult2));
}

// Tests that arguments are faithfully passed to API calls that degrade to batch operations.
TEST_F(CacheStorageTest, BatchOperationArguments)
{
    ScriptState::Scope scope(getScriptState());
    ScopedFetcherForTests* fetcher = ScopedFetcherForTests::create();
    ErrorWebCacheForTests* testCache;
    Cache* cache = createCache(fetcher, testCache = new NotImplementedErrorCache());
    ASSERT(cache);

    WebServiceWorkerCache::QueryParams expectedQueryParams;
    expectedQueryParams.cacheName = "this is another cache name";
    testCache->setExpectedQueryParams(&expectedQueryParams);

    CacheQueryOptions options;
    options.setCacheName(expectedQueryParams.cacheName);

    const String url = "http://batch.operations.test/";
    Request* request = newRequestFromUrl(url);
    ASSERT(request);

    WebServiceWorkerResponse webResponse;
    webResponse.setURL(KURL(ParsedURLString, url));
    Response* response = Response::create(getScriptState(), webResponse);

    WebVector<WebServiceWorkerCache::BatchOperation> expectedDeleteOperations(size_t(1));
    {
        WebServiceWorkerCache::BatchOperation deleteOperation;
        deleteOperation.operationType = WebServiceWorkerCache::OperationTypeDelete;
        request->populateWebServiceWorkerRequest(deleteOperation.request);
        deleteOperation.matchParams = expectedQueryParams;
        expectedDeleteOperations[0] = deleteOperation;
    }
    testCache->setExpectedBatchOperations(&expectedDeleteOperations);

    ScriptPromise deleteResult = cache->deleteFunction(getScriptState(), requestToRequestInfo(request), options, exceptionState());
    EXPECT_EQ("dispatchBatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(deleteResult));

    ScriptPromise stringDeleteResult = cache->deleteFunction(getScriptState(), stringToRequestInfo(url), options, exceptionState());
    EXPECT_EQ("dispatchBatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(stringDeleteResult));

    WebVector<WebServiceWorkerCache::BatchOperation> expectedPutOperations(size_t(1));
    {
        WebServiceWorkerCache::BatchOperation putOperation;
        putOperation.operationType = WebServiceWorkerCache::OperationTypePut;
        request->populateWebServiceWorkerRequest(putOperation.request);
        response->populateWebServiceWorkerResponse(putOperation.response);
        expectedPutOperations[0] = putOperation;
    }
    testCache->setExpectedBatchOperations(&expectedPutOperations);

    request = newRequestFromUrl(url);
    ASSERT(request);
    ScriptPromise putResult = cache->put(getScriptState(), requestToRequestInfo(request), response->clone(getScriptState(), exceptionState()), exceptionState());
    EXPECT_EQ("dispatchBatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(putResult));

    ScriptPromise stringPutResult = cache->put(getScriptState(), stringToRequestInfo(url), response, exceptionState());
    EXPECT_EQ("dispatchBatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
    EXPECT_EQ(kNotImplementedString, getRejectString(stringPutResult));

    // FIXME: test add & addAll.
}

class MatchTestCache : public NotImplementedErrorCache {
public:
    MatchTestCache(WebServiceWorkerResponse& response)
        : m_response(response) { }

    // From WebServiceWorkerCache:
    void dispatchMatch(CacheMatchCallbacks* callbacks, const WebServiceWorkerRequest& webRequest, const QueryParams& queryParams) override
    {
        std::unique_ptr<CacheMatchCallbacks> ownedCallbacks(wrapUnique(callbacks));
        return callbacks->onSuccess(m_response);
    }

private:
    WebServiceWorkerResponse& m_response;
};

TEST_F(CacheStorageTest, MatchResponseTest)
{
    ScriptState::Scope scope(getScriptState());
    ScopedFetcherForTests* fetcher = ScopedFetcherForTests::create();
    const String requestUrl = "http://request.url/";
    const String responseUrl = "http://match.response.test/";

    WebServiceWorkerResponse webResponse;
    webResponse.setURL(KURL(ParsedURLString, responseUrl));
    webResponse.setResponseType(WebServiceWorkerResponseTypeDefault);

    Cache* cache = createCache(fetcher, new MatchTestCache(webResponse));
    CacheQueryOptions options;

    ScriptPromise result = cache->match(getScriptState(), stringToRequestInfo(requestUrl), options, exceptionState());
    ScriptValue scriptValue = getResolveValue(result);
    Response* response = V8Response::toImplWithTypeCheck(isolate(), scriptValue.v8Value());
    ASSERT_TRUE(response);
    EXPECT_EQ(responseUrl, response->url());
}

class KeysTestCache : public NotImplementedErrorCache {
public:
    KeysTestCache(WebVector<WebServiceWorkerRequest>& requests)
        : m_requests(requests) { }

    void dispatchKeys(CacheWithRequestsCallbacks* callbacks, const WebServiceWorkerRequest* webRequest, const QueryParams& queryParams) override
    {
        std::unique_ptr<CacheWithRequestsCallbacks> ownedCallbacks(wrapUnique(callbacks));
        return callbacks->onSuccess(m_requests);
    }

private:
    WebVector<WebServiceWorkerRequest>& m_requests;
};

TEST_F(CacheStorageTest, KeysResponseTest)
{
    ScriptState::Scope scope(getScriptState());
    ScopedFetcherForTests* fetcher = ScopedFetcherForTests::create();
    const String url1 = "http://first.request/";
    const String url2 = "http://second.request/";

    Vector<String> expectedUrls(size_t(2));
    expectedUrls[0] = url1;
    expectedUrls[1] = url2;

    WebVector<WebServiceWorkerRequest> webRequests(size_t(2));
    webRequests[0].setURL(KURL(ParsedURLString, url1));
    webRequests[1].setURL(KURL(ParsedURLString, url2));

    Cache* cache = createCache(fetcher, new KeysTestCache(webRequests));

    ScriptPromise result = cache->keys(getScriptState(), exceptionState());
    ScriptValue scriptValue = getResolveValue(result);

    Vector<v8::Local<v8::Value>> requests = toImplArray<Vector<v8::Local<v8::Value>>>(scriptValue.v8Value(), 0, isolate(), exceptionState());
    EXPECT_EQ(expectedUrls.size(), requests.size());
    for (int i = 0, minsize = std::min(expectedUrls.size(), requests.size()); i < minsize; ++i) {
        Request* request = V8Request::toImplWithTypeCheck(isolate(), requests[i]);
        EXPECT_TRUE(request);
        if (request)
            EXPECT_EQ(expectedUrls[i], request->url());
    }
}

class MatchAllAndBatchTestCache : public NotImplementedErrorCache {
public:
    MatchAllAndBatchTestCache(WebVector<WebServiceWorkerResponse>& responses)
        : m_responses(responses) { }

    void dispatchMatchAll(CacheWithResponsesCallbacks* callbacks, const WebServiceWorkerRequest& webRequest, const QueryParams& queryParams) override
    {
        std::unique_ptr<CacheWithResponsesCallbacks> ownedCallbacks(wrapUnique(callbacks));
        return callbacks->onSuccess(m_responses);
    }

    void dispatchBatch(CacheBatchCallbacks* callbacks, const WebVector<BatchOperation>& batchOperations) override
    {
        std::unique_ptr<CacheBatchCallbacks> ownedCallbacks(wrapUnique(callbacks));
        return callbacks->onSuccess();
    }

private:
    WebVector<WebServiceWorkerResponse>& m_responses;
};

TEST_F(CacheStorageTest, MatchAllAndBatchResponseTest)
{
    ScriptState::Scope scope(getScriptState());
    ScopedFetcherForTests* fetcher = ScopedFetcherForTests::create();
    const String url1 = "http://first.response/";
    const String url2 = "http://second.response/";

    Vector<String> expectedUrls(size_t(2));
    expectedUrls[0] = url1;
    expectedUrls[1] = url2;

    WebVector<WebServiceWorkerResponse> webResponses(size_t(2));
    webResponses[0].setURL(KURL(ParsedURLString, url1));
    webResponses[0].setResponseType(WebServiceWorkerResponseTypeDefault);
    webResponses[1].setURL(KURL(ParsedURLString, url2));
    webResponses[1].setResponseType(WebServiceWorkerResponseTypeDefault);

    Cache* cache = createCache(fetcher, new MatchAllAndBatchTestCache(webResponses));

    CacheQueryOptions options;
    ScriptPromise result = cache->matchAll(getScriptState(), stringToRequestInfo("http://some.url/"), options, exceptionState());
    ScriptValue scriptValue = getResolveValue(result);

    Vector<v8::Local<v8::Value>> responses = toImplArray<Vector<v8::Local<v8::Value>>>(scriptValue.v8Value(), 0, isolate(), exceptionState());
    EXPECT_EQ(expectedUrls.size(), responses.size());
    for (int i = 0, minsize = std::min(expectedUrls.size(), responses.size()); i < minsize; ++i) {
        Response* response = V8Response::toImplWithTypeCheck(isolate(), responses[i]);
        EXPECT_TRUE(response);
        if (response)
            EXPECT_EQ(expectedUrls[i], response->url());
    }

    result = cache->deleteFunction(getScriptState(), stringToRequestInfo("http://some.url/"), options, exceptionState());
    scriptValue = getResolveValue(result);
    EXPECT_TRUE(scriptValue.v8Value()->IsBoolean());
    EXPECT_EQ(true, scriptValue.v8Value().As<v8::Boolean>()->Value());
}

TEST_F(CacheStorageTest, Add)
{
    ScriptState::Scope scope(getScriptState());
    ScopedFetcherForTests* fetcher = ScopedFetcherForTests::create();
    const String url = "http://www.cacheadd.test/";
    const String contentType = "text/plain";
    const String content = "hello cache";

    ErrorWebCacheForTests* testCache;
    Cache* cache = createCache(fetcher, testCache = new NotImplementedErrorCache());

    fetcher->setExpectedFetchUrl(&url);

    Request* request = newRequestFromUrl(url);
    Response* response = Response::create(getScriptState(), new BodyStreamBuffer(getScriptState(), FetchFormDataConsumerHandle::create(content)), contentType, ResponseInit(), exceptionState());
    fetcher->setResponse(response);

    WebVector<WebServiceWorkerCache::BatchOperation> expectedPutOperations(size_t(1));
    {
        WebServiceWorkerCache::BatchOperation putOperation;
        putOperation.operationType = WebServiceWorkerCache::OperationTypePut;
        request->populateWebServiceWorkerRequest(putOperation.request);
        response->populateWebServiceWorkerResponse(putOperation.response);
        expectedPutOperations[0] = putOperation;
    }
    testCache->setExpectedBatchOperations(&expectedPutOperations);

    ScriptPromise addResult = cache->add(getScriptState(), requestToRequestInfo(request), exceptionState());

    EXPECT_EQ(kNotImplementedString, getRejectString(addResult));
    EXPECT_EQ(1, fetcher->fetchCount());
    EXPECT_EQ("dispatchBatch", testCache->getAndClearLastErrorWebCacheMethodCalled());
}

} // namespace
} // namespace blink
