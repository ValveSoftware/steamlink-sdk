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

#include "config.h"
#include "core/fetch/MemoryCache.h"

#include "core/fetch/MockImageResourceClient.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/ResourcePtr.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/Platform.h"
#include "wtf/OwnPtr.h"

#include <gtest/gtest.h>

namespace WebCore {

class MemoryCacheTest : public ::testing::Test {
public:
    class FakeDecodedResource : public WebCore::Resource {
    public:
        FakeDecodedResource(const ResourceRequest& request, Type type)
            : Resource(request, type)
        {
        }

        virtual void appendData(const char* data, int len)
        {
            Resource::appendData(data, len);
            setDecodedSize(this->size());
        }

    protected:
        virtual void destroyDecodedDataIfPossible() OVERRIDE
        {
            setDecodedSize(0);
        }
    };

    class FakeResource : public WebCore::Resource {
    public:
        FakeResource(const ResourceRequest& request, Type type)
            : Resource(request, type)
        {
        }

        void fakeEncodedSize(size_t size)
        {
            setEncodedSize(size);
        }
    };

protected:
    virtual void SetUp()
    {
        // Save the global memory cache to restore it upon teardown.
        m_globalMemoryCache = adoptPtr(memoryCache());
        // Create the test memory cache instance and hook it in.
        m_testingMemoryCache = adoptPtr(new MemoryCache());
        setMemoryCacheForTesting(m_testingMemoryCache.leakPtr());
    }

    virtual void TearDown()
    {
        // Regain the ownership of testing memory cache, so that it will be
        // destroyed.
        m_testingMemoryCache = adoptPtr(memoryCache());
        // Yield the ownership of the global memory cache back.
        setMemoryCacheForTesting(m_globalMemoryCache.leakPtr());
    }

    OwnPtr<MemoryCache> m_testingMemoryCache;
    OwnPtr<MemoryCache> m_globalMemoryCache;
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
    ResourcePtr<FakeResource> cachedResource =
        new FakeResource(ResourceRequest(""), Resource::Raw);
    cachedResource->fakeEncodedSize(resourceSize1);

    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());
    memoryCache()->add(cachedResource.get());
    ASSERT_EQ(cachedResource->size(), memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());

    MockImageResourceClient client;
    cachedResource->addClient(&client);
    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(cachedResource->size(), memoryCache()->liveSize());

    cachedResource->fakeEncodedSize(resourceSize2);
    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(cachedResource->size(), memoryCache()->liveSize());
}

// Verifies that dead resources that exceed dead resource capacity are evicted
// from cache when pruning.
TEST_F(MemoryCacheTest, DeadResourceEviction)
{
    memoryCache()->setDelayBeforeLiveDecodedPrune(0);
    memoryCache()->setMaxPruneDeferralDelay(0);
    const unsigned totalCapacity = 1000000;
    const unsigned minDeadCapacity = 0;
    const unsigned maxDeadCapacity = 0;
    memoryCache()->setCapacities(minDeadCapacity, maxDeadCapacity, totalCapacity);

    Resource* cachedResource =
        new Resource(ResourceRequest(""), Resource::Raw);
    const char data[5] = "abcd";
    cachedResource->appendData(data, 3);
    // The resource size has to be nonzero for this test to be meaningful, but
    // we do not rely on it having any particular value.
    ASSERT_GT(cachedResource->size(), 0u);

    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());

    memoryCache()->add(cachedResource);
    ASSERT_EQ(cachedResource->size(), memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());

    memoryCache()->prune();
    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());
}

// Verified that when ordering a prune in a runLoop task, the prune
// is deferred to the end of the task.
TEST_F(MemoryCacheTest, LiveResourceEvictionAtEndOfTask)
{
    memoryCache()->setDelayBeforeLiveDecodedPrune(0);
    const unsigned totalCapacity = 1;
    const unsigned minDeadCapacity = 0;
    const unsigned maxDeadCapacity = 0;
    memoryCache()->setCapacities(minDeadCapacity, maxDeadCapacity, totalCapacity);
    const char data[6] = "abcde";
    Resource* cachedDeadResource =
        new Resource(ResourceRequest("hhtp://foo"), Resource::Raw);
    cachedDeadResource->appendData(data, 3);
    ResourcePtr<Resource> cachedLiveResource =
        new FakeDecodedResource(ResourceRequest(""), Resource::Raw);
    MockImageResourceClient client;
    cachedLiveResource->addClient(&client);
    cachedLiveResource->appendData(data, 4);

    class Task1 : public blink::WebThread::Task {
    public:
        Task1(const ResourcePtr<Resource>& live, Resource* dead)
            : m_live(live)
            , m_dead(dead)
        { }

        virtual void run() OVERRIDE
        {
            // The resource size has to be nonzero for this test to be meaningful, but
            // we do not rely on it having any particular value.
            ASSERT_GT(m_live->size(), 0u);
            ASSERT_GT(m_dead->size(), 0u);

            ASSERT_EQ(0u, memoryCache()->deadSize());
            ASSERT_EQ(0u, memoryCache()->liveSize());

            memoryCache()->add(m_dead);
            memoryCache()->add(m_live.get());
            memoryCache()->updateDecodedResource(m_live.get(), UpdateForPropertyChange);
            ASSERT_EQ(m_dead->size(), memoryCache()->deadSize());
            ASSERT_EQ(m_live->size(), memoryCache()->liveSize());
            ASSERT_GT(m_live->decodedSize(), 0u);

            memoryCache()->prune(); // Dead resources are pruned immediately
            ASSERT_EQ(m_dead->size(), memoryCache()->deadSize());
            ASSERT_EQ(m_live->size(), memoryCache()->liveSize());
            ASSERT_GT(m_live->decodedSize(), 0u);
        }

    private:
        ResourcePtr<Resource> m_live;
        Resource* m_dead;
    };

    class Task2 : public blink::WebThread::Task {
    public:
        Task2(unsigned liveSizeWithoutDecode)
            : m_liveSizeWithoutDecode(liveSizeWithoutDecode) { }

        virtual void run() OVERRIDE
        {
            // Next task: now, the live resource was evicted.
            ASSERT_EQ(0u, memoryCache()->deadSize());
            ASSERT_EQ(m_liveSizeWithoutDecode, memoryCache()->liveSize());
            blink::Platform::current()->currentThread()->exitRunLoop();
        }

    private:
        unsigned m_liveSizeWithoutDecode;
    };


    blink::Platform::current()->currentThread()->postTask(new Task1(cachedLiveResource, cachedDeadResource));
    blink::Platform::current()->currentThread()->postTask(new Task2(cachedLiveResource->encodedSize() + cachedLiveResource->overheadSize()));
    blink::Platform::current()->currentThread()->enterRunLoop();
    cachedLiveResource->removeClient(&client);
}

// Verifies that cached resources are evicted immediately after release when
// the total dead resource size is more than double the dead resource capacity.
TEST_F(MemoryCacheTest, ClientRemoval)
{
    const char data[6] = "abcde";
    ResourcePtr<Resource> resource1 =
        new FakeDecodedResource(ResourceRequest("http://foo.com"), Resource::Raw);
    MockImageResourceClient client1;
    resource1->addClient(&client1);
    resource1->appendData(data, 4);
    ResourcePtr<Resource> resource2 =
        new FakeDecodedResource(ResourceRequest(""), Resource::Raw);
    MockImageResourceClient client2;
    resource2->addClient(&client2);
    resource2->appendData(data, 4);

    const unsigned minDeadCapacity = 0;
    const unsigned maxDeadCapacity = ((resource1->size() + resource2->size()) / 2) - 1;
    const unsigned totalCapacity = maxDeadCapacity;
    memoryCache()->setCapacities(minDeadCapacity, maxDeadCapacity, totalCapacity);
    memoryCache()->add(resource1.get());
    memoryCache()->add(resource2.get());
    // Call prune. There is nothing to prune, but this will initialize
    // the prune timestamp, allowing future prunes to be deferred.
    memoryCache()->prune();
    ASSERT_GT(resource1->decodedSize(), 0u);
    ASSERT_GT(resource2->decodedSize(), 0u);
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), resource1->size() + resource2->size());

    // Removing the client from resource1 should result in all resources
    // remaining in cache since the prune is deferred.
    resource1->removeClient(&client1);
    ASSERT_GT(resource1->decodedSize(), 0u);
    ASSERT_GT(resource2->decodedSize(), 0u);
    ASSERT_EQ(memoryCache()->deadSize(), resource1->size());
    ASSERT_EQ(memoryCache()->liveSize(), resource2->size());
    ASSERT_TRUE(memoryCache()->contains(resource1.get()));
    ASSERT_TRUE(memoryCache()->contains(resource2.get()));

    // Removing the client from resource2 should result in immediate
    // eviction of resource2 because we are over the prune deferral limit.
    resource2->removeClient(&client2);
    ASSERT_GT(resource1->decodedSize(), 0u);
    ASSERT_GT(resource2->decodedSize(), 0u);
    ASSERT_EQ(memoryCache()->deadSize(), resource1->size());
    ASSERT_EQ(memoryCache()->liveSize(), 0u);
    ASSERT_TRUE(memoryCache()->contains(resource1.get()));
    ASSERT_FALSE(memoryCache()->contains(resource2.get()));
}

// Verifies that CachedResources are evicted from the decode cache
// according to their DecodeCachePriority.
TEST_F(MemoryCacheTest, DecodeCacheOrder)
{
    memoryCache()->setDelayBeforeLiveDecodedPrune(0);
    memoryCache()->setMaxPruneDeferralDelay(0);
    ResourcePtr<FakeDecodedResource> cachedImageLowPriority =
        new FakeDecodedResource(ResourceRequest("http://foo.com"), Resource::Raw);
    ResourcePtr<FakeDecodedResource> cachedImageHighPriority =
        new FakeDecodedResource(ResourceRequest(""), Resource::Raw);

    MockImageResourceClient clientLowPriority;
    MockImageResourceClient clientHighPriority;
    cachedImageLowPriority->addClient(&clientLowPriority);
    cachedImageHighPriority->addClient(&clientHighPriority);

    const char data[5] = "abcd";
    cachedImageLowPriority->appendData(data, 1);
    cachedImageHighPriority->appendData(data, 4);
    const unsigned lowPrioritySize = cachedImageLowPriority->size();
    const unsigned highPrioritySize = cachedImageHighPriority->size();
    const unsigned lowPriorityMockDecodeSize = cachedImageLowPriority->decodedSize();
    const unsigned highPriorityMockDecodeSize = cachedImageHighPriority->decodedSize();
    const unsigned totalSize = lowPrioritySize + highPrioritySize;

    // Verify that the sizes are different to ensure that we can test eviction order.
    ASSERT_GT(lowPrioritySize, 0u);
    ASSERT_NE(lowPrioritySize, highPrioritySize);
    ASSERT_GT(lowPriorityMockDecodeSize, 0u);
    ASSERT_NE(lowPriorityMockDecodeSize, highPriorityMockDecodeSize);

    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), 0u);

    // Add the items. The item added first would normally be evicted first.
    memoryCache()->add(cachedImageHighPriority.get());
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), highPrioritySize);

    memoryCache()->add(cachedImageLowPriority.get());
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), highPrioritySize + lowPrioritySize);

    // Insert all items in the decoded items list with the same priority
    memoryCache()->updateDecodedResource(cachedImageHighPriority.get(), UpdateForPropertyChange);
    memoryCache()->updateDecodedResource(cachedImageLowPriority.get(), UpdateForPropertyChange);
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), totalSize);

    // Now we will assign their priority and make sure they are moved to the correct buckets.
    memoryCache()->updateDecodedResource(cachedImageLowPriority.get(), UpdateForPropertyChange, MemoryCacheLiveResourcePriorityLow);
    memoryCache()->updateDecodedResource(cachedImageHighPriority.get(), UpdateForPropertyChange, MemoryCacheLiveResourcePriorityHigh);

    // Should first prune the LowPriority item.
    memoryCache()->setCapacities(memoryCache()->minDeadCapacity(), memoryCache()->liveSize() - 10, memoryCache()->liveSize() - 10);
    memoryCache()->prune();
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), totalSize - lowPriorityMockDecodeSize);

    // Should prune the HighPriority item.
    memoryCache()->setCapacities(memoryCache()->minDeadCapacity(), memoryCache()->liveSize() - 10, memoryCache()->liveSize() - 10);
    memoryCache()->prune();
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), totalSize - lowPriorityMockDecodeSize - highPriorityMockDecodeSize);
}

TEST_F(MemoryCacheTest, MultipleReplace)
{
    ResourcePtr<FakeResource> resource1 = new FakeResource(ResourceRequest(""), Resource::Raw);
    memoryCache()->add(resource1.get());

    ResourcePtr<FakeResource> resource2 = new FakeResource(ResourceRequest(""), Resource::Raw);
    memoryCache()->replace(resource2.get(), resource1.get());
    EXPECT_TRUE(memoryCache()->contains(resource2.get()));
    EXPECT_FALSE(memoryCache()->contains(resource1.get()));

    ResourcePtr<FakeResource> resource3 = new FakeResource(ResourceRequest(""), Resource::Raw);
    memoryCache()->replace(resource3.get(), resource2.get());
    EXPECT_TRUE(memoryCache()->contains(resource3.get()));
    EXPECT_FALSE(memoryCache()->contains(resource2.get()));
}

TEST_F(MemoryCacheTest, RemoveDuringRevalidation)
{
    ResourcePtr<FakeResource> resource1 = new FakeResource(ResourceRequest(""), Resource::Raw);
    memoryCache()->add(resource1.get());

    ResourcePtr<FakeResource> resource2 = new FakeResource(ResourceRequest(""), Resource::Raw);
    memoryCache()->remove(resource1.get());
    memoryCache()->add(resource2.get());
    EXPECT_TRUE(memoryCache()->contains(resource2.get()));
    EXPECT_FALSE(memoryCache()->contains(resource1.get()));

    ResourcePtr<FakeResource> resource3 = new FakeResource(ResourceRequest(""), Resource::Raw);
    memoryCache()->remove(resource2.get());
    memoryCache()->add(resource3.get());
    EXPECT_TRUE(memoryCache()->contains(resource3.get()));
    EXPECT_FALSE(memoryCache()->contains(resource2.get()));

    memoryCache()->replace(resource1.get(), resource2.get());
    EXPECT_TRUE(memoryCache()->contains(resource1.get()));
    EXPECT_FALSE(memoryCache()->contains(resource2.get()));
    EXPECT_FALSE(memoryCache()->contains(resource3.get()));
}

} // namespace
