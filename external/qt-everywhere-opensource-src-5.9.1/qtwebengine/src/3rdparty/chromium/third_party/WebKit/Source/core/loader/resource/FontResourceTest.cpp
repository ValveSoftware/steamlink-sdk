// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/FontResource.h"

#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/MockFetchContext.h"
#include "core/fetch/MockResourceClients.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FontResourceTest : public ::testing::Test {};

// Tests if ResourceFetcher works fine with FontResource that requires defered
// loading supports.
TEST_F(FontResourceTest,
       ResourceFetcherRevalidateDeferedResourceFromTwoInitiators) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/font.woff");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::ETag, "1234567890");
  Platform::current()->getURLLoaderMockFactory()->registerURL(
      url, WrappedResourceResponse(response), "");

  MockFetchContext* context =
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource);
  ResourceFetcher* fetcher = ResourceFetcher::create(context);

  // Fetch to cache a resource.
  ResourceRequest request1(url);
  FetchRequest fetchRequest1 = FetchRequest(request1, FetchInitiatorInfo());
  Resource* resource1 = FontResource::fetch(fetchRequest1, fetcher);
  ASSERT_TRUE(resource1);
  fetcher->startLoad(resource1);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  EXPECT_TRUE(resource1->isLoaded());
  EXPECT_FALSE(resource1->errorOccurred());

  // Set the context as it is on reloads.
  context->setLoadComplete(true);
  context->setCachePolicy(CachePolicyRevalidate);

  // Revalidate the resource.
  ResourceRequest request2(url);
  FetchRequest fetchRequest2 = FetchRequest(request2, FetchInitiatorInfo());
  Resource* resource2 = FontResource::fetch(fetchRequest2, fetcher);
  ASSERT_TRUE(resource2);
  EXPECT_EQ(resource1, resource2);
  EXPECT_TRUE(resource2->isCacheValidator());
  EXPECT_TRUE(resource2->stillNeedsLoad());

  // Fetch the same resource again before actual load operation starts.
  ResourceRequest request3(url);
  FetchRequest fetchRequest3 = FetchRequest(request3, FetchInitiatorInfo());
  Resource* resource3 = FontResource::fetch(fetchRequest3, fetcher);
  ASSERT_TRUE(resource3);
  EXPECT_EQ(resource2, resource3);
  EXPECT_TRUE(resource3->isCacheValidator());
  EXPECT_TRUE(resource3->stillNeedsLoad());

  // startLoad() can be called from any initiator. Here, call it from the
  // latter.
  fetcher->startLoad(resource3);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  EXPECT_TRUE(resource3->isLoaded());
  EXPECT_FALSE(resource3->errorOccurred());
  EXPECT_TRUE(resource2->isLoaded());
  EXPECT_FALSE(resource2->errorOccurred());

  memoryCache()->remove(resource1);
}

}  // namespace blink
