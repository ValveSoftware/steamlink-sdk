// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchResponseData.h"

#include "core/dom/DOMArrayBuffer.h"
#include "modules/fetch/FetchHeaderList.h"
#include "platform/blob/BlobData.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FetchResponseDataTest : public ::testing::Test {
public:
    FetchResponseData* createInternalResponse()
    {
        FetchResponseData* internalResponse = FetchResponseData::create();
        internalResponse->setStatus(200);
        internalResponse->setURL(KURL(ParsedURLString, "http://www.example.com"));
        internalResponse->headerList()->append("set-cookie", "foo");
        internalResponse->headerList()->append("bar", "bar");
        internalResponse->headerList()->append("cache-control", "no-cache");
        return internalResponse;
    }

    void CheckHeaders(const WebServiceWorkerResponse& webResponse)
    {
        EXPECT_STREQ("foo", webResponse.getHeader("set-cookie").utf8().c_str());
        EXPECT_STREQ("bar", webResponse.getHeader("bar").utf8().c_str());
        EXPECT_STREQ("no-cache", webResponse.getHeader("cache-control").utf8().c_str());
    }
};

TEST_F(FetchResponseDataTest, HeaderList)
{
    FetchResponseData* responseData = createInternalResponse();

    Vector<String> setCookieValues;
    responseData->headerList()->getAll("set-cookie", setCookieValues);
    ASSERT_EQ(1U, setCookieValues.size());
    EXPECT_EQ("foo", setCookieValues[0]);

    Vector<String> barValues;
    responseData->headerList()->getAll("bar", barValues);
    ASSERT_EQ(1U, barValues.size());
    EXPECT_EQ("bar", barValues[0]);

    Vector<String> cacheControlValues;
    responseData->headerList()->getAll("cache-control", cacheControlValues);
    ASSERT_EQ(1U, cacheControlValues.size());
    EXPECT_EQ("no-cache", cacheControlValues[0]);
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerDefaultType)
{
    WebServiceWorkerResponse webResponse;
    FetchResponseData* internalResponse = createInternalResponse();

    internalResponse->populateWebServiceWorkerResponse(webResponse);
    EXPECT_EQ(WebServiceWorkerResponseTypeDefault, webResponse.responseType());
    CheckHeaders(webResponse);
}

TEST_F(FetchResponseDataTest, BasicFilter)
{
    FetchResponseData* internalResponse = createInternalResponse();
    FetchResponseData* basicResponseData = internalResponse->createBasicFilteredResponse();

    EXPECT_EQ(internalResponse, basicResponseData->internalResponse());

    EXPECT_FALSE(basicResponseData->headerList()->has("set-cookie"));

    Vector<String> barValues;
    basicResponseData->headerList()->getAll("bar", barValues);
    ASSERT_EQ(1U, barValues.size());
    EXPECT_EQ("bar", barValues[0]);

    Vector<String> cacheControlValues;
    basicResponseData->headerList()->getAll("cache-control", cacheControlValues);
    ASSERT_EQ(1U, cacheControlValues.size());
    EXPECT_EQ("no-cache", cacheControlValues[0]);
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerBasicType)
{
    WebServiceWorkerResponse webResponse;
    FetchResponseData* internalResponse = createInternalResponse();
    FetchResponseData* basicResponseData = internalResponse->createBasicFilteredResponse();

    basicResponseData->populateWebServiceWorkerResponse(webResponse);
    EXPECT_EQ(WebServiceWorkerResponseTypeBasic, webResponse.responseType());
    CheckHeaders(webResponse);
}

TEST_F(FetchResponseDataTest, CORSFilter)
{
    FetchResponseData* internalResponse = createInternalResponse();
    FetchResponseData* corsResponseData = internalResponse->createCORSFilteredResponse();

    EXPECT_EQ(internalResponse, corsResponseData->internalResponse());

    EXPECT_FALSE(corsResponseData->headerList()->has("set-cookie"));

    EXPECT_FALSE(corsResponseData->headerList()->has("bar"));

    Vector<String> cacheControlValues;
    corsResponseData->headerList()->getAll("cache-control", cacheControlValues);
    ASSERT_EQ(1U, cacheControlValues.size());
    EXPECT_EQ("no-cache", cacheControlValues[0]);
}

TEST_F(FetchResponseDataTest, CORSFilterOnResponseWithAccessControlExposeHeaders)
{
    FetchResponseData* internalResponse = createInternalResponse();
    internalResponse->headerList()->append("access-control-expose-headers", "set-cookie, bar");

    FetchResponseData* corsResponseData = internalResponse->createCORSFilteredResponse();

    EXPECT_EQ(internalResponse, corsResponseData->internalResponse());

    EXPECT_FALSE(corsResponseData->headerList()->has("set-cookie"));

    Vector<String> barValues;
    corsResponseData->headerList()->getAll("bar", barValues);
    ASSERT_EQ(1U, barValues.size());
    EXPECT_EQ("bar", barValues[0]);
}

TEST_F(FetchResponseDataTest, CORSFilterWithEmptyHeaderSet)
{
    FetchResponseData* internalResponse = createInternalResponse();
    FetchResponseData* corsResponseData = internalResponse->createCORSFilteredResponse(HTTPHeaderSet());

    EXPECT_EQ(internalResponse, corsResponseData->internalResponse());

    EXPECT_FALSE(corsResponseData->headerList()->has("set-cookie"));

    EXPECT_FALSE(corsResponseData->headerList()->has("bar"));

    Vector<String> cacheControlValues;
    corsResponseData->headerList()->getAll("cache-control", cacheControlValues);
    ASSERT_EQ(1U, cacheControlValues.size());
    EXPECT_EQ("no-cache", cacheControlValues[0]);
}

TEST_F(FetchResponseDataTest, CORSFilterWithEmptyHeaderSetOnResponseWithAccessControlExposeHeaders)
{
    FetchResponseData* internalResponse = createInternalResponse();
    internalResponse->headerList()->append("access-control-expose-headers", "set-cookie, bar");

    FetchResponseData* corsResponseData = internalResponse->createCORSFilteredResponse(HTTPHeaderSet());

    EXPECT_EQ(internalResponse, corsResponseData->internalResponse());

    EXPECT_FALSE(corsResponseData->headerList()->has("set-cookie"));

    EXPECT_FALSE(corsResponseData->headerList()->has("bar"));

    Vector<String> cacheControlValues;
    corsResponseData->headerList()->getAll("cache-control", cacheControlValues);
    ASSERT_EQ(1U, cacheControlValues.size());
    EXPECT_EQ("no-cache", cacheControlValues[0]);
}

TEST_F(FetchResponseDataTest, CORSFilterWithExplicitHeaderSet)
{
    FetchResponseData* internalResponse = createInternalResponse();
    HTTPHeaderSet exposedHeaders;
    exposedHeaders.add("set-cookie");
    exposedHeaders.add("bar");

    FetchResponseData* corsResponseData = internalResponse->createCORSFilteredResponse(exposedHeaders);

    EXPECT_EQ(internalResponse, corsResponseData->internalResponse());

    EXPECT_FALSE(corsResponseData->headerList()->has("set-cookie"));

    Vector<String> barValues;
    corsResponseData->headerList()->getAll("bar", barValues);
    ASSERT_EQ(1U, barValues.size());
    EXPECT_EQ("bar", barValues[0]);
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerCORSType)
{
    WebServiceWorkerResponse webResponse;
    FetchResponseData* internalResponse = createInternalResponse();
    FetchResponseData* corsResponseData = internalResponse->createCORSFilteredResponse();

    corsResponseData->populateWebServiceWorkerResponse(webResponse);
    EXPECT_EQ(WebServiceWorkerResponseTypeCORS, webResponse.responseType());
    CheckHeaders(webResponse);
}

TEST_F(FetchResponseDataTest, OpaqueFilter)
{
    FetchResponseData* internalResponse = createInternalResponse();
    FetchResponseData* opaqueResponseData = internalResponse->createOpaqueFilteredResponse();

    EXPECT_EQ(internalResponse, opaqueResponseData->internalResponse());

    EXPECT_FALSE(opaqueResponseData->headerList()->has("set-cookie"));
    EXPECT_FALSE(opaqueResponseData->headerList()->has("bar"));
    EXPECT_FALSE(opaqueResponseData->headerList()->has("cache-control"));
}

TEST_F(FetchResponseDataTest, OpaqueRedirectFilter)
{
    FetchResponseData* internalResponse = createInternalResponse();
    FetchResponseData* opaqueResponseData = internalResponse->createOpaqueRedirectFilteredResponse();

    EXPECT_EQ(internalResponse, opaqueResponseData->internalResponse());

    EXPECT_EQ(opaqueResponseData->headerList()->size(), 0u);
    EXPECT_EQ(opaqueResponseData->url(), internalResponse->url());
}

TEST_F(FetchResponseDataTest, OpaqueFilterOnResponseWithAccessControlExposeHeaders)
{
    FetchResponseData* internalResponse = createInternalResponse();
    internalResponse->headerList()->append("access-control-expose-headers", "set-cookie, bar");

    FetchResponseData* opaqueResponseData = internalResponse->createOpaqueFilteredResponse();

    EXPECT_EQ(internalResponse, opaqueResponseData->internalResponse());

    EXPECT_FALSE(opaqueResponseData->headerList()->has("set-cookie"));
    EXPECT_FALSE(opaqueResponseData->headerList()->has("bar"));
    EXPECT_FALSE(opaqueResponseData->headerList()->has("cache-control"));
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerOpaqueType)
{
    WebServiceWorkerResponse webResponse;
    FetchResponseData* internalResponse = createInternalResponse();
    FetchResponseData* opaqueResponseData = internalResponse->createOpaqueFilteredResponse();

    opaqueResponseData->populateWebServiceWorkerResponse(webResponse);
    EXPECT_EQ(WebServiceWorkerResponseTypeOpaque, webResponse.responseType());
    CheckHeaders(webResponse);
}

TEST_F(FetchResponseDataTest, ToWebServiceWorkerOpaqueRedirectType)
{
    WebServiceWorkerResponse webResponse;
    FetchResponseData* internalResponse = createInternalResponse();
    FetchResponseData* opaqueRedirectResponseData = internalResponse->createOpaqueRedirectFilteredResponse();

    opaqueRedirectResponseData->populateWebServiceWorkerResponse(webResponse);
    EXPECT_EQ(WebServiceWorkerResponseTypeOpaqueRedirect, webResponse.responseType());
    CheckHeaders(webResponse);
}

} // namespace blink
