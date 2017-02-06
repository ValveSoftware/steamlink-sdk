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

#include "platform/graphics/DeferredImageDecoder.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/SharedBuffer.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/graphics/ImageFrameGenerator.h"
#include "platform/graphics/test/MockImageDecoder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

namespace {

// Raw data for a PNG file with 1x1 white pixels.
const unsigned char whitePNG[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00,
    0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90,
    0x77, 0x53, 0xde, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47,
    0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00, 0x00, 0x09,
    0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00,
    0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00,
    0x0c, 0x49, 0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff,
    0xff, 0x3f, 0x00, 0x05, 0xfe, 0x02, 0xfe, 0xdc, 0xcc, 0x59,
    0xe7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
    0x42, 0x60, 0x82,
};

// Raw data for a 1x1 animated GIF with 2 frames.
const unsigned char animatedGIF[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00,
    0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x21,
    0xff, 0x0b, 0x4e, 0x45, 0x54, 0x53, 0x43, 0x41, 0x50, 0x45,
    0x32, 0x2e, 0x30, 0x03, 0x01, 0x00, 0x00, 0x00, 0x21, 0xff,
    0x0b, 0x49, 0x6d, 0x61, 0x67, 0x65, 0x4d, 0x61, 0x67, 0x69,
    0x63, 0x6b, 0x0d, 0x67, 0x61, 0x6d, 0x6d, 0x61, 0x3d, 0x30,
    0x2e, 0x34, 0x35, 0x34, 0x35, 0x35, 0x00, 0x21, 0xff, 0x0b,
    0x49, 0x6d, 0x61, 0x67, 0x65, 0x4d, 0x61, 0x67, 0x69, 0x63,
    0x6b, 0x0d, 0x67, 0x61, 0x6d, 0x6d, 0x61, 0x3d, 0x30, 0x2e,
    0x34, 0x35, 0x34, 0x35, 0x35, 0x00, 0x21, 0xf9, 0x04, 0x00,
    0x14, 0x00, 0xff, 0x00, 0x21, 0xfe, 0x20, 0x43, 0x72, 0x65,
    0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20,
    0x65, 0x7a, 0x67, 0x69, 0x66, 0x2e, 0x63, 0x6f, 0x6d, 0x20,
    0x67, 0x69, 0x66, 0x20, 0x6d, 0x61, 0x6b, 0x65, 0x72, 0x00,
    0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x02, 0x02, 0x44, 0x01, 0x00, 0x21, 0xf9, 0x04, 0x00, 0x14,
    0x00, 0xff, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x00, 0x02, 0x02, 0x4c, 0x01, 0x00, 0x3b,
};

struct Rasterizer {
    SkCanvas* canvas;
    SkPicture* picture;
};

} // namespace

class DeferredImageDecoderTest : public ::testing::Test, public MockImageDecoderClient {
public:
    void SetUp() override
    {
        ImageDecodingStore::instance().setCacheLimitInBytes(1024 * 1024);
        m_data = SharedBuffer::create(whitePNG, sizeof(whitePNG));
        m_frameCount = 1;
        std::unique_ptr<MockImageDecoder> decoder = MockImageDecoder::create(this);
        m_actualDecoder = decoder.get();
        m_actualDecoder->setSize(1, 1);
        m_lazyDecoder = DeferredImageDecoder::createForTesting(std::move(decoder));
        m_surface = SkSurface::MakeRasterN32Premul(100, 100);
        ASSERT_TRUE(m_surface.get());
        m_decodeRequestCount = 0;
        m_repetitionCount = cAnimationNone;
        m_status = ImageFrame::FrameComplete;
        m_frameDuration = 0;
        m_decodedSize = m_actualDecoder->size();
    }

    void TearDown() override
    {
        ImageDecodingStore::instance().clear();
    }

    void decoderBeingDestroyed() override
    {
        m_actualDecoder = 0;
    }

    void decodeRequested() override
    {
        ++m_decodeRequestCount;
    }

    size_t frameCount() override
    {
        return m_frameCount;
    }

    int repetitionCount() const override
    {
        return m_repetitionCount;
    }

    ImageFrame::Status status() override
    {
        return m_status;
    }

    float frameDuration() const override
    {
        return m_frameDuration;
    }

    IntSize decodedSize() const override
    {
        return m_decodedSize;
    }

protected:
    void useMockImageDecoderFactory()
    {
        m_lazyDecoder->frameGenerator()->setImageDecoderFactory(MockImageDecoderFactory::create(this, m_decodedSize));
    }

    // Don't own this but saves the pointer to query states.
    MockImageDecoder* m_actualDecoder;
    std::unique_ptr<DeferredImageDecoder> m_lazyDecoder;
    sk_sp<SkSurface> m_surface;
    int m_decodeRequestCount;
    RefPtr<SharedBuffer> m_data;
    size_t m_frameCount;
    int m_repetitionCount;
    ImageFrame::Status m_status;
    float m_frameDuration;
    IntSize m_decodedSize;
};

TEST_F(DeferredImageDecoderTest, drawIntoSkPicture)
{
    m_lazyDecoder->setData(*m_data, true);
    RefPtr<SkImage> image = m_lazyDecoder->createFrameAtIndex(0);
    ASSERT_TRUE(image);
    EXPECT_EQ(1, image->width());
    EXPECT_EQ(1, image->height());

    SkPictureRecorder recorder;
    SkCanvas* tempCanvas = recorder.beginRecording(100, 100, 0, 0);
    tempCanvas->drawImage(image.get(), 0, 0);
    sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
    EXPECT_EQ(0, m_decodeRequestCount);

    m_surface->getCanvas()->drawPicture(picture);
    EXPECT_EQ(0, m_decodeRequestCount);

    SkBitmap canvasBitmap;
    canvasBitmap.allocN32Pixels(100, 100);
    ASSERT_TRUE(m_surface->getCanvas()->readPixels(&canvasBitmap, 0, 0));
    SkAutoLockPixels autoLock(canvasBitmap);
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), canvasBitmap.getColor(0, 0));
}

TEST_F(DeferredImageDecoderTest, drawIntoSkPictureProgressive)
{
    RefPtr<SharedBuffer> partialData = SharedBuffer::create(m_data->data(), m_data->size() - 10);

    // Received only half the file.
    m_lazyDecoder->setData(*partialData, false);
    RefPtr<SkImage> image = m_lazyDecoder->createFrameAtIndex(0);
    ASSERT_TRUE(image);
    SkPictureRecorder recorder;
    SkCanvas* tempCanvas = recorder.beginRecording(100, 100, 0, 0);
    tempCanvas->drawImage(image.get(), 0, 0);
    m_surface->getCanvas()->drawPicture(recorder.finishRecordingAsPicture());

    // Fully received the file and draw the SkPicture again.
    m_lazyDecoder->setData(*m_data, true);
    image = m_lazyDecoder->createFrameAtIndex(0);
    ASSERT_TRUE(image);
    tempCanvas = recorder.beginRecording(100, 100, 0, 0);
    tempCanvas->drawImage(image.get(), 0, 0);
    m_surface->getCanvas()->drawPicture(recorder.finishRecordingAsPicture());

    SkBitmap canvasBitmap;
    canvasBitmap.allocN32Pixels(100, 100);
    ASSERT_TRUE(m_surface->getCanvas()->readPixels(&canvasBitmap, 0, 0));
    SkAutoLockPixels autoLock(canvasBitmap);
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), canvasBitmap.getColor(0, 0));
}

static void rasterizeMain(SkCanvas* canvas, SkPicture* picture)
{
    canvas->drawPicture(picture);
}

TEST_F(DeferredImageDecoderTest, decodeOnOtherThread)
{
    m_lazyDecoder->setData(*m_data, true);
    RefPtr<SkImage> image = m_lazyDecoder->createFrameAtIndex(0);
    ASSERT_TRUE(image);
    EXPECT_EQ(1, image->width());
    EXPECT_EQ(1, image->height());

    SkPictureRecorder recorder;
    SkCanvas* tempCanvas = recorder.beginRecording(100, 100, 0, 0);
    tempCanvas->drawImage(image.get(), 0, 0);
    sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
    EXPECT_EQ(0, m_decodeRequestCount);

    // Create a thread to rasterize SkPicture.
    std::unique_ptr<WebThread> thread = wrapUnique(Platform::current()->createThread("RasterThread"));
    thread->getWebTaskRunner()->postTask(BLINK_FROM_HERE, crossThreadBind(&rasterizeMain, crossThreadUnretained(m_surface->getCanvas()), crossThreadUnretained(picture.get())));
    thread.reset();
    EXPECT_EQ(0, m_decodeRequestCount);

    SkBitmap canvasBitmap;
    canvasBitmap.allocN32Pixels(100, 100);
    ASSERT_TRUE(m_surface->getCanvas()->readPixels(&canvasBitmap, 0, 0));
    SkAutoLockPixels autoLock(canvasBitmap);
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), canvasBitmap.getColor(0, 0));
}

TEST_F(DeferredImageDecoderTest, singleFrameImageLoading)
{
    m_status = ImageFrame::FramePartial;
    m_lazyDecoder->setData(*m_data, false);
    EXPECT_FALSE(m_lazyDecoder->frameIsCompleteAtIndex(0));
    RefPtr<SkImage> image = m_lazyDecoder->createFrameAtIndex(0);
    ASSERT_TRUE(image);
    unsigned firstId = image->uniqueID();
    EXPECT_FALSE(m_lazyDecoder->frameIsCompleteAtIndex(0));
    EXPECT_TRUE(m_actualDecoder);

    m_status = ImageFrame::FrameComplete;
    m_data->append(" ", 1u);
    m_lazyDecoder->setData(*m_data, true);
    EXPECT_FALSE(m_actualDecoder);
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(0));

    image = m_lazyDecoder->createFrameAtIndex(0);
    ASSERT_TRUE(image);
    unsigned secondId = image->uniqueID();
    EXPECT_FALSE(m_decodeRequestCount);
    EXPECT_NE(firstId, secondId);
}

TEST_F(DeferredImageDecoderTest, multiFrameImageLoading)
{
    m_repetitionCount = 10;
    m_frameCount = 1;
    m_frameDuration = 10;
    m_status = ImageFrame::FramePartial;
    m_lazyDecoder->setData(*m_data, false);

    RefPtr<SkImage> image = m_lazyDecoder->createFrameAtIndex(0);
    ASSERT_TRUE(image);
    unsigned firstId = image->uniqueID();
    EXPECT_FALSE(m_lazyDecoder->frameIsCompleteAtIndex(0));
    EXPECT_EQ(10.0f, m_lazyDecoder->frameDurationAtIndex(0));

    m_frameCount = 2;
    m_frameDuration = 20;
    m_status = ImageFrame::FrameComplete;
    m_data->append(" ", 1u);
    m_lazyDecoder->setData(*m_data, false);

    image = m_lazyDecoder->createFrameAtIndex(0);
    ASSERT_TRUE(image);
    unsigned secondId = image->uniqueID();
    EXPECT_NE(firstId, secondId);
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(0));
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(1));
    EXPECT_EQ(20.0f, m_lazyDecoder->frameDurationAtIndex(1));
    EXPECT_TRUE(m_actualDecoder);

    m_frameCount = 3;
    m_frameDuration = 30;
    m_status = ImageFrame::FrameComplete;
    m_lazyDecoder->setData(*m_data, true);
    EXPECT_FALSE(m_actualDecoder);
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(0));
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(1));
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(2));
    EXPECT_EQ(10.0f, m_lazyDecoder->frameDurationAtIndex(0));
    EXPECT_EQ(20.0f, m_lazyDecoder->frameDurationAtIndex(1));
    EXPECT_EQ(30.0f, m_lazyDecoder->frameDurationAtIndex(2));
    EXPECT_EQ(10, m_lazyDecoder->repetitionCount());
}

TEST_F(DeferredImageDecoderTest, decodedSize)
{
    m_decodedSize = IntSize(22, 33);
    m_lazyDecoder->setData(*m_data, true);
    RefPtr<SkImage> image = m_lazyDecoder->createFrameAtIndex(0);
    ASSERT_TRUE(image);
    EXPECT_EQ(m_decodedSize.width(), image->width());
    EXPECT_EQ(m_decodedSize.height(), image->height());

    useMockImageDecoderFactory();

    // The following code should not fail any assert.
    SkPictureRecorder recorder;
    SkCanvas* tempCanvas = recorder.beginRecording(100, 100, 0, 0);
    tempCanvas->drawImage(image.get(), 0, 0);
    sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
    EXPECT_EQ(0, m_decodeRequestCount);
    m_surface->getCanvas()->drawPicture(picture);
    EXPECT_EQ(1, m_decodeRequestCount);
}

TEST_F(DeferredImageDecoderTest, smallerFrameCount)
{
    m_frameCount = 1;
    m_lazyDecoder->setData(*m_data, false);
    EXPECT_EQ(m_frameCount, m_lazyDecoder->frameCount());
    m_frameCount = 2;
    m_lazyDecoder->setData(*m_data, false);
    EXPECT_EQ(m_frameCount, m_lazyDecoder->frameCount());
    m_frameCount = 0;
    m_lazyDecoder->setData(*m_data, true);
    EXPECT_EQ(m_frameCount, m_lazyDecoder->frameCount());
}

TEST_F(DeferredImageDecoderTest, frameOpacity)
{
    std::unique_ptr<ImageDecoder> actualDecoder = ImageDecoder::create(*m_data,
        ImageDecoder::AlphaPremultiplied, ImageDecoder::GammaAndColorProfileApplied);
    std::unique_ptr<DeferredImageDecoder> decoder = DeferredImageDecoder::createForTesting(std::move(actualDecoder));
    decoder->setData(*m_data, true);

    SkImageInfo pixInfo = SkImageInfo::MakeN32Premul(1, 1);

    size_t rowBytes = pixInfo.minRowBytes();
    size_t size = pixInfo.getSafeSize(rowBytes);

    SkAutoMalloc storage(size);
    SkPixmap pixmap(pixInfo, storage.get(), rowBytes);

    // Before decoding, the frame is not known to be opaque.
    RefPtr<SkImage> frame = decoder->createFrameAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_FALSE(frame->isOpaque());

    // Force a lazy decode by reading pixels.
    EXPECT_TRUE(frame->readPixels(pixmap, 0, 0));

    // After decoding, the frame is known to be opaque.
    frame = decoder->createFrameAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_TRUE(frame->isOpaque());

    // Re-generating the opaque-marked frame should not fail.
    EXPECT_TRUE(frame->readPixels(pixmap, 0, 0));
}

// The DeferredImageDecoder would sometimes assume that a frame was a certain
// size even if the actual decoder reported it had a size of 0 bytes. This test
// verifies that it's fixed by always checking with the actual decoder when
// creating a frame.
TEST_F(DeferredImageDecoderTest, respectActualDecoderSizeOnCreate)
{
    m_data = SharedBuffer::create(animatedGIF, sizeof(animatedGIF));
    m_frameCount = 2;
    forceFirstFrameToBeEmpty();
    m_lazyDecoder->setData(*m_data, false);
    m_lazyDecoder->createFrameAtIndex(0);
    m_lazyDecoder->createFrameAtIndex(1);
    m_lazyDecoder->setData(*m_data, true);
    // Clears only the first frame (0 bytes). If DeferredImageDecoder doesn't
    // check with the actual decoder it reports 4 bytes instead.
    size_t frameBytesCleared = m_lazyDecoder->clearCacheExceptFrame(1);
    EXPECT_EQ(static_cast<size_t>(0), frameBytesCleared);
}

} // namespace blink
