/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "core/fetch/MemoryCache.h"

#include "core/fetch/MockResourceClients.h"
#include "core/fetch/RawResource.h"
#include "platform/network/ResourceRequest.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MemoryCacheTest : public ::testing::Test {
 public:
  class FakeDecodedResource final : public Resource {
   public:
    static FakeDecodedResource* create(const ResourceRequest& request,
                                       Type type) {
      return new FakeDecodedResource(request, type, ResourceLoaderOptions());
    }

    virtual void appendData(const char* data, size_t len) {
      Resource::appendData(data, len);
      setDecodedSize(this->size());
    }

   private:
    FakeDecodedResource(const ResourceRequest& request,
                        Type type,
                        const ResourceLoaderOptions& options)
        : Resource(request, type, options) {}

    void destroyDecodedDataIfPossible() override { setDecodedSize(0); }
  };

  class FakeResource final : public Resource {
   public:
    static FakeResource* create(const ResourceRequest& request, Type type) {
      return new FakeResource(request, type, ResourceLoaderOptions());
    }

    void fakeEncodedSize(size_t size) { setEncodedSize(size); }

   private:
    FakeResource(const ResourceRequest& request,
                 Type type,
                 const ResourceLoaderOptions& options)
        : Resource(request, type, options) {}
  };

 protected:
  virtual void SetUp() {
    // Save the global memory cache to restore it upon teardown.
    m_globalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());
  }

  virtual void TearDown() {
    replaceMemoryCacheForTesting(m_globalMemoryCache.release());
  }

  Persistent<MemoryCache> m_globalMemoryCache;
};

// Verifies that setters and getters for cache capacities work correcty.
TEST_F(MemoryCacheTest, CapacityAccounting) {
  const size_t sizeMax = ~static_cast<size_t>(0);
  const size_t totalCapacity = sizeMax / 4;
  memoryCache()->setCapacity(totalCapacity);
  EXPECT_EQ(totalCapacity, memoryCache()->capacity());
}

TEST_F(MemoryCacheTest, VeryLargeResourceAccounting) {
  const size_t sizeMax = ~static_cast<size_t>(0);
  const size_t totalCapacity = sizeMax / 4;
  const size_t resourceSize1 = sizeMax / 16;
  const size_t resourceSize2 = sizeMax / 20;
  memoryCache()->setCapacity(totalCapacity);
  FakeResource* cachedResource = FakeResource::create(
      ResourceRequest("http://test/resource"), Resource::Raw);
  cachedResource->fakeEncodedSize(resourceSize1);

  EXPECT_EQ(0u, memoryCache()->size());
  memoryCache()->add(cachedResource);
  EXPECT_EQ(cachedResource->size(), memoryCache()->size());

  Persistent<MockResourceClient> client =
      new MockResourceClient(cachedResource);
  EXPECT_EQ(cachedResource->size(), memoryCache()->size());

  cachedResource->fakeEncodedSize(resourceSize2);
  EXPECT_EQ(cachedResource->size(), memoryCache()->size());
}

static void runTask1(Resource* resource1, Resource* resource2) {
  // The resource size has to be nonzero for this test to be meaningful, but
  // we do not rely on it having any particular value.
  EXPECT_GT(resource1->size(), 0u);
  EXPECT_GT(resource2->size(), 0u);

  EXPECT_EQ(0u, memoryCache()->size());

  memoryCache()->add(resource1);
  memoryCache()->add(resource2);

  size_t totalSize = resource1->size() + resource2->size();
  EXPECT_EQ(totalSize, memoryCache()->size());
  EXPECT_GT(resource1->decodedSize(), 0u);
  EXPECT_GT(resource2->decodedSize(), 0u);

  // We expect actual pruning doesn't occur here synchronously but deferred
  // to the end of this task, due to the previous pruning invoked in
  // testResourcePruningAtEndOfTask().
  memoryCache()->prune();
  EXPECT_EQ(totalSize, memoryCache()->size());
  EXPECT_GT(resource1->decodedSize(), 0u);
  EXPECT_GT(resource2->decodedSize(), 0u);
}

static void runTask2(unsigned sizeWithoutDecode) {
  // Next task: now, the resources was pruned.
  EXPECT_EQ(sizeWithoutDecode, memoryCache()->size());
}

static void testResourcePruningAtEndOfTask(Resource* resource1,
                                           Resource* resource2) {
  memoryCache()->setDelayBeforeLiveDecodedPrune(0);

  // Enforce pruning by adding |dummyResource| and then call prune().
  Resource* dummyResource =
      RawResource::create(ResourceRequest("http://dummy"), Resource::Raw);
  memoryCache()->add(dummyResource);
  EXPECT_GT(memoryCache()->size(), 1u);
  const unsigned totalCapacity = 1;
  memoryCache()->setCapacity(totalCapacity);
  memoryCache()->prune();
  memoryCache()->remove(dummyResource);
  EXPECT_EQ(0u, memoryCache()->size());

  const char data[6] = "abcde";
  resource1->appendData(data, 3u);
  resource1->finish();
  Persistent<MockResourceClient> client = new MockResourceClient(resource2);
  resource2->appendData(data, 4u);
  resource2->finish();

  Platform::current()->currentThread()->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, WTF::bind(&runTask1, wrapPersistent(resource1),
                                 wrapPersistent(resource2)));
  Platform::current()->currentThread()->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE,
      WTF::bind(&runTask2,
                resource1->encodedSize() + resource1->overheadSize() +
                    resource2->encodedSize() + resource2->overheadSize()));
  testing::runPendingTasks();
}

// Verified that when ordering a prune in a runLoop task, the prune
// is deferred to the end of the task.
TEST_F(MemoryCacheTest, ResourcePruningAtEndOfTask_Basic) {
  Resource* resource1 = FakeDecodedResource::create(
      ResourceRequest("http://test/resource1"), Resource::Raw);
  Resource* resource2 = FakeDecodedResource::create(
      ResourceRequest("http://test/resource2"), Resource::Raw);
  testResourcePruningAtEndOfTask(resource1, resource2);
}

TEST_F(MemoryCacheTest, ResourcePruningAtEndOfTask_MultipleResourceMaps) {
  {
    Resource* resource1 = FakeDecodedResource::create(
        ResourceRequest("http://test/resource1"), Resource::Raw);
    Resource* resource2 = FakeDecodedResource::create(
        ResourceRequest("http://test/resource2"), Resource::Raw);
    resource1->setCacheIdentifier("foo");
    testResourcePruningAtEndOfTask(resource1, resource2);
    memoryCache()->evictResources();
  }
  {
    Resource* resource1 = FakeDecodedResource::create(
        ResourceRequest("http://test/resource1"), Resource::Raw);
    Resource* resource2 = FakeDecodedResource::create(
        ResourceRequest("http://test/resource2"), Resource::Raw);
    resource1->setCacheIdentifier("foo");
    resource2->setCacheIdentifier("bar");
    testResourcePruningAtEndOfTask(resource1, resource2);
    memoryCache()->evictResources();
  }
}

// Verifies that
// - Resources are not pruned synchronously when ResourceClient is removed.
// - size() is updated appropriately when Resources are added to MemoryCache
//   and garbage collected.
static void testClientRemoval(Resource* resource1, Resource* resource2) {
  const char data[6] = "abcde";
  Persistent<MockResourceClient> client1 = new MockResourceClient(resource1);
  resource1->appendData(data, 4u);
  Persistent<MockResourceClient> client2 = new MockResourceClient(resource2);
  resource2->appendData(data, 4u);

  memoryCache()->setCapacity(0);
  memoryCache()->add(resource1);
  memoryCache()->add(resource2);

  size_t originalTotalSize = resource1->size() + resource2->size();

  // Call prune. There is nothing to prune, but this will initialize
  // the prune timestamp, allowing future prunes to be deferred.
  memoryCache()->prune();
  EXPECT_GT(resource1->decodedSize(), 0u);
  EXPECT_GT(resource2->decodedSize(), 0u);
  EXPECT_EQ(originalTotalSize, memoryCache()->size());

  // Removing the client from resource1 should not trigger pruning.
  client1->removeAsClient();
  EXPECT_GT(resource1->decodedSize(), 0u);
  EXPECT_GT(resource2->decodedSize(), 0u);
  EXPECT_EQ(originalTotalSize, memoryCache()->size());
  EXPECT_TRUE(memoryCache()->contains(resource1));
  EXPECT_TRUE(memoryCache()->contains(resource2));

  // Removing the client from resource2 should not trigger pruning.
  client2->removeAsClient();
  EXPECT_GT(resource1->decodedSize(), 0u);
  EXPECT_GT(resource2->decodedSize(), 0u);
  EXPECT_EQ(originalTotalSize, memoryCache()->size());
  EXPECT_TRUE(memoryCache()->contains(resource1));
  EXPECT_TRUE(memoryCache()->contains(resource2));

  WeakPersistent<Resource> resource1Weak = resource1;
  WeakPersistent<Resource> resource2Weak = resource2;

  ThreadState::current()->collectGarbage(
      BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);
  // Resources are garbage-collected (WeakMemoryCache) and thus removed
  // from MemoryCache.
  EXPECT_FALSE(resource1Weak);
  EXPECT_FALSE(resource2Weak);
  EXPECT_EQ(0u, memoryCache()->size());
}

TEST_F(MemoryCacheTest, ClientRemoval_Basic) {
  Resource* resource1 = FakeDecodedResource::create(
      ResourceRequest("http://foo.com"), Resource::Raw);
  Resource* resource2 = FakeDecodedResource::create(
      ResourceRequest("http://test/resource"), Resource::Raw);
  testClientRemoval(resource1, resource2);
}

TEST_F(MemoryCacheTest, ClientRemoval_MultipleResourceMaps) {
  {
    Resource* resource1 = FakeDecodedResource::create(
        ResourceRequest("http://foo.com"), Resource::Raw);
    resource1->setCacheIdentifier("foo");
    Resource* resource2 = FakeDecodedResource::create(
        ResourceRequest("http://test/resource"), Resource::Raw);
    testClientRemoval(resource1, resource2);
    memoryCache()->evictResources();
  }
  {
    Resource* resource1 = FakeDecodedResource::create(
        ResourceRequest("http://foo.com"), Resource::Raw);
    Resource* resource2 = FakeDecodedResource::create(
        ResourceRequest("http://test/resource"), Resource::Raw);
    resource2->setCacheIdentifier("foo");
    testClientRemoval(resource1, resource2);
    memoryCache()->evictResources();
  }
  {
    Resource* resource1 = FakeDecodedResource::create(
        ResourceRequest("http://test/resource"), Resource::Raw);
    resource1->setCacheIdentifier("foo");
    Resource* resource2 = FakeDecodedResource::create(
        ResourceRequest("http://test/resource"), Resource::Raw);
    resource2->setCacheIdentifier("bar");
    testClientRemoval(resource1, resource2);
    memoryCache()->evictResources();
  }
}

TEST_F(MemoryCacheTest, RemoveDuringRevalidation) {
  FakeResource* resource1 = FakeResource::create(
      ResourceRequest("http://test/resource"), Resource::Raw);
  memoryCache()->add(resource1);

  FakeResource* resource2 = FakeResource::create(
      ResourceRequest("http://test/resource"), Resource::Raw);
  memoryCache()->remove(resource1);
  memoryCache()->add(resource2);
  EXPECT_TRUE(memoryCache()->contains(resource2));
  EXPECT_FALSE(memoryCache()->contains(resource1));

  FakeResource* resource3 = FakeResource::create(
      ResourceRequest("http://test/resource"), Resource::Raw);
  memoryCache()->remove(resource2);
  memoryCache()->add(resource3);
  EXPECT_TRUE(memoryCache()->contains(resource3));
  EXPECT_FALSE(memoryCache()->contains(resource2));
}

TEST_F(MemoryCacheTest, ResourceMapIsolation) {
  FakeResource* resource1 = FakeResource::create(
      ResourceRequest("http://test/resource"), Resource::Raw);
  memoryCache()->add(resource1);

  FakeResource* resource2 = FakeResource::create(
      ResourceRequest("http://test/resource"), Resource::Raw);
  resource2->setCacheIdentifier("foo");
  memoryCache()->add(resource2);
  EXPECT_TRUE(memoryCache()->contains(resource1));
  EXPECT_TRUE(memoryCache()->contains(resource2));

  const KURL url = KURL(ParsedURLString, "http://test/resource");
  EXPECT_EQ(resource1, memoryCache()->resourceForURL(url));
  EXPECT_EQ(resource1, memoryCache()->resourceForURL(
                           url, memoryCache()->defaultCacheIdentifier()));
  EXPECT_EQ(resource2, memoryCache()->resourceForURL(url, "foo"));
  EXPECT_EQ(0, memoryCache()->resourceForURL(KURL()));

  FakeResource* resource3 = FakeResource::create(
      ResourceRequest("http://test/resource"), Resource::Raw);
  resource3->setCacheIdentifier("foo");
  memoryCache()->remove(resource2);
  memoryCache()->add(resource3);
  EXPECT_TRUE(memoryCache()->contains(resource1));
  EXPECT_FALSE(memoryCache()->contains(resource2));
  EXPECT_TRUE(memoryCache()->contains(resource3));

  HeapVector<Member<Resource>> resources = memoryCache()->resourcesForURL(url);
  EXPECT_EQ(2u, resources.size());

  memoryCache()->evictResources();
  EXPECT_FALSE(memoryCache()->contains(resource1));
  EXPECT_FALSE(memoryCache()->contains(resource3));
}

TEST_F(MemoryCacheTest, FragmentIdentifier) {
  const KURL url1 = KURL(ParsedURLString, "http://test/resource#foo");
  FakeResource* resource =
      FakeResource::create(ResourceRequest(url1), Resource::Raw);
  memoryCache()->add(resource);
  EXPECT_TRUE(memoryCache()->contains(resource));

  EXPECT_EQ(resource, memoryCache()->resourceForURL(url1));

  const KURL url2 = MemoryCache::removeFragmentIdentifierIfNeeded(url1);
  EXPECT_EQ(resource, memoryCache()->resourceForURL(url2));
}

TEST_F(MemoryCacheTest, RemoveURLFromCache) {
  const KURL url1 = KURL(ParsedURLString, "http://test/resource1");
  Persistent<FakeResource> resource1 =
      FakeResource::create(ResourceRequest(url1), Resource::Raw);
  memoryCache()->add(resource1);
  EXPECT_TRUE(memoryCache()->contains(resource1));

  memoryCache()->removeURLFromCache(url1);
  EXPECT_FALSE(memoryCache()->contains(resource1));

  const KURL url2 = KURL(ParsedURLString, "http://test/resource2#foo");
  FakeResource* resource2 =
      FakeResource::create(ResourceRequest(url2), Resource::Raw);
  memoryCache()->add(resource2);
  EXPECT_TRUE(memoryCache()->contains(resource2));

  memoryCache()->removeURLFromCache(url2);
  EXPECT_FALSE(memoryCache()->contains(resource2));
}

}  // namespace blink
