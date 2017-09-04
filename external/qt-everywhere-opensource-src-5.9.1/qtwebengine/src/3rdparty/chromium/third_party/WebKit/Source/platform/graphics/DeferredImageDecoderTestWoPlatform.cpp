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
static void mixImages(const char* fileName,
                      size_t bytesForFirstFrame,
                      size_t laterFrame) {
  RefPtr<SharedBuffer> file = readFile(fileName);
  ASSERT_NE(file, nullptr);

  RefPtr<SharedBuffer> partialFile =
      SharedBuffer::create(file->data(), bytesForFirstFrame);
  std::unique_ptr<DeferredImageDecoder> decoder = DeferredImageDecoder::create(
      partialFile, false, ImageDecoder::AlphaPremultiplied,
      ImageDecoder::ColorSpaceIgnored);
  ASSERT_NE(decoder, nullptr);
  sk_sp<SkImage> partialImage = decoder->createFrameAtIndex(0);

  RefPtr<SharedBuffer> almostCompleteFile =
      SharedBuffer::create(file->data(), file->size() - 1);
  decoder->setData(almostCompleteFile, false);
  sk_sp<SkImage> imageWithMoreData = decoder->createFrameAtIndex(laterFrame);

  imageWithMoreData->preroll();
  partialImage->preroll();
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesGif) {
  mixImages("/LayoutTests/images/resources/animated.gif", 818u, 1u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesPng) {
  mixImages("/LayoutTests/images/resources/mu.png", 910u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesJpg) {
  mixImages("/LayoutTests/images/resources/2-dht.jpg", 177u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesWebp) {
  mixImages("/LayoutTests/images/resources/webp-animated.webp", 142u, 1u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesBmp) {
  mixImages("/LayoutTests/images/resources/lenna.bmp", 122u, 0u);
}

TEST(DeferredImageDecoderTestWoPlatform, mixImagesIco) {
  mixImages("/LayoutTests/images/resources/wrong-frame-dimensions.ico", 1376u,
            1u);
}

TEST(DeferredImageDecoderTestWoPlatform, fragmentedSignature) {
  const char* testFiles[] = {
      "/LayoutTests/images/resources/animated.gif",
      "/LayoutTests/images/resources/mu.png",
      "/LayoutTests/images/resources/2-dht.jpg",
      "/LayoutTests/images/resources/webp-animated.webp",
      "/LayoutTests/images/resources/lenna.bmp",
      "/LayoutTests/images/resources/wrong-frame-dimensions.ico",
  };

  for (size_t i = 0; i < SK_ARRAY_COUNT(testFiles); ++i) {
    RefPtr<SharedBuffer> fileBuffer = readFile(testFiles[i]);
    ASSERT_NE(fileBuffer, nullptr);
    // We need contiguous data, which SharedBuffer doesn't guarantee.
    sk_sp<SkData> skData = fileBuffer->getAsSkData();
    EXPECT_EQ(skData->size(), fileBuffer->size());
    const char* data = reinterpret_cast<const char*>(skData->bytes());

    // Truncated signature (only 1 byte).  Decoder instantiation should fail.
    RefPtr<SharedBuffer> buffer = SharedBuffer::create<size_t>(data, 1u);
    EXPECT_FALSE(ImageDecoder::hasSufficientDataToSniffImageType(*buffer));
    EXPECT_EQ(nullptr, DeferredImageDecoder::create(
                           buffer, false, ImageDecoder::AlphaPremultiplied,
                           ImageDecoder::ColorSpaceIgnored));

    // Append the rest of the data.  We should be able to sniff the signature
    // now, even if segmented.
    buffer->append<size_t>(data + 1, skData->size() - 1);
    EXPECT_TRUE(ImageDecoder::hasSufficientDataToSniffImageType(*buffer));
    std::unique_ptr<DeferredImageDecoder> decoder =
        DeferredImageDecoder::create(buffer, false,
                                     ImageDecoder::AlphaPremultiplied,
                                     ImageDecoder::ColorSpaceIgnored);
    ASSERT_NE(decoder, nullptr);
    EXPECT_TRUE(String(testFiles[i]).endsWith(decoder->filenameExtension()));
  }
}

}  // namespace blink
