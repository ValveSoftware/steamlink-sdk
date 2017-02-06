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
    class FakeDecodedResource : public Resource {
    public:
        static FakeDecodedResource* create(const ResourceRequest& request, Type type)
        {
            return new FakeDecodedResource(request, type, ResourceLoaderOptions());
        }

        virtual void appendData(const char* data, size_t len)
        {
            Resource::appendData(data, len);
            setDecodedSize(this->size());
        }

    protected:
        FakeDecodedResource(const ResourceRequest& request, Type type, const ResourceLoaderOptions& options)
            : Resource(request, type, options)
        {
        }

        void destroyDecodedDataIfPossible() override
        {
            setDecodedSize(0);
        }
    };

    class FakeResource : public Resource {
    public:
        static FakeResource* create(const ResourceRequest& request, Type type)
        {
            return new FakeResource(request, type, ResourceLoaderOptions());
        }

        void fakeEncodedSize(size_t size)
        {
            setEncodedSize(size);
        }

    private:
        FakeResource(const ResourceRequest& request, Type type, const ResourceLoaderOptions& options)
            : Resource(request, type, options)
        {
        }
    };

protected:
    virtual void SetUp()
    {
        // Save the global memory cache to restore it upon teardown.
        m_globalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());
    }

    virtual void TearDown()
    {
        replaceMemoryCacheForTesting(m_globalMemoryCache.release());
    }

    Persistent<MemoryCache> m_globalMemoryCache;
};

// Verifies that setters and getters for cache capacities work correcty.
TEST_F(MemoryCacheTest, CapacityAccounting)
{
    const size_t sizeMax = ~static_cast<size_t>(0);
    const size_t totalCapacity = sizeMax / 4;
    const size_t minDeadCapacity = sizeMax / 16;
    const size_t maxDeadCapacity = sizeMax / 8;
    memoryCache()->setCapacities(minDeadCapacity, maxDeadCapacity, totalCapacity);
    ASSERT_EQ(totalCapacity, memoryCache()->capacity());
    ASSERT_EQ(minDeadCapacity, memoryCache()->minDeadCapacity());
    ASSERT_EQ(maxDeadCapacity, memoryCache()->maxDeadCapacity());
}

TEST_F(MemoryCacheTest, VeryLargeResourceAccounting)
{
    const size_t sizeMax = ~static_cast<size_t>(0);
    const size_t totalCapacity = sizeMax / 4;
    const size_t minDeadCapacity = sizeMax / 16;
    const size_t maxDeadCapacity = sizeMax / 8;
    const size_t resourceSize1 = sizeMax / 16;
    const size_t resourceSize2 = sizeMax / 20;
    memoryCache()->setCapacities(minDeadCapacity, maxDeadCapacity, totalCapacity);
    FakeResource* cachedResource =
        FakeResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
    cachedResource->fakeEncodedSize(resourceSize1);

    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());
    memoryCache()->add(cachedResource);
    ASSERT_EQ(cachedResource->size(), memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());

    Persistent<MockResourceClient> client = new MockResourceClient(cachedResource);
    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(cachedResource->size(), memoryCache()->liveSize());

    cachedResource->fakeEncodedSize(resourceSize2);
    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(cachedResource->size(), memoryCache()->liveSize());
}

// Verifies that dead resources that exceed dead resource capacity are evicted
// from cache when pruning.
static void TestDeadResourceEviction(Resource* resource1, Resource* resource2)
{
    memoryCache()->setDelayBeforeLiveDecodedPrune(0);
    memoryCache()->setMaxPruneDeferralDelay(0);
    const unsigned totalCapacity = 1000000;
    const unsigned minDeadCapacity = 0;
    const unsigned maxDeadCapacity = 0;
    memoryCache()->setCapacities(minDeadCapacity, maxDeadCapacity, totalCapacity);

    const char data[5] = "abcd";
    resource1->appendData(data, 3u);
    resource2->appendData(data, 2u);

    // The resource size has to be nonzero for this test to be meaningful, but
    // we do not rely on it having any particular value.
    ASSERT_GT(resource1->size(), 0u);
    ASSERT_GT(resource2->size(), 0u);

    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());

    memoryCache()->add(resource1);
    ASSERT_EQ(resource1->size(), memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());

    memoryCache()->add(resource2);
    ASSERT_EQ(resource1->size() + resource2->size(), memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());

    memoryCache()->prune();
    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());
}

TEST_F(MemoryCacheTest, DeadResourceEviction_Basic)
{
    Resource* resource1 =
        Resource::create(ResourceRequest("http://test/resource1"), Resource::Raw);
    Resource* resource2 =
        Resource::create(ResourceRequest("http://test/resource2"), Resource::Raw);
    TestDeadResourceEviction(resource1, resource2);
}

TEST_F(MemoryCacheTest, DeadResourceEviction_MultipleResourceMaps)
{
    Resource* resource1 =
        Resource::create(ResourceRequest("http://test/resource1"), Resource::Raw);
    Resource* resource2 =
        Resource::create(ResourceRequest("http://test/resource2"), Resource::Raw);
    resource2->setCacheIdentifier("foo");
    TestDeadResourceEviction(resource1, resource2);
}

static void runTask1(Resource* live, Resource* dead)
{
    // The resource size has to be nonzero for this test to be meaningful, but
    // we do not rely on it having any particular value.
    ASSERT_GT(live->size(), 0u);
    ASSERT_GT(dead->size(), 0u);

    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());

    memoryCache()->add(dead);
    memoryCache()->add(live);
    memoryCache()->updateDecodedResource(live, UpdateForPropertyChange);
    ASSERT_EQ(dead->size(), memoryCache()->deadSize());
    ASSERT_EQ(live->size(), memoryCache()->liveSize());
    ASSERT_GT(live->decodedSize(), 0u);

    memoryCache()->prune(); // Dead resources are pruned immediately
    ASSERT_EQ(dead->size(), memoryCache()->deadSize());
    ASSERT_EQ(live->size(), memoryCache()->liveSize());
    ASSERT_GT(live->decodedSize(), 0u);
}

static void runTask2(unsigned liveSizeWithoutDecode)
{
    // Next task: now, the live resource was evicted.
    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(liveSizeWithoutDecode, memoryCache()->liveSize());
}

static void TestLiveResourceEvictionAtEndOfTask(Resource* cachedDeadResource, Resource* cachedLiveResource)
{
    memoryCache()->setDelayBeforeLiveDecodedPrune(0);
    const unsigned totalCapacity = 1;
    const unsigned minDeadCapacity = 0;
    const unsigned maxDeadCapacity = 0;
    memoryCache()->setCapacities(minDeadCapacity, maxDeadCapacity, totalCapacity);
    const char data[6] = "abcde";
    cachedDeadResource->appendData(data, 3u);
    cachedDeadResource->finish();
    Persistent<MockResourceClient> client = new MockResourceClient(cachedLiveResource);
    cachedLiveResource->appendData(data, 4u);
    cachedLiveResource->finish();

    Platform::current()->currentThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&runTask1, wrapPersistent(cachedLiveResource), wrapPersistent(cachedDeadResource)));
    Platform::current()->currentThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&runTask2, cachedLiveResource->encodedSize() + cachedLiveResource->overheadSize()));
    testing::runPendingTasks();
}

// Verified that when ordering a prune in a runLoop task, the prune
// is deferred to the end of the task.
TEST_F(MemoryCacheTest, LiveResourceEvictionAtEndOfTask_Basic)
{
    Resource* cachedDeadResource =
        Resource::create(ResourceRequest("hhtp://foo"), Resource::Raw);
    Resource* cachedLiveResource =
        FakeDecodedResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
    TestLiveResourceEvictionAtEndOfTask(cachedDeadResource, cachedLiveResource);
}

TEST_F(MemoryCacheTest, LiveResourceEvictionAtEndOfTask_MultipleResourceMaps)
{
    {
        Resource* cachedDeadResource =
            Resource::create(ResourceRequest("hhtp://foo"), Resource::Raw);
        cachedDeadResource->setCacheIdentifier("foo");
        Resource* cachedLiveResource =
            FakeDecodedResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
        TestLiveResourceEvictionAtEndOfTask(cachedDeadResource, cachedLiveResource);
        memoryCache()->evictResources();
    }
    {
        Resource* cachedDeadResource =
            Resource::create(ResourceRequest("hhtp://foo"), Resource::Raw);
        Resource* cachedLiveResource =
            FakeDecodedResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
        cachedLiveResource->setCacheIdentifier("foo");
        TestLiveResourceEvictionAtEndOfTask(cachedDeadResource, cachedLiveResource);
        memoryCache()->evictResources();
    }
    {
        Resource* cachedDeadResource =
            Resource::create(ResourceRequest("hhtp://test/resource"), Resource::Raw);
        cachedDeadResource->setCacheIdentifier("foo");
        Resource* cachedLiveResource =
            FakeDecodedResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
        cachedLiveResource->setCacheIdentifier("bar");
        TestLiveResourceEvictionAtEndOfTask(cachedDeadResource, cachedLiveResource);
        memoryCache()->evictResources();
    }
}

// Verifies that cached resources are evicted immediately after release when
// the total dead resource size is more than double the dead resource capacity.
static void TestClientRemoval(Resource* resource1, Resource* resource2)
{
    const char data[6] = "abcde";
    Persistent<MockResourceClient> client1 = new MockResourceClient(resource1);
    resource1->appendData(data, 4u);
    Persistent<MockResourceClient> client2 = new MockResourceClient(resource2);
    resource2->appendData(data, 4u);

    const unsigned minDeadCapacity = 0;
    const unsigned maxDeadCapacity = ((resource1->size() + resource2->size()) / 2) - 1;
    const unsigned totalCapacity = maxDeadCapacity;
    memoryCache()->setCapacities(minDeadCapacity, maxDeadCapacity, totalCapacity);
    memoryCache()->add(resource1);
    memoryCache()->add(resource2);
    // Call prune. There is nothing to prune, but this will initialize
    // the prune timestamp, allowing future prunes to be deferred.
    memoryCache()->prune();
    ASSERT_GT(resource1->decodedSize(), 0u);
    ASSERT_GT(resource2->decodedSize(), 0u);
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), resource1->size() + resource2->size());

    // Removing the client from resource1 should result in all resources
    // remaining in cache since the prune is deferred.
    client1->removeAsClient();
    ASSERT_GT(resource1->decodedSize(), 0u);
    ASSERT_GT(resource2->decodedSize(), 0u);
    ASSERT_EQ(memoryCache()->deadSize(), resource1->size());
    ASSERT_EQ(memoryCache()->liveSize(), resource2->size());
    ASSERT_TRUE(memoryCache()->contains(resource1));
    ASSERT_TRUE(memoryCache()->contains(resource2));

    // Removing the client from resource2 should result in immediate
    // eviction of resource2 because we are over the prune deferral limit.
    client2->removeAsClient();
    ASSERT_GT(resource1->decodedSize(), 0u);
    ASSERT_GT(resource2->decodedSize(), 0u);
    ASSERT_EQ(memoryCache()->deadSize(), resource1->size());
    ASSERT_EQ(memoryCache()->liveSize(), 0u);
    ASSERT_TRUE(memoryCache()->contains(resource1));
    ASSERT_FALSE(memoryCache()->contains(resource2));
}

TEST_F(MemoryCacheTest, ClientRemoval_Basic)
{
    Resource* resource1 =
        FakeDecodedResource::create(ResourceRequest("http://foo.com"), Resource::Raw);
    Resource* resource2 =
        FakeDecodedResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
    TestClientRemoval(resource1, resource2);
}

TEST_F(MemoryCacheTest, ClientRemoval_MultipleResourceMaps)
{
    {
        Resource* resource1 =
            FakeDecodedResource::create(ResourceRequest("http://foo.com"), Resource::Raw);
        resource1->setCacheIdentifier("foo");
        Resource* resource2 =
            FakeDecodedResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
        TestClientRemoval(resource1, resource2);
        memoryCache()->evictResources();
    }
    {
        Resource* resource1 =
            FakeDecodedResource::create(ResourceRequest("http://foo.com"), Resource::Raw);
        Resource* resource2 =
            FakeDecodedResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
        resource2->setCacheIdentifier("foo");
        TestClientRemoval(resource1, resource2);
        memoryCache()->evictResources();
    }
    {
        Resource* resource1 =
            FakeDecodedResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
        resource1->setCacheIdentifier("foo");
        Resource* resource2 =
            FakeDecodedResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
        resource2->setCacheIdentifier("bar");
        TestClientRemoval(resource1, resource2);
        memoryCache()->evictResources();
    }
}

TEST_F(MemoryCacheTest, RemoveDuringRevalidation)
{
    FakeResource* resource1 = FakeResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
    memoryCache()->add(resource1);

    FakeResource* resource2 = FakeResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
    memoryCache()->remove(resource1);
    memoryCache()->add(resource2);
    EXPECT_TRUE(memoryCache()->contains(resource2));
    EXPECT_FALSE(memoryCache()->contains(resource1));

    FakeResource* resource3 = FakeResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
    memoryCache()->remove(resource2);
    memoryCache()->add(resource3);
    EXPECT_TRUE(memoryCache()->contains(resource3));
    EXPECT_FALSE(memoryCache()->contains(resource2));
}

TEST_F(MemoryCacheTest, ResourceMapIsolation)
{
    FakeResource* resource1 = FakeResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
    memoryCache()->add(resource1);

    FakeResource* resource2 = FakeResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
    resource2->setCacheIdentifier("foo");
    memoryCache()->add(resource2);
    EXPECT_TRUE(memoryCache()->contains(resource1));
    EXPECT_TRUE(memoryCache()->contains(resource2));

    const KURL url = KURL(ParsedURLString, "http://test/resource");
    EXPECT_EQ(resource1, memoryCache()->resourceForURL(url));
    EXPECT_EQ(resource1, memoryCache()->resourceForURL(url, memoryCache()->defaultCacheIdentifier()));
    EXPECT_EQ(resource2, memoryCache()->resourceForURL(url, "foo"));
    EXPECT_EQ(0, memoryCache()->resourceForURL(KURL()));

    FakeResource* resource3 = FakeResource::create(ResourceRequest("http://test/resource"), Resource::Raw);
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

} // namespace blink
