/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "platform/graphics/ImageDecodingStore.h"

#include "platform/SharedBuffer.h"
#include "platform/graphics/ImageFrameGenerator.h"
#include "platform/graphics/test/MockDiscardablePixelRef.h"
#include "platform/graphics/test/MockImageDecoder.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class ImageDecodingStoreTest : public ::testing::Test, public MockImageDecoderClient {
public:
    virtual void SetUp()
    {
        ImageDecodingStore::instance()->setCacheLimitInBytes(1024 * 1024);
        ImageDecodingStore::instance()->setImageCachingEnabled(true);
        m_data = SharedBuffer::create();
        m_generator = ImageFrameGenerator::create(SkISize::Make(100, 100), m_data, true);
        m_decodersDestroyed = 0;
    }

    virtual void TearDown()
    {
        ImageDecodingStore::instance()->clear();
    }

    virtual void decoderBeingDestroyed()
    {
        ++m_decodersDestroyed;
    }

    virtual void frameBufferRequested()
    {
        // Decoder is never used by ImageDecodingStore.
        ASSERT_TRUE(false);
    }

    virtual ImageFrame::Status status()
    {
        return ImageFrame::FramePartial;
    }

    virtual size_t frameCount() { return 1; }
    virtual int repetitionCount() const { return cAnimationNone; }
    virtual float frameDuration() const { return 0; }

protected:
    PassOwnPtr<ScaledImageFragment> createCompleteImage(const SkISize& size, bool discardable = false, size_t index = 0)
    {
        SkBitmap bitmap;
        bitmap.setInfo(SkImageInfo::MakeN32Premul(size));
        if (!discardable) {
            bitmap.allocPixels();
        } else {
            MockDiscardablePixelRef::Allocator mockDiscardableAllocator;
            bitmap.allocPixels(&mockDiscardableAllocator, 0);
        }
        return ScaledImageFragment::createComplete(size, index, bitmap);
    }

    PassOwnPtr<ScaledImageFragment> createIncompleteImage(const SkISize& size, bool discardable = false, size_t generation = 0)
    {
        SkBitmap bitmap;
        bitmap.setInfo(SkImageInfo::MakeN32Premul(size));
        if (!discardable) {
            bitmap.allocPixels();
        } else {
            MockDiscardablePixelRef::Allocator mockDiscardableAllocator;
            bitmap.allocPixels(&mockDiscardableAllocator, 0);
        }
        return ScaledImageFragment::createPartial(size, 0, generation, bitmap);
    }

    void insertCache(const SkISize& size)
    {
        const ScaledImageFragment* image = ImageDecodingStore::instance()->insertAndLockCache(
            m_generator.get(), createCompleteImage(size));
        unlockCache(image);
    }

    const ScaledImageFragment* lockCache(const SkISize& size, size_t index = 0)
    {
        const ScaledImageFragment* cachedImage = 0;
        if (ImageDecodingStore::instance()->lockCache(m_generator.get(), size, index, &cachedImage))
            return cachedImage;
        return 0;
    }

    void unlockCache(const ScaledImageFragment* cachedImage)
    {
        ImageDecodingStore::instance()->unlockCache(m_generator.get(), cachedImage);
    }

    void evictOneCache()
    {
        size_t memoryUsageInBytes = ImageDecodingStore::instance()->memoryUsageInBytes();
        if (memoryUsageInBytes)
            ImageDecodingStore::instance()->setCacheLimitInBytes(memoryUsageInBytes - 1);
        else
            ImageDecodingStore::instance()->setCacheLimitInBytes(0);
    }

    bool isCacheAlive(const SkISize& size)
    {
        const ScaledImageFragment* cachedImage = lockCache(size);
        if (!cachedImage)
            return false;
        ImageDecodingStore::instance()->unlockCache(m_generator.get(), cachedImage);
        return true;
    }

    RefPtr<SharedBuffer> m_data;
    RefPtr<ImageFrameGenerator> m_generator;
    int m_decodersDestroyed;
};

TEST_F(ImageDecodingStoreTest, evictOneCache)
{
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    insertCache(SkISize::Make(3, 3));
    EXPECT_EQ(3, ImageDecodingStore::instance()->cacheEntries());

    evictOneCache();
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());

    evictOneCache();
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, pruneOrderIsLeastRecentlyUsed)
{
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    insertCache(SkISize::Make(3, 3));
    insertCache(SkISize::Make(4, 4));
    insertCache(SkISize::Make(5, 5));
    EXPECT_EQ(5, ImageDecodingStore::instance()->cacheEntries());

    // Use cache in the order 3, 2, 4, 1, 5.
    EXPECT_TRUE(isCacheAlive(SkISize::Make(3, 3)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(2, 2)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(4, 4)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(1, 1)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(5, 5)));

    // Evict 3.
    evictOneCache();
    EXPECT_FALSE(isCacheAlive(SkISize::Make(3, 3)));
    EXPECT_EQ(4, ImageDecodingStore::instance()->cacheEntries());

    // Evict 2.
    evictOneCache();
    EXPECT_FALSE(isCacheAlive(SkISize::Make(2, 2)));
    EXPECT_EQ(3, ImageDecodingStore::instance()->cacheEntries());

    // Evict 4.
    evictOneCache();
    EXPECT_FALSE(isCacheAlive(SkISize::Make(4, 4)));
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());

    // Evict 1.
    evictOneCache();
    EXPECT_FALSE(isCacheAlive(SkISize::Make(1, 1)));
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());

    // Evict 5.
    evictOneCache();
    EXPECT_FALSE(isCacheAlive(SkISize::Make(5, 5)));
    EXPECT_EQ(0, ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, pruneCausedByInsertion)
{
    ImageDecodingStore::instance()->setCacheLimitInBytes(100);

    // Insert 100 entries.
    // Cache entries stored should increase and eventually decrease to 1.
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    insertCache(SkISize::Make(3, 3));
    EXPECT_EQ(3, ImageDecodingStore::instance()->cacheEntries());

    for (int i = 4; i <= 100; ++i)
        insertCache(SkISize::Make(i, i));

    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());
    for (int i = 1; i <= 99; ++i)
        EXPECT_FALSE(isCacheAlive(SkISize::Make(i, i)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(100, 100)));
}

TEST_F(ImageDecodingStoreTest, cacheInUseNotEvicted)
{
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    insertCache(SkISize::Make(3, 3));
    EXPECT_EQ(3, ImageDecodingStore::instance()->cacheEntries());

    const ScaledImageFragment* cachedImage = lockCache(SkISize::Make(1, 1));
    ASSERT_TRUE(cachedImage);

    // Cache 2 is evicted because cache 1 is in use.
    evictOneCache();
    EXPECT_TRUE(isCacheAlive(SkISize::Make(1, 1)));
    EXPECT_FALSE(isCacheAlive(SkISize::Make(2, 2)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(3, 3)));

    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());
    unlockCache(cachedImage);
}

TEST_F(ImageDecodingStoreTest, destroyImageFrameGenerator)
{
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    insertCache(SkISize::Make(3, 3));
    OwnPtr<ImageDecoder> decoder = MockImageDecoder::create(this);
    decoder->setSize(1, 1);
    ImageDecodingStore::instance()->insertDecoder(m_generator.get(), decoder.release(), false);
    EXPECT_EQ(4, ImageDecodingStore::instance()->cacheEntries());

    m_generator.clear();
    EXPECT_FALSE(ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, insertDecoder)
{
    const SkISize size = SkISize::Make(1, 1);
    OwnPtr<ImageDecoder> decoder = MockImageDecoder::create(this);
    decoder->setSize(1, 1);
    const ImageDecoder* refDecoder = decoder.get();
    ImageDecodingStore::instance()->insertDecoder(m_generator.get(), decoder.release(), false);
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(4u, ImageDecodingStore::instance()->memoryUsageInBytes());

    ImageDecoder* testDecoder;
    EXPECT_TRUE(ImageDecodingStore::instance()->lockDecoder(m_generator.get(), size, &testDecoder));
    EXPECT_TRUE(testDecoder);
    EXPECT_EQ(refDecoder, testDecoder);
    ImageDecodingStore::instance()->unlockDecoder(m_generator.get(), testDecoder);
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, evictDecoder)
{
    OwnPtr<ImageDecoder> decoder1 = MockImageDecoder::create(this);
    OwnPtr<ImageDecoder> decoder2 = MockImageDecoder::create(this);
    OwnPtr<ImageDecoder> decoder3 = MockImageDecoder::create(this);
    decoder1->setSize(1, 1);
    decoder2->setSize(2, 2);
    decoder3->setSize(3, 3);
    ImageDecodingStore::instance()->insertDecoder(m_generator.get(), decoder1.release(), false);
    ImageDecodingStore::instance()->insertDecoder(m_generator.get(), decoder2.release(), false);
    ImageDecodingStore::instance()->insertDecoder(m_generator.get(), decoder3.release(), false);
    EXPECT_EQ(3, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(56u, ImageDecodingStore::instance()->memoryUsageInBytes());

    evictOneCache();
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(52u, ImageDecodingStore::instance()->memoryUsageInBytes());

    evictOneCache();
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(36u, ImageDecodingStore::instance()->memoryUsageInBytes());

    evictOneCache();
    EXPECT_FALSE(ImageDecodingStore::instance()->cacheEntries());
    EXPECT_FALSE(ImageDecodingStore::instance()->memoryUsageInBytes());
}

TEST_F(ImageDecodingStoreTest, decoderInUseNotEvicted)
{
    OwnPtr<ImageDecoder> decoder1 = MockImageDecoder::create(this);
    OwnPtr<ImageDecoder> decoder2 = MockImageDecoder::create(this);
    OwnPtr<ImageDecoder> decoder3 = MockImageDecoder::create(this);
    decoder1->setSize(1, 1);
    decoder2->setSize(2, 2);
    decoder3->setSize(3, 3);
    ImageDecodingStore::instance()->insertDecoder(m_generator.get(), decoder1.release(), false);
    ImageDecodingStore::instance()->insertDecoder(m_generator.get(), decoder2.release(), false);
    ImageDecodingStore::instance()->insertDecoder(m_generator.get(), decoder3.release(), false);
    EXPECT_EQ(3, ImageDecodingStore::instance()->cacheEntries());

    ImageDecoder* testDecoder;
    EXPECT_TRUE(ImageDecodingStore::instance()->lockDecoder(m_generator.get(), SkISize::Make(2, 2), &testDecoder));

    evictOneCache();
    evictOneCache();
    evictOneCache();
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(16u, ImageDecodingStore::instance()->memoryUsageInBytes());

    ImageDecodingStore::instance()->unlockDecoder(m_generator.get(), testDecoder);
    evictOneCache();
    EXPECT_FALSE(ImageDecodingStore::instance()->cacheEntries());
    EXPECT_FALSE(ImageDecodingStore::instance()->memoryUsageInBytes());
}

TEST_F(ImageDecodingStoreTest, removeDecoder)
{
    const SkISize size = SkISize::Make(1, 1);
    OwnPtr<ImageDecoder> decoder = MockImageDecoder::create(this);
    decoder->setSize(1, 1);
    const ImageDecoder* refDecoder = decoder.get();
    ImageDecodingStore::instance()->insertDecoder(m_generator.get(), decoder.release(), false);
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(4u, ImageDecodingStore::instance()->memoryUsageInBytes());

    ImageDecoder* testDecoder;
    EXPECT_TRUE(ImageDecodingStore::instance()->lockDecoder(m_generator.get(), size, &testDecoder));
    EXPECT_TRUE(testDecoder);
    EXPECT_EQ(refDecoder, testDecoder);
    ImageDecodingStore::instance()->removeDecoder(m_generator.get(), testDecoder);
    EXPECT_FALSE(ImageDecodingStore::instance()->cacheEntries());

    EXPECT_FALSE(ImageDecodingStore::instance()->lockDecoder(m_generator.get(), size, &testDecoder));
}

TEST_F(ImageDecodingStoreTest, multipleIndex)
{
    const SkISize size = SkISize::Make(1, 1);
    const ScaledImageFragment* refImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createCompleteImage(size, false, 0));
    unlockCache(refImage);
    const ScaledImageFragment* testImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createCompleteImage(size, false, 1));
    unlockCache(testImage);
    EXPECT_NE(refImage, testImage);
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());

    EXPECT_TRUE(ImageDecodingStore::instance()->lockCache(m_generator.get(), size, 1, &refImage));
    EXPECT_EQ(refImage, testImage);
    unlockCache(refImage);
}

TEST_F(ImageDecodingStoreTest, finalAndPartialImage)
{
    const SkISize size = SkISize::Make(1, 1);
    const ScaledImageFragment* refImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createCompleteImage(size, false, 0));
    unlockCache(refImage);
    const ScaledImageFragment* testImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createIncompleteImage(size, false, 1));
    unlockCache(testImage);
    EXPECT_NE(refImage, testImage);
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());

    EXPECT_TRUE(ImageDecodingStore::instance()->lockCache(m_generator.get(), size, 0, &refImage));
    EXPECT_NE(refImage, testImage);
    unlockCache(refImage);
}

TEST_F(ImageDecodingStoreTest, insertNoGenerationCollision)
{
    const SkISize size = SkISize::Make(1, 1);
    const ScaledImageFragment* refImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createIncompleteImage(size, false, 1));
    unlockCache(refImage);
    const ScaledImageFragment* testImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createIncompleteImage(size, false, 2));
    unlockCache(testImage);
    EXPECT_NE(refImage, testImage);
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, insertGenerationCollision)
{
    const SkISize size = SkISize::Make(1, 1);
    const ScaledImageFragment* refImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createIncompleteImage(size, false, 1));
    unlockCache(refImage);
    const ScaledImageFragment* testImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createIncompleteImage(size, false, 1));
    unlockCache(testImage);
    EXPECT_EQ(refImage, testImage);
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, insertGenerationCollisionAfterMemoryDiscarded)
{
    const SkISize size = SkISize::Make(1, 1);
    const ScaledImageFragment* refImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createIncompleteImage(size, true, 1));
    unlockCache(refImage);
    MockDiscardablePixelRef* pixelRef = static_cast<MockDiscardablePixelRef*>(refImage->bitmap().pixelRef());
    pixelRef->discard();
    const ScaledImageFragment* testImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createIncompleteImage(size, false, 1));
    unlockCache(testImage);
    EXPECT_NE(refImage, testImage);
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, lockCacheFailedAfterMemoryDiscarded)
{
    const ScaledImageFragment* cachedImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createCompleteImage(SkISize::Make(1, 1), true));
    unlockCache(cachedImage);
    MockDiscardablePixelRef* pixelRef = static_cast<MockDiscardablePixelRef*>(cachedImage->bitmap().pixelRef());
    pixelRef->discard();
    EXPECT_EQ(0, lockCache(SkISize::Make(1, 1)));
    EXPECT_EQ(0, ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, clear)
{
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());

    OwnPtr<ImageDecoder> decoder = MockImageDecoder::create(this);
    decoder->setSize(1, 1);
    ImageDecodingStore::instance()->insertDecoder(m_generator.get(), decoder.release(), false);
    EXPECT_EQ(3, ImageDecodingStore::instance()->cacheEntries());

    ImageDecodingStore::instance()->clear();
    EXPECT_EQ(0, ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, clearInUse)
{
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());

    const ScaledImageFragment* cachedImage = lockCache(SkISize::Make(1, 1));
    ASSERT_TRUE(cachedImage);
    ImageDecodingStore::instance()->clear();
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());

    unlockCache(cachedImage);
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, disableImageCaching)
{
    ImageDecodingStore::instance()->setImageCachingEnabled(false);
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    EXPECT_EQ(0, ImageDecodingStore::instance()->cacheEntries());

    const ScaledImageFragment* cachedImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createCompleteImage(SkISize::Make(3, 3), true));
    EXPECT_EQ(1, ImageDecodingStore::instance()->cacheEntries());
    unlockCache(cachedImage);
    EXPECT_EQ(0, ImageDecodingStore::instance()->cacheEntries());
}

} // namespace
