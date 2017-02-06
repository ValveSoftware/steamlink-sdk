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

#include "core/frame/ImageBitmap.h"

#include "SkPixelRef.h" // FIXME: qualify this skia header file.
#include "core/dom/Document.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/heap/Handle.h"
#include "platform/network/ResourceRequest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class ImageBitmapTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(10, 10);
        surface->getCanvas()->clear(0xFFFFFFFF);
        m_image = fromSkSp(surface->makeImageSnapshot());

        sk_sp<SkSurface> surface2 = SkSurface::MakeRasterN32Premul(5, 5);
        surface2->getCanvas()->clear(0xAAAAAAAA);
        m_image2 = fromSkSp(surface2->makeImageSnapshot());

        // Save the global memory cache to restore it upon teardown.
        m_globalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());
    }
    virtual void TearDown()
    {
        // Garbage collection is required prior to switching out the
        // test's memory cache; image resources are released, evicting
        // them from the cache.
        ThreadHeap::collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);

        replaceMemoryCacheForTesting(m_globalMemoryCache.release());
    }

    RefPtr<SkImage> m_image, m_image2;
    Persistent<MemoryCache> m_globalMemoryCache;
};

TEST_F(ImageBitmapTest, ImageResourceConsistency)
{
    const ImageBitmapOptions defaultOptions;
    HTMLImageElement* imageElement = HTMLImageElement::create(*Document::create());
    ImageResource* image = ImageResource::create(StaticBitmapImage::create(m_image).get());
    imageElement->setImageResource(image);

    ImageBitmap* imageBitmapNoCrop = ImageBitmap::create(imageElement,
        IntRect(0, 0, m_image->width(), m_image->height()),
        &(imageElement->document()), defaultOptions);
    ImageBitmap* imageBitmapInteriorCrop = ImageBitmap::create(imageElement,
        IntRect(m_image->width() / 2, m_image->height() / 2, m_image->width() / 2, m_image->height() / 2),
        &(imageElement->document()), defaultOptions);
    ImageBitmap* imageBitmapExteriorCrop = ImageBitmap::create(imageElement,
        IntRect(-m_image->width() / 2, -m_image->height() / 2, m_image->width(), m_image->height()),
        &(imageElement->document()), defaultOptions);
    ImageBitmap* imageBitmapOutsideCrop = ImageBitmap::create(imageElement,
        IntRect(-m_image->width(), -m_image->height(), m_image->width(), m_image->height()),
        &(imageElement->document()), defaultOptions);

    ASSERT_EQ(imageBitmapNoCrop->bitmapImage()->imageForCurrentFrame(), imageElement->cachedImage()->getImage()->imageForCurrentFrame());
    ASSERT_NE(imageBitmapInteriorCrop->bitmapImage()->imageForCurrentFrame(), imageElement->cachedImage()->getImage()->imageForCurrentFrame());
    ASSERT_NE(imageBitmapExteriorCrop->bitmapImage()->imageForCurrentFrame(), imageElement->cachedImage()->getImage()->imageForCurrentFrame());

    StaticBitmapImage* emptyImage = imageBitmapOutsideCrop->bitmapImage();
    ASSERT_NE(emptyImage->imageForCurrentFrame(), imageElement->cachedImage()->getImage()->imageForCurrentFrame());
}

// Verifies that ImageBitmaps constructed from HTMLImageElements hold a reference to the original Image if the HTMLImageElement src is changed.
TEST_F(ImageBitmapTest, ImageBitmapSourceChanged)
{
    HTMLImageElement* image = HTMLImageElement::create(*Document::create());
    ImageResource* originalImageResource = ImageResource::create(
        StaticBitmapImage::create(m_image).get());
    image->setImageResource(originalImageResource);

    const ImageBitmapOptions defaultOptions;
    ImageBitmap* imageBitmap = ImageBitmap::create(image,
        IntRect(0, 0, m_image->width(), m_image->height()),
        &(image->document()), defaultOptions);
    ASSERT_EQ(imageBitmap->bitmapImage()->imageForCurrentFrame(), originalImageResource->getImage()->imageForCurrentFrame());

    ImageResource* newImageResource = ImageResource::create(
        StaticBitmapImage::create(m_image2).get());
    image->setImageResource(newImageResource);

    // The ImageBitmap should contain the same data as the original cached image
    {
        ASSERT_EQ(imageBitmap->bitmapImage()->imageForCurrentFrame(), originalImageResource->getImage()->imageForCurrentFrame());
        SkImage* image1 = imageBitmap->bitmapImage()->imageForCurrentFrame().get();
        ASSERT_NE(image1, nullptr);
        SkImage* image2 = originalImageResource->getImage()->imageForCurrentFrame().get();
        ASSERT_NE(image2, nullptr);
        ASSERT_EQ(image1, image2);
    }

    {
        ASSERT_NE(imageBitmap->bitmapImage()->imageForCurrentFrame(), newImageResource->getImage()->imageForCurrentFrame());
        SkImage* image1 = imageBitmap->bitmapImage()->imageForCurrentFrame().get();
        ASSERT_NE(image1, nullptr);
        SkImage* image2 = newImageResource->getImage()->imageForCurrentFrame().get();
        ASSERT_NE(image2, nullptr);
        ASSERT_NE(image1, image2);
    }
}

} // namespace blink
