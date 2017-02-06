// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/DeferredImageDecoder.h"

#include "platform/SharedBuffer.h"
#include "platform/image-decoders/ImageDecoderTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

/**
 *  Used to test decoding SkImages out of order.
 *  e.g.
 *  SkImage* imageA = decoder.createFrameAtIndex(0);
 *  // supply more (but not all) data to the decoder
 *  SkImage* imageB = decoder.createFrameAtIndex(laterFrame);
 *  imageB->preroll();
 *  imageA->preroll();
 *
 *  This results in using the same ImageDecoder (in the ImageDecodingStore) to
 *  decode less data the second time. This test ensures that it is safe to do
 *  so.
 *
 *  @param fileName File to decode
 *  @param bytesForFirstFrame Number of bytes needed to return an SkImage
 *  @param laterFrame Frame to decode with almost complete data. Can be 0.
 */
static void mixImages(const char* fileName, size_t bytesForFirstFrame, size_t laterFrame)
{
    RefPtr<SharedBuffer> file = readFile(fileName);
    ASSERT_NE(file, nullptr);

    std::unique_ptr<DeferredImageDecoder> decoder = DeferredImageDecoder::create(*file.get(), ImageDecoder::AlphaPremultiplied, ImageDecoder::GammaAndColorProfileIgnored);
    ASSERT_TRUE(decoder.get());

    RefPtr<SharedBuffer> partialFile = SharedBuffer::create(file->data(), bytesForFirstFrame);
    decoder->setData(*partialFile.get(), false);
    RefPtr<SkImage> partialImage = decoder->createFrameAtIndex(0);

    RefPtr<SharedBuffer> almostCompleteFile = SharedBuffer::create(file->data(), file->size() - 1);
    decoder->setData(*almostCompleteFile.get(), false);
    RefPtr<SkImage> imageWithMoreData = decoder->createFrameAtIndex(laterFrame);

    imageWithMoreData->preroll();
    partialImage->preroll();
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesGif)
{
    mixImages("/LayoutTests/fast/images/resources/animated.gif", 818u, 1u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesPng)
{
    mixImages("/LayoutTests/fast/images/resources/mu.png", 910u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesJpg)
{
    mixImages("/LayoutTests/fast/images/resources/2-dht.jpg", 177u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesWebp)
{
    mixImages("/LayoutTests/fast/images/resources/webp-animated.webp", 142u, 1u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesBmp)
{
    mixImages("/LayoutTests/fast/images/resources/lenna.bmp", 122u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesIco)
{
    mixImages("/LayoutTests/fast/images/resources/wrong-frame-dimensions.ico", 1376u, 1u);
}

} // namespace blink
