/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/fetch/FetchContext.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/network/ResourceRequest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"

namespace blink {

// An URL for the original request.
const char kResourceURL[] = "http://resource.com/";

// The origin time of our first request.
const char kOriginalRequestDateAsString[] = "Thu, 25 May 1977 18:30:00 GMT";
const double kOriginalRequestDateAsDouble = 233433000.;

const char kOneDayBeforeOriginalRequest[] = "Wed, 24 May 1977 18:30:00 GMT";
const char kOneDayAfterOriginalRequest[] = "Fri, 26 May 1977 18:30:00 GMT";

class MockFetchContext : public FetchContext {
public:
    static MockFetchContext* create()
    {
        return new MockFetchContext;
    }

    ~MockFetchContext() { }

    bool allowImage(bool imagesEnabled, const KURL&) const override { return true; }
    bool canRequest(Resource::Type, const ResourceRequest&, const KURL&, const ResourceLoaderOptions&, bool forPreload, FetchRequest::OriginRestriction) const override { return true; }

private:
    MockFetchContext() { }
};

class CachingCorrectnessTest : public ::testing::Test {
protected:
    static void advanceClock(double seconds)
    {
        s_timeElapsed += seconds;
    }

    Resource* resourceFromResourceResponse(ResourceResponse response, Resource::Type type = Resource::Raw)
    {
        if (response.url().isNull())
            response.setURL(KURL(ParsedURLString, kResourceURL));
        Resource* resource = nullptr;
        switch (type) {
        case Resource::Raw:
            resource = Resource::create(ResourceRequest(response.url()), type);
            break;
        case Resource::Image:
            resource = ImageResource::create(ResourceRequest(response.url()));
            break;
        default:
            EXPECT_TRUE(false) << "'Unreachable' code was reached";
            return nullptr;
        }
        resource->setResponse(response);
        resource->finish();
        // Because we didn't give any real data, an image will have set its
        // status to DecodeError. Override it so the resource is cacaheable
        // for testing purposes.
        if (type == Resource::Image)
            resource->setStatus(Resource::Cached);
        memoryCache()->add(resource);

        return resource;
    }

    Resource* resourceFromResourceRequest(ResourceRequest request, Resource::Type type = Resource::Raw)
    {
        if (request.url().isNull())
            request.setURL(KURL(ParsedURLString, kResourceURL));
        Resource* resource =
            Resource::create(request, type);
        resource->setResponse(ResourceResponse(KURL(ParsedURLString, kResourceURL), "text/html", 0, nullAtom, String()));
        resource->finish();
        memoryCache()->add(resource);

        return resource;
    }

    Resource* fetch()
    {
        FetchRequest fetchRequest(ResourceRequest(KURL(ParsedURLString, kResourceURL)), FetchInitiatorInfo());
        return RawResource::fetchSynchronously(fetchRequest, fetcher());
    }

    Resource* fetchImage()
    {
        FetchRequest fetchRequest(ResourceRequest(KURL(ParsedURLString, kResourceURL)), FetchInitiatorInfo());
        return ImageResource::fetch(fetchRequest, fetcher());
    }

    ResourceFetcher* fetcher() const { return m_fetcher.get(); }

private:
    static double returnMockTime()
    {
        return kOriginalRequestDateAsDouble + s_timeElapsed;
    }

    virtual void SetUp()
    {
        // Save the global memory cache to restore it upon teardown.
        m_globalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());

        m_fetcher = ResourceFetcher::create(MockFetchContext::create());

        s_timeElapsed = 0.0;
        m_originalTimeFunction = setTimeFunctionsForTesting(returnMockTime);
    }

    virtual void TearDown()
    {
        memoryCache()->evictResources();

        // Yield the ownership of the global memory cache back.
        replaceMemoryCacheForTesting(m_globalMemoryCache.release());

        setTimeFunctionsForTesting(m_originalTimeFunction);
    }

    Persistent<MemoryCache> m_globalMemoryCache;
    Persistent<ResourceFetcher> m_fetcher;
    TimeFunction m_originalTimeFunction;
    static double s_timeElapsed;
};

double CachingCorrectnessTest::s_timeElapsed;

TEST_F(CachingCorrectnessTest, FreshFromLastModified)
{
    ResourceResponse fresh200Response;
    fresh200Response.setHTTPStatusCode(200);
    fresh200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
    fresh200Response.setHTTPHeaderField("Last-Modified", kOneDayBeforeOriginalRequest);

    Resource* fresh200 = resourceFromResourceResponse(fresh200Response);

    // Advance the clock within the implicit freshness period of this resource before we make a request.
    advanceClock(600.);

    Resource* fetched = fetch();
    EXPECT_EQ(fresh200, fetched);
}

TEST_F(CachingCorrectnessTest, FreshFromExpires)
{
    ResourceResponse fresh200Response;
    fresh200Response.setHTTPStatusCode(200);
    fresh200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
    fresh200Response.setHTTPHeaderField("Expires", kOneDayAfterOriginalRequest);

    Resource* fresh200 = resourceFromResourceResponse(fresh200Response);

    // Advance the clock within the freshness period of this resource before we make a request.
    advanceClock(24. * 60. * 60. - 15.);

    Resource* fetched = fetch();
    EXPECT_EQ(fresh200, fetched);
}

TEST_F(CachingCorrectnessTest, FreshFromMaxAge)
{
    ResourceResponse fresh200Response;
    fresh200Response.setHTTPStatusCode(200);
    fresh200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
    fresh200Response.setHTTPHeaderField("Cache-Control", "max-age=600");

    Resource* fresh200 = resourceFromResourceResponse(fresh200Response);

    // Advance the clock within the freshness period of this resource before we make a request.
    advanceClock(500.);

    Resource* fetched = fetch();
    EXPECT_EQ(fresh200, fetched);
}

// The strong validator causes a revalidation to be launched, and the proxy and original resources leak because of their reference loop.
TEST_F(CachingCorrectnessTest, DISABLED_ExpiredFromLastModified)
{
    ResourceResponse expired200Response;
    expired200Response.setHTTPStatusCode(200);
    expired200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
    expired200Response.setHTTPHeaderField("Last-Modified", kOneDayBeforeOriginalRequest);

    Resource* expired200 = resourceFromResourceResponse(expired200Response);

    // Advance the clock beyond the implicit freshness period.
    advanceClock(24. * 60. * 60. * 0.2);

    Resource* fetched = fetch();
    EXPECT_NE(expired200, fetched);
}

TEST_F(CachingCorrectnessTest, ExpiredFromExpires)
{
    ResourceResponse expired200Response;
    expired200Response.setHTTPStatusCode(200);
    expired200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
    expired200Response.setHTTPHeaderField("Expires", kOneDayAfterOriginalRequest);

    Resource* expired200 = resourceFromResourceResponse(expired200Response);

    // Advance the clock within the expiredness period of this resource before we make a request.
    advanceClock(24. * 60. * 60. + 15.);

    Resource* fetched = fetch();
    EXPECT_NE(expired200, fetched);
}

// If the image hasn't been loaded in this "document" before, then it shouldn't have list of available images logic.
TEST_F(CachingCorrectnessTest, NewImageExpiredFromExpires)
{
    ResourceResponse expired200Response;
    expired200Response.setHTTPStatusCode(200);
    expired200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
    expired200Response.setHTTPHeaderField("Expires", kOneDayAfterOriginalRequest);

    Resource* expired200 = resourceFromResourceResponse(expired200Response, Resource::Image);

    // Advance the clock within the expiredness period of this resource before we make a request.
    advanceClock(24. * 60. * 60. + 15.);

    Resource* fetched = fetchImage();
    EXPECT_NE(expired200, fetched);
}

// If the image has been loaded in this "document" before, then it should have list of available images logic, and so
// normal cache testing should be bypassed.
TEST_F(CachingCorrectnessTest, ReuseImageExpiredFromExpires)
{
    ResourceResponse expired200Response;
    expired200Response.setHTTPStatusCode(200);
    expired200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
    expired200Response.setHTTPHeaderField("Expires", kOneDayAfterOriginalRequest);

    Resource* expired200 = resourceFromResourceResponse(expired200Response, Resource::Image);

    // Advance the clock within the freshness period, and make a request to add this image to the document resources.
    advanceClock(15.);
    Resource* firstFetched = fetchImage();
    EXPECT_EQ(expired200, firstFetched);

    // Advance the clock within the expiredness period of this resource before we make a request.
    advanceClock(24. * 60. * 60. + 15.);

    Resource* fetched = fetchImage();
    EXPECT_EQ(expired200, fetched);
}

TEST_F(CachingCorrectnessTest, ExpiredFromMaxAge)
{
    ResourceResponse expired200Response;
    expired200Response.setHTTPStatusCode(200);
    expired200Response.setHTTPHeaderField("Date", kOriginalRequestDateAsString);
    expired200Response.setHTTPHeaderField("Cache-Control", "max-age=600");

    Resource* expired200 = resourceFromResourceResponse(expired200Response);

    // Advance the clock within the expiredness period of this resource before we make a request.
    advanceClock(700.);

    Resource* fetched = fetch();
    EXPECT_NE(expired200, fetched);
}

TEST_F(CachingCorrectnessTest, FreshButNoCache)
{
    ResourceResponse fresh200NocacheResponse;
    fresh200NocacheResponse.setHTTPStatusCode(200);
    fresh200NocacheResponse.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh200NocacheResponse.setHTTPHeaderField(HTTPNames::Expires, kOneDayAfterOriginalRequest);
    fresh200NocacheResponse.setHTTPHeaderField(HTTPNames::Cache_Control, "no-cache");

    Resource* fresh200Nocache = resourceFromResourceResponse(fresh200NocacheResponse);

    // Advance the clock within the freshness period of this resource before we make a request.
    advanceClock(24. * 60. * 60. - 15.);

    Resource* fetched = fetch();
    EXPECT_NE(fresh200Nocache, fetched);
}

TEST_F(CachingCorrectnessTest, RequestWithNoCahe)
{
    ResourceRequest noCacheRequest;
    noCacheRequest.setHTTPHeaderField(HTTPNames::Cache_Control, "no-cache");
    Resource* noCacheResource = resourceFromResourceRequest(noCacheRequest);
    Resource* fetched = fetch();
    EXPECT_NE(noCacheResource, fetched);
}

TEST_F(CachingCorrectnessTest, FreshButNoStore)
{
    ResourceResponse fresh200NostoreResponse;
    fresh200NostoreResponse.setHTTPStatusCode(200);
    fresh200NostoreResponse.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh200NostoreResponse.setHTTPHeaderField(HTTPNames::Expires, kOneDayAfterOriginalRequest);
    fresh200NostoreResponse.setHTTPHeaderField(HTTPNames::Cache_Control, "no-store");

    Resource* fresh200Nostore = resourceFromResourceResponse(fresh200NostoreResponse);

    // Advance the clock within the freshness period of this resource before we make a request.
    advanceClock(24. * 60. * 60. - 15.);

    Resource* fetched = fetch();
    EXPECT_NE(fresh200Nostore, fetched);
}

TEST_F(CachingCorrectnessTest, RequestWithNoStore)
{
    ResourceRequest noStoreRequest;
    noStoreRequest.setHTTPHeaderField(HTTPNames::Cache_Control, "no-store");
    Resource* noStoreResource = resourceFromResourceRequest(noStoreRequest);
    Resource* fetched = fetch();
    EXPECT_NE(noStoreResource, fetched);
}

// FIXME: Determine if ignoring must-revalidate for blink is correct behaviour.
// See crbug.com/340088 .
TEST_F(CachingCorrectnessTest, DISABLED_FreshButMustRevalidate)
{
    ResourceResponse fresh200MustRevalidateResponse;
    fresh200MustRevalidateResponse.setHTTPStatusCode(200);
    fresh200MustRevalidateResponse.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh200MustRevalidateResponse.setHTTPHeaderField(HTTPNames::Expires, kOneDayAfterOriginalRequest);
    fresh200MustRevalidateResponse.setHTTPHeaderField(HTTPNames::Cache_Control, "must-revalidate");

    Resource* fresh200MustRevalidate = resourceFromResourceResponse(fresh200MustRevalidateResponse);

    // Advance the clock within the freshness period of this resource before we make a request.
    advanceClock(24. * 60. * 60. - 15.);

    Resource* fetched = fetch();
    EXPECT_NE(fresh200MustRevalidate, fetched);
}

TEST_F(CachingCorrectnessTest, FreshWithFreshRedirect)
{
    KURL redirectUrl(ParsedURLString, kResourceURL);
    const char redirectTargetUrlString[] = "http://redirect-target.com";
    KURL redirectTargetUrl(ParsedURLString, redirectTargetUrlString);

    Resource* firstResource = Resource::create(ResourceRequest(redirectUrl), Resource::Raw);

    ResourceResponse fresh301Response;
    fresh301Response.setURL(redirectUrl);
    fresh301Response.setHTTPStatusCode(301);
    fresh301Response.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh301Response.setHTTPHeaderField(HTTPNames::Location, redirectTargetUrlString);
    fresh301Response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=600");

    // Add the redirect to our request.
    ResourceRequest redirectRequest = ResourceRequest(redirectTargetUrl);
    firstResource->willFollowRedirect(redirectRequest, fresh301Response);

    // Add the final response to our request.
    ResourceResponse fresh200Response;
    fresh200Response.setURL(redirectTargetUrl);
    fresh200Response.setHTTPStatusCode(200);
    fresh200Response.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh200Response.setHTTPHeaderField(HTTPNames::Expires, kOneDayAfterOriginalRequest);

    firstResource->setResponse(fresh200Response);
    firstResource->finish();
    memoryCache()->add(firstResource);

    advanceClock(500.);

    Resource* fetched = fetch();
    EXPECT_EQ(firstResource, fetched);
}

TEST_F(CachingCorrectnessTest, FreshWithStaleRedirect)
{
    KURL redirectUrl(ParsedURLString, kResourceURL);
    const char redirectTargetUrlString[] = "http://redirect-target.com";
    KURL redirectTargetUrl(ParsedURLString, redirectTargetUrlString);

    Resource* firstResource = Resource::create(ResourceRequest(redirectUrl), Resource::Raw);

    ResourceResponse stale301Response;
    stale301Response.setURL(redirectUrl);
    stale301Response.setHTTPStatusCode(301);
    stale301Response.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    stale301Response.setHTTPHeaderField(HTTPNames::Location, redirectTargetUrlString);

    // Add the redirect to our request.
    ResourceRequest redirectRequest = ResourceRequest(redirectTargetUrl);
    firstResource->willFollowRedirect(redirectRequest, stale301Response);

    // Add the final response to our request.
    ResourceResponse fresh200Response;
    fresh200Response.setURL(redirectTargetUrl);
    fresh200Response.setHTTPStatusCode(200);
    fresh200Response.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh200Response.setHTTPHeaderField(HTTPNames::Expires, kOneDayAfterOriginalRequest);

    firstResource->setResponse(fresh200Response);
    firstResource->finish();
    memoryCache()->add(firstResource);

    advanceClock(500.);

    Resource* fetched = fetch();
    EXPECT_NE(firstResource, fetched);
}

TEST_F(CachingCorrectnessTest, PostToSameURLTwice)
{
    ResourceRequest request1(KURL(ParsedURLString, kResourceURL));
    request1.setHTTPMethod(HTTPNames::POST);
    Resource* resource1 = Resource::create(ResourceRequest(request1.url()), Resource::Raw);
    resource1->setStatus(Resource::Pending);
    memoryCache()->add(resource1);

    ResourceRequest request2(KURL(ParsedURLString, kResourceURL));
    request2.setHTTPMethod(HTTPNames::POST);
    FetchRequest fetch2(request2, FetchInitiatorInfo());
    Resource* resource2 = RawResource::fetchSynchronously(fetch2, fetcher());

    EXPECT_EQ(resource2, memoryCache()->resourceForURL(request2.url()));
    EXPECT_NE(resource1, resource2);
}

TEST_F(CachingCorrectnessTest, 302RedirectNotImplicitlyFresh)
{
    KURL redirectUrl(ParsedURLString, kResourceURL);
    const char redirectTargetUrlString[] = "http://redirect-target.com";
    KURL redirectTargetUrl(ParsedURLString, redirectTargetUrlString);

    Resource* firstResource = Resource::create(ResourceRequest(redirectUrl), Resource::Raw);

    ResourceResponse fresh302Response;
    fresh302Response.setURL(redirectUrl);
    fresh302Response.setHTTPStatusCode(302);
    fresh302Response.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh302Response.setHTTPHeaderField(HTTPNames::Last_Modified, kOneDayBeforeOriginalRequest);
    fresh302Response.setHTTPHeaderField(HTTPNames::Location, redirectTargetUrlString);

    // Add the redirect to our request.
    ResourceRequest redirectRequest = ResourceRequest(redirectTargetUrl);
    firstResource->willFollowRedirect(redirectRequest, fresh302Response);

    // Add the final response to our request.
    ResourceResponse fresh200Response;
    fresh200Response.setURL(redirectTargetUrl);
    fresh200Response.setHTTPStatusCode(200);
    fresh200Response.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh200Response.setHTTPHeaderField(HTTPNames::Expires, kOneDayAfterOriginalRequest);

    firstResource->setResponse(fresh200Response);
    firstResource->finish();
    memoryCache()->add(firstResource);

    advanceClock(500.);

    Resource* fetched = fetch();
    EXPECT_NE(firstResource, fetched);
}

TEST_F(CachingCorrectnessTest, 302RedirectExplicitlyFreshMaxAge)
{
    KURL redirectUrl(ParsedURLString, kResourceURL);
    const char redirectTargetUrlString[] = "http://redirect-target.com";
    KURL redirectTargetUrl(ParsedURLString, redirectTargetUrlString);

    Resource* firstResource = Resource::create(ResourceRequest(redirectUrl), Resource::Raw);

    ResourceResponse fresh302Response;
    fresh302Response.setURL(redirectUrl);
    fresh302Response.setHTTPStatusCode(302);
    fresh302Response.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh302Response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=600");
    fresh302Response.setHTTPHeaderField(HTTPNames::Location, redirectTargetUrlString);

    // Add the redirect to our request.
    ResourceRequest redirectRequest = ResourceRequest(redirectTargetUrl);
    firstResource->willFollowRedirect(redirectRequest, fresh302Response);

    // Add the final response to our request.
    ResourceResponse fresh200Response;
    fresh200Response.setURL(redirectTargetUrl);
    fresh200Response.setHTTPStatusCode(200);
    fresh200Response.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh200Response.setHTTPHeaderField(HTTPNames::Expires, kOneDayAfterOriginalRequest);

    firstResource->setResponse(fresh200Response);
    firstResource->finish();
    memoryCache()->add(firstResource);

    advanceClock(500.);

    Resource* fetched = fetch();
    EXPECT_EQ(firstResource, fetched);
}

TEST_F(CachingCorrectnessTest, 302RedirectExplicitlyFreshExpires)
{
    KURL redirectUrl(ParsedURLString, kResourceURL);
    const char redirectTargetUrlString[] = "http://redirect-target.com";
    KURL redirectTargetUrl(ParsedURLString, redirectTargetUrlString);

    Resource* firstResource = Resource::create(ResourceRequest(redirectUrl), Resource::Raw);

    ResourceResponse fresh302Response;
    fresh302Response.setURL(redirectUrl);
    fresh302Response.setHTTPStatusCode(302);
    fresh302Response.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh302Response.setHTTPHeaderField(HTTPNames::Expires, kOneDayAfterOriginalRequest);
    fresh302Response.setHTTPHeaderField(HTTPNames::Location, redirectTargetUrlString);

    // Add the redirect to our request.
    ResourceRequest redirectRequest = ResourceRequest(redirectTargetUrl);
    firstResource->willFollowRedirect(redirectRequest, fresh302Response);

    // Add the final response to our request.
    ResourceResponse fresh200Response;
    fresh200Response.setURL(redirectTargetUrl);
    fresh200Response.setHTTPStatusCode(200);
    fresh200Response.setHTTPHeaderField(HTTPNames::Date, kOriginalRequestDateAsString);
    fresh200Response.setHTTPHeaderField(HTTPNames::Expires, kOneDayAfterOriginalRequest);

    firstResource->setResponse(fresh200Response);
    firstResource->finish();
    memoryCache()->add(firstResource);

    advanceClock(500.);

    Resource* fetched = fetch();
    EXPECT_EQ(firstResource, fetched);
}

} // namespace blink
