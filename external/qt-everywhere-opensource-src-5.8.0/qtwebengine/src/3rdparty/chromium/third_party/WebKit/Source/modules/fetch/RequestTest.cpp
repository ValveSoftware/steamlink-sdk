// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/Request.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/Document.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/HashMap.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {
namespace {

TEST(ServiceWorkerRequestTest, FromString)
{
    V8TestingScope scope;
    TrackExceptionState exceptionState;

    KURL url(ParsedURLString, "http://www.example.com/");
    Request* request = Request::create(scope.getScriptState(), url, exceptionState);
    ASSERT_FALSE(exceptionState.hadException());
    ASSERT(request);
    EXPECT_EQ(url, request->url());
}

TEST(ServiceWorkerRequestTest, FromRequest)
{
    V8TestingScope scope;
    TrackExceptionState exceptionState;

    KURL url(ParsedURLString, "http://www.example.com/");
    Request* request1 = Request::create(scope.getScriptState(), url, exceptionState);
    ASSERT(request1);

    Request* request2 = Request::create(scope.getScriptState(), request1, exceptionState);
    ASSERT_FALSE(exceptionState.hadException());
    ASSERT(request2);
    EXPECT_EQ(url, request2->url());
}

TEST(ServiceWorkerRequestTest, FromAndToWebRequest)
{
    V8TestingScope scope;
    WebServiceWorkerRequest webRequest;

    const KURL url(ParsedURLString, "http://www.example.com/");
    const String method = "GET";
    struct {
        const char* key;
        const char* value;
    } headers[] = { {"X-Foo", "bar"}, {"X-Quux", "foop"}, {0, 0} };
    const String referrer = "http://www.referrer.com/";
    const WebReferrerPolicy referrerPolicy = WebReferrerPolicyAlways;
    const WebURLRequest::RequestContext context = WebURLRequest::RequestContextAudio;
    const WebURLRequest::FetchRequestMode mode = WebURLRequest::FetchRequestModeNavigate;

    webRequest.setURL(url);
    webRequest.setMethod(method);
    webRequest.setRequestContext(context);
    webRequest.setMode(mode);
    for (int i = 0; headers[i].key; ++i)
        webRequest.setHeader(WebString::fromUTF8(headers[i].key), WebString::fromUTF8(headers[i].value));
    webRequest.setReferrer(referrer, referrerPolicy);

    Request* request = Request::create(scope.getScriptState(), webRequest);
    ASSERT(request);
    EXPECT_EQ(url, request->url());
    EXPECT_EQ(method, request->method());
    EXPECT_EQ("audio", request->context());
    EXPECT_EQ(referrer, request->referrer());
    EXPECT_EQ("navigate", request->mode());

    Headers* requestHeaders = request->getHeaders();

    WTF::HashMap<String, String> headersMap;
    for (int i = 0; headers[i].key; ++i)
        headersMap.add(headers[i].key, headers[i].value);
    EXPECT_EQ(headersMap.size(), requestHeaders->headerList()->size());
    for (WTF::HashMap<String, String>::iterator iter = headersMap.begin(); iter != headersMap.end(); ++iter) {
        TrackExceptionState exceptionState;
        EXPECT_EQ(iter->value, requestHeaders->get(iter->key, exceptionState));
        EXPECT_FALSE(exceptionState.hadException());
    }

    WebServiceWorkerRequest secondWebRequest;
    request->populateWebServiceWorkerRequest(secondWebRequest);
    EXPECT_EQ(url, KURL(secondWebRequest.url()));
    EXPECT_EQ(method, String(secondWebRequest.method()));
    EXPECT_EQ(context, secondWebRequest.requestContext());
    EXPECT_EQ(referrer, KURL(secondWebRequest.referrerUrl()));
    EXPECT_EQ(WebReferrerPolicyAlways, secondWebRequest.referrerPolicy());
    EXPECT_EQ(webRequest.headers(), secondWebRequest.headers());
    EXPECT_EQ(WebURLRequest::FetchRequestModeNoCORS, secondWebRequest.mode());
}

TEST(ServiceWorkerRequestTest, ToWebRequestStripsURLFragment)
{
    V8TestingScope scope;
    TrackExceptionState exceptionState;
    String urlWithoutFragment = "http://www.example.com/";
    String url = urlWithoutFragment + "#fragment";
    Request* request = Request::create(scope.getScriptState(), url, exceptionState);
    ASSERT(request);

    WebServiceWorkerRequest webRequest;
    request->populateWebServiceWorkerRequest(webRequest);
    EXPECT_EQ(urlWithoutFragment, KURL(webRequest.url()));
}

} // namespace
} // namespace blink
