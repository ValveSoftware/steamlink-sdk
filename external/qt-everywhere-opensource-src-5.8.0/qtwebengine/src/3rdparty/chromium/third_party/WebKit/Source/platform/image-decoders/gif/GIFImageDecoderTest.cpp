/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/image-decoders/gif/GIFImageDecoder.h"

#include "platform/SharedBuffer.h"
#include "platform/image-decoders/ImageDecoderTestHelpers.h"
#include "public/platform/WebData.h"
#include "public/platform/WebSize.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

namespace {

const char decodersTestingDir[] = "Source/platform/image-decoders/testing";
const char layoutTestResourcesDir[] = "LayoutTests/fast/images/resources";

std::unique_ptr<ImageDecoder> createDecoder()
{
    return wrapUnique(new GIFImageDecoder(ImageDecoder::AlphaNotPremultiplied, ImageDecoder::GammaAndColorProfileApplied, ImageDecoder::noDecodedImageByteLimit));
}

void testRandomFrameDecode(const char* dir, const char* gifFile)
{
    SCOPED_TRACE(gifFile);

    RefPtr<SharedBuffer> fullData = readFile(dir, gifFile);
    ASSERT_TRUE(fullData.get());
    Vector<unsigned> baselineHashes;
    createDecodingBaseline(&createDecoder, fullData.get(), &baselineHashes);
    size_t frameCount = baselineHashes.size();

    // Random decoding should get the same results as sequential decoding.
    std::unique_ptr<ImageDecoder> decoder = createDecoder();
    decoder->setData(fullData.get(), true);
    const size_t skippingStep = 5;
    for (size_t i = 0; i < skippingStep; ++i) {
        for (size_t j = i; j < frameCount; j += skippingStep) {
            SCOPED_TRACE(testing::Message() << "Random i:" << i << " j:" << j);
            ImageFrame* frame = decoder->frameBufferAtIndex(j);
            EXPECT_EQ(baselineHashes[j], hashBitmap(frame->bitmap()));
        }
    }

    // Decoding in reverse order.
    decoder = createDecoder();
    decoder->setData(fullData.get(), true);
    for (size_t i = frameCount; i; --i) {
        SCOPED_TRACE(testing::Message() << "Reverse i:" << i);
        ImageFrame* frame = decoder->frameBufferAtIndex(i - 1);
        EXPECT_EQ(baselineHashes[i - 1], hashBitmap(frame->bitmap()));
    }
}

void testRandomDecodeAfterClearFrameBufferCache(const char* dir, const char* gifFile)
{
    SCOPED_TRACE(gifFile);

    RefPtr<SharedBuffer> data = readFile(dir, gifFile);
    ASSERT_TRUE(data.get());
    Vector<unsigned> baselineHashes;
    createDecodingBaseline(&createDecoder, data.get(), &baselineHashes);
    size_t frameCount = baselineHashes.size();

    std::unique_ptr<ImageDecoder> decoder = createDecoder();
    decoder->setData(data.get(), true);
    for (size_t clearExceptFrame = 0; clearExceptFrame < frameCount; ++clearExceptFrame) {
        decoder->clearCacheExceptFrame(clearExceptFrame);
        const size_t skippingStep = 5;
        for (size_t i = 0; i < skippingStep; ++i) {
            for (size_t j = 0; j < frameCount; j += skippingStep) {
                SCOPED_TRACE(testing::Message() << "Random i:" << i << " j:" << j);
                ImageFrame* frame = decoder->frameBufferAtIndex(j);
                EXPECT_EQ(baselineHashes[j], hashBitmap(frame->bitmap()));
            }
        }
    }
}

} // anonymous namespace

TEST(GIFImageDecoderTest, decodeTwoFrames)
{
    std::unique_ptr<ImageDecoder> decoder = createDecoder();

    RefPtr<SharedBuffer> data = readFile(layoutTestResourcesDir, "animated.gif");
    ASSERT_TRUE(data.get());
    decoder->setData(data.get(), true);
    EXPECT_EQ(cAnimationLoopOnce, decoder->repetitionCount());

    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    uint32_t generationID0 = frame->bitmap().getGenerationID();
    EXPECT_EQ(ImageFrame::FrameComplete, frame->getStatus());
    EXPECT_EQ(16, frame->bitmap().width());
    EXPECT_EQ(16, frame->bitmap().height());

    frame = decoder->frameBufferAtIndex(1);
    uint32_t generationID1 = frame->bitmap().getGenerationID();
    EXPECT_EQ(ImageFrame::FrameComplete, frame->getStatus());
    EXPECT_EQ(16, frame->bitmap().width());
    EXPECT_EQ(16, frame->bitmap().height());
    EXPECT_TRUE(generationID0 != generationID1);

    EXPECT_EQ(2u, decoder->frameCount());
    EXPECT_EQ(cAnimationLoopInfinite, decoder->repetitionCount());
}

TEST(GIFImageDecoderTest, parseAndDecode)
{
    std::unique_ptr<ImageDecoder> decoder = createDecoder();

    RefPtr<SharedBuffer> data = readFile(layoutTestResourcesDir, "animated.gif");
    ASSERT_TRUE(data.get());
    decoder->setData(data.get(), true);
    EXPECT_EQ(cAnimationLoopOnce, decoder->repetitionCount());

    // This call will parse the entire file.
    EXPECT_EQ(2u, decoder->frameCount());

    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    EXPECT_EQ(ImageFrame::FrameComplete, frame->getStatus());
    EXPECT_EQ(16, frame->bitmap().width());
    EXPECT_EQ(16, frame->bitmap().height());

    frame = decoder->frameBufferAtIndex(1);
    EXPECT_EQ(ImageFrame::FrameComplete, frame->getStatus());
    EXPECT_EQ(16, frame->bitmap().width());
    EXPECT_EQ(16, frame->bitmap().height());
    EXPECT_EQ(cAnimationLoopInfinite, decoder->repetitionCount());
}

TEST(GIFImageDecoderTest, parseByteByByte)
{
    std::unique_ptr<ImageDecoder> decoder = createDecoder();

    RefPtr<SharedBuffer> data = readFile(layoutTestResourcesDir, "animated.gif");
    ASSERT_TRUE(data.get());

    size_t frameCount = 0;

    // Pass data to decoder byte by byte.
    for (size_t length = 1; length <= data->size(); ++length) {
        RefPtr<SharedBuffer> tempData = SharedBuffer::create(data->data(), length);
        decoder->setData(tempData.get(), length == data->size());

        EXPECT_LE(frameCount, decoder->frameCount());
        frameCount = decoder->frameCount();
    }

    EXPECT_EQ(2u, decoder->frameCount());

    decoder->frameBufferAtIndex(0);
    decoder->frameBufferAtIndex(1);
    EXPECT_EQ(cAnimationLoopInfinite, decoder->repetitionCount());
}

TEST(GIFImageDecoderTest, parseAndDecodeByteByByte)
{
    std::unique_ptr<ImageDecoder> decoder = createDecoder();

    RefPtr<SharedBuffer> data = readFile(layoutTestResourcesDir, "animated-gif-with-offsets.gif");
    ASSERT_TRUE(data.get());

    size_t frameCount = 0;
    size_t framesDecoded = 0;

    // Pass data to decoder byte by byte.
    for (size_t length = 1; length <= data->size(); ++length) {
        RefPtr<SharedBuffer> tempData = SharedBuffer::create(data->data(), length);
        decoder->setData(tempData.get(), length == data->size());

        EXPECT_LE(frameCount, decoder->frameCount());
        frameCount = decoder->frameCount();

        ImageFrame* frame = decoder->frameBufferAtIndex(frameCount - 1);
        if (frame && frame->getStatus() == ImageFrame::FrameComplete && framesDecoded < frameCount)
            ++framesDecoded;
    }

    EXPECT_EQ(5u, decoder->frameCount());
    EXPECT_EQ(5u, framesDecoded);
    EXPECT_EQ(cAnimationLoopInfinite, decoder->repetitionCount());
}

TEST(GIFImageDecoderTest, brokenSecondFrame)
{
    std::unique_ptr<ImageDecoder> decoder = createDecoder();

    RefPtr<SharedBuffer> data = readFile(decodersTestingDir, "broken.gif");
    ASSERT_TRUE(data.get());
    decoder->setData(data.get(), true);

    // One frame is detected but cannot be decoded.
    EXPECT_EQ(1u, decoder->frameCount());
    ImageFrame* frame = decoder->frameBufferAtIndex(1);
    EXPECT_FALSE(frame);
}

TEST(GIFImageDecoderTest, progressiveDecode)
{
    RefPtr<SharedBuffer> fullData = readFile(decodersTestingDir, "radient.gif");
    ASSERT_TRUE(fullData.get());
    const size_t fullLength = fullData->size();

    std::unique_ptr<ImageDecoder> decoder;
    ImageFrame* frame;

    Vector<unsigned> truncatedHashes;
    Vector<unsigned> progressiveHashes;

    // Compute hashes when the file is truncated.
    const size_t increment = 1;
    for (size_t i = 1; i <= fullLength; i += increment) {
        decoder = createDecoder();
        RefPtr<SharedBuffer> data = SharedBuffer::create(fullData->data(), i);
        decoder->setData(data.get(), i == fullLength);
        frame = decoder->frameBufferAtIndex(0);
        if (!frame) {
            truncatedHashes.append(0);
            continue;
        }
        truncatedHashes.append(hashBitmap(frame->bitmap()));
    }

    // Compute hashes when the file is progressively decoded.
    decoder = createDecoder();
    EXPECT_EQ(cAnimationLoopOnce, decoder->repetitionCount());
    for (size_t i = 1; i <= fullLength; i += increment) {
        RefPtr<SharedBuffer> data = SharedBuffer::create(fullData->data(), i);
        decoder->setData(data.get(), i == fullLength);
        frame = decoder->frameBufferAtIndex(0);
        if (!frame) {
            progressiveHashes.append(0);
            continue;
        }
        progressiveHashes.append(hashBitmap(frame->bitmap()));
    }
    EXPECT_EQ(cAnimationNone, decoder->repetitionCount());

    bool match = true;
    for (size_t i = 0; i < truncatedHashes.size(); ++i) {
        if (truncatedHashes[i] != progressiveHashes[i]) {
            match = false;
            break;
        }
    }
    EXPECT_TRUE(match);
}

TEST(GIFImageDecoderTest, allDataReceivedTruncation)
{
    std::unique_ptr<ImageDecoder> decoder = createDecoder();

    RefPtr<SharedBuffer> data = readFile(layoutTestResourcesDir, "animated.gif");
    ASSERT_TRUE(data.get());

    ASSERT_GE(data->size(), 10u);
    RefPtr<SharedBuffer> tempData = SharedBuffer::create(data->data(), data->size() - 10);
    decoder->setData(tempData.get(), true);

    EXPECT_EQ(2u, decoder->frameCount());
    EXPECT_FALSE(decoder->failed());

    decoder->frameBufferAtIndex(0);
    EXPECT_FALSE(decoder->failed());
    decoder->frameBufferAtIndex(1);
    EXPECT_TRUE(decoder->failed());
}

TEST(GIFImageDecoderTest, frameIsComplete)
{
    std::unique_ptr<ImageDecoder> decoder = createDecoder();

    RefPtr<SharedBuffer> data = readFile(layoutTestResourcesDir, "animated.gif");
    ASSERT_TRUE(data.get());
    decoder->setData(data.get(), true);

    EXPECT_EQ(2u, decoder->frameCount());
    EXPECT_FALSE(decoder->failed());
    EXPECT_TRUE(decoder->frameIsCompleteAtIndex(0));
    EXPECT_TRUE(decoder->frameIsCompleteAtIndex(1));
    EXPECT_EQ(cAnimationLoopInfinite, decoder->repetitionCount());
}

TEST(GIFImageDecoderTest, frameIsCompleteLoading)
{
    std::unique_ptr<ImageDecoder> decoder = createDecoder();

    RefPtr<SharedBuffer> data = readFile(layoutTestResourcesDir, "animated.gif");
    ASSERT_TRUE(data.get());

    ASSERT_GE(data->size(), 10u);
    RefPtr<SharedBuffer> tempData = SharedBuffer::create(data->data(), data->size() - 10);
    decoder->setData(tempData.get(), false);

    EXPECT_EQ(2u, decoder->frameCount());
    EXPECT_FALSE(decoder->failed());
    EXPECT_TRUE(decoder->frameIsCompleteAtIndex(0));
    EXPECT_FALSE(decoder->frameIsCompleteAtIndex(1));

    decoder->setData(data.get(), true);
    EXPECT_EQ(2u, decoder->frameCount());
    EXPECT_TRUE(decoder->frameIsCompleteAtIndex(0));
    EXPECT_TRUE(decoder->frameIsCompleteAtIndex(1));
}

TEST(GIFImageDecoderTest, badTerminator)
{
    RefPtr<SharedBuffer> referenceData = readFile(decodersTestingDir, "radient.gif");
    RefPtr<SharedBuffer> testData = readFile(decodersTestingDir, "radient-bad-terminator.gif");
    ASSERT_TRUE(referenceData.get());
    ASSERT_TRUE(testData.get());

    std::unique_ptr<ImageDecoder> referenceDecoder = createDecoder();
    referenceDecoder->setData(referenceData.get(), true);
    EXPECT_EQ(1u, referenceDecoder->frameCount());
    ImageFrame* referenceFrame = referenceDecoder->frameBufferAtIndex(0);
    ASSERT(referenceFrame);

    std::unique_ptr<ImageDecoder> testDecoder = createDecoder();
    testDecoder->setData(testData.get(), true);
    EXPECT_EQ(1u, testDecoder->frameCount());
    ImageFrame* testFrame = testDecoder->frameBufferAtIndex(0);
    ASSERT(testFrame);

    EXPECT_EQ(hashBitmap(referenceFrame->bitmap()), hashBitmap(testFrame->bitmap()));
}

TEST(GIFImageDecoderTest, updateRequiredPreviousFrameAfterFirstDecode)
{
    std::unique_ptr<ImageDecoder> decoder = createDecoder();

    RefPtr<SharedBuffer> fullData = readFile(layoutTestResourcesDir, "animated-10color.gif");
    ASSERT_TRUE(fullData.get());

    // Give it data that is enough to parse but not decode in order to check the status
    // of requiredPreviousFrameIndex before decoding.
    size_t partialSize = 1;
    do {
        RefPtr<SharedBuffer> data = SharedBuffer::create(fullData->data(), partialSize);
        decoder->setData(data.get(), false);
        ++partialSize;
    } while (!decoder->frameCount() || decoder->frameBufferAtIndex(0)->getStatus() == ImageFrame::FrameEmpty);

    EXPECT_EQ(kNotFound, decoder->frameBufferAtIndex(0)->requiredPreviousFrameIndex());
    unsigned frameCount = decoder->frameCount();
    for (size_t i = 1; i < frameCount; ++i)
        EXPECT_EQ(i - 1, decoder->frameBufferAtIndex(i)->requiredPreviousFrameIndex());

    decoder->setData(fullData.get(), true);
    for (size_t i = 0; i < frameCount; ++i)
        EXPECT_EQ(kNotFound, decoder->frameBufferAtIndex(i)->requiredPreviousFrameIndex());
}

TEST(GIFImageDecoderTest, randomFrameDecode)
{
    // Single frame image.
    testRandomFrameDecode(decodersTestingDir, "radient.gif");
    // Multiple frame images.
    testRandomFrameDecode(layoutTestResourcesDir, "animated-gif-with-offsets.gif");
    testRandomFrameDecode(layoutTestResourcesDir, "animated-10color.gif");
}

TEST(GIFImageDecoderTest, randomDecodeAfterClearFrameBufferCache)
{
    // Single frame image.
    testRandomDecodeAfterClearFrameBufferCache(decodersTestingDir, "radient.gif");
    // Multiple frame images.
    testRandomDecodeAfterClearFrameBufferCache(layoutTestResourcesDir, "animated-gif-with-offsets.gif");
    testRandomDecodeAfterClearFrameBufferCache(layoutTestResourcesDir, "animated-10color.gif");
}

TEST(GIFImageDecoderTest, resumePartialDecodeAfterClearFrameBufferCache)
{
    RefPtr<SharedBuffer> fullData = readFile(layoutTestResourcesDir, "animated-10color.gif");
    ASSERT_TRUE(fullData.get());
    Vector<unsigned> baselineHashes;
    createDecodingBaseline(&createDecoder, fullData.get(), &baselineHashes);
    size_t frameCount = baselineHashes.size();

    std::unique_ptr<ImageDecoder> decoder = createDecoder();

    // Let frame 0 be partially decoded.
    size_t partialSize = 1;
    do {
        RefPtr<SharedBuffer> data = SharedBuffer::create(fullData->data(), partialSize);
        decoder->setData(data.get(), false);
        ++partialSize;
    } while (!decoder->frameCount() || decoder->frameBufferAtIndex(0)->getStatus() == ImageFrame::FrameEmpty);

    // Skip to the last frame and clear.
    decoder->setData(fullData.get(), true);
    EXPECT_EQ(frameCount, decoder->frameCount());
    ImageFrame* lastFrame = decoder->frameBufferAtIndex(frameCount - 1);
    EXPECT_EQ(baselineHashes[frameCount - 1], hashBitmap(lastFrame->bitmap()));
    decoder->clearCacheExceptFrame(kNotFound);

    // Resume decoding of the first frame.
    ImageFrame* firstFrame = decoder->frameBufferAtIndex(0);
    EXPECT_EQ(ImageFrame::FrameComplete, firstFrame->getStatus());
    EXPECT_EQ(baselineHashes[0], hashBitmap(firstFrame->bitmap()));
}

// The first LZW codes in the image are invalid values that try to create a loop
// in the dictionary. Decoding should fail, but not infinitely loop or corrupt memory.
TEST(GIFImageDecoderTest, badInitialCode)
{
    RefPtr<SharedBuffer> testData = readFile(decodersTestingDir, "bad-initial-code.gif");
    ASSERT_TRUE(testData.get());

    std::unique_ptr<ImageDecoder> testDecoder = createDecoder();
    testDecoder->setData(testData.get(), true);
    EXPECT_EQ(1u, testDecoder->frameCount());
    ASSERT_TRUE(testDecoder->frameBufferAtIndex(0));
    EXPECT_TRUE(testDecoder->failed());
}

// The image has an invalid LZW code that exceeds dictionary size. Decoding should fail.
TEST(GIFImageDecoderTest, badCode)
{
    RefPtr<SharedBuffer> testData = readFile(decodersTestingDir, "bad-code.gif");
    ASSERT_TRUE(testData.get());

    std::unique_ptr<ImageDecoder> testDecoder = createDecoder();
    testDecoder->setData(testData.get(), true);
    EXPECT_EQ(1u, testDecoder->frameCount());
    ASSERT_TRUE(testDecoder->frameBufferAtIndex(0));
    EXPECT_TRUE(testDecoder->failed());
}

TEST(GIFImageDecoderTest, invalidDisposalMethod)
{
    std::unique_ptr<ImageDecoder> decoder = createDecoder();

    // The image has 2 frames, with disposal method 4 and 5, respectively.
    RefPtr<SharedBuffer> data = readFile(decodersTestingDir, "invalid-disposal-method.gif");
    ASSERT_TRUE(data.get());
    decoder->setData(data.get(), true);

    EXPECT_EQ(2u, decoder->frameCount());
    // Disposal method 4 is converted to ImageFrame::DisposeOverwritePrevious.
    EXPECT_EQ(ImageFrame::DisposeOverwritePrevious, decoder->frameBufferAtIndex(0)->getDisposalMethod());
    // Disposal method 5 is ignored.
    EXPECT_EQ(ImageFrame::DisposeNotSpecified, decoder->frameBufferAtIndex(1)->getDisposalMethod());
}

TEST(GIFImageDecoderTest, firstFrameHasGreaterSizeThanScreenSize)
{
    RefPtr<SharedBuffer> fullData = readFile(decodersTestingDir, "first-frame-has-greater-size-than-screen-size.gif");
    ASSERT_TRUE(fullData.get());

    std::unique_ptr<ImageDecoder> decoder;
    IntSize frameSize;

    // Compute hashes when the file is truncated.
    for (size_t i = 1; i <= fullData->size(); ++i) {
        decoder = createDecoder();
        RefPtr<SharedBuffer> data = SharedBuffer::create(fullData->data(), i);
        decoder->setData(data.get(), i == fullData->size());

        if (decoder->isSizeAvailable() && !frameSize.width() && !frameSize.height()) {
            frameSize = decoder->decodedSize();
            continue;
        }

        ASSERT_EQ(frameSize.width(), decoder->decodedSize().width());
        ASSERT_EQ(frameSize.height(), decoder->decodedSize().height());
    }
}

TEST(GIFImageDecoderTest, verifyRepetitionCount)
{
    const int expectedRepetitionCount = 2;
    std::unique_ptr<ImageDecoder> decoder = createDecoder();
    RefPtr<SharedBuffer> data = readFile(layoutTestResourcesDir, "full2loop.gif");
    ASSERT_TRUE(data.get());
    decoder->setData(data.get(), true);
    EXPECT_EQ(cAnimationLoopOnce, decoder->repetitionCount()); // Default value before decode.

    for (size_t i = 0; i < decoder->frameCount(); ++i) {
        ImageFrame* frame = decoder->frameBufferAtIndex(i);
        EXPECT_EQ(ImageFrame::FrameComplete, frame->getStatus());
    }

    EXPECT_EQ(expectedRepetitionCount, decoder->repetitionCount()); // Expected value after decode.
}

TEST(GIFImageDecoderTest, bitmapAlphaType)
{
    RefPtr<SharedBuffer> fullData = readFile(decodersTestingDir, "radient.gif");
    ASSERT_TRUE(fullData.get());

    // Empirically chosen truncation size:
    //   a) large enough to produce a partial frame &&
    //   b) small enough to not fully decode the frame
    const size_t kTruncateSize = 800;
    ASSERT_TRUE(kTruncateSize < fullData->size());
    RefPtr<SharedBuffer> partialData = SharedBuffer::create(fullData->data(), kTruncateSize);

    std::unique_ptr<ImageDecoder> premulDecoder = wrapUnique(new GIFImageDecoder(
        ImageDecoder::AlphaPremultiplied,
        ImageDecoder::GammaAndColorProfileApplied,
        ImageDecoder::noDecodedImageByteLimit));
    std::unique_ptr<ImageDecoder> unpremulDecoder = wrapUnique(new GIFImageDecoder(
        ImageDecoder::AlphaNotPremultiplied,
        ImageDecoder::GammaAndColorProfileApplied,
        ImageDecoder::noDecodedImageByteLimit));

    // Partially decoded frame => the frame alpha type is unknown and should reflect the requested format.
    premulDecoder->setData(partialData.get(), false);
    ASSERT_TRUE(premulDecoder->frameCount());
    unpremulDecoder->setData(partialData.get(), false);
    ASSERT_TRUE(unpremulDecoder->frameCount());
    ImageFrame* premulFrame = premulDecoder->frameBufferAtIndex(0);
    EXPECT_TRUE(premulFrame && premulFrame->getStatus() != ImageFrame::FrameComplete);
    EXPECT_EQ(premulFrame->bitmap().alphaType(), kPremul_SkAlphaType);
    ImageFrame* unpremulFrame = unpremulDecoder->frameBufferAtIndex(0);
    EXPECT_TRUE(unpremulFrame && unpremulFrame->getStatus() != ImageFrame::FrameComplete);
    EXPECT_EQ(unpremulFrame->bitmap().alphaType(), kUnpremul_SkAlphaType);

    // Fully decoded frame => the frame alpha type is known (opaque).
    premulDecoder->setData(fullData.get(), true);
    ASSERT_TRUE(premulDecoder->frameCount());
    unpremulDecoder->setData(fullData.get(), true);
    ASSERT_TRUE(unpremulDecoder->frameCount());
    premulFrame = premulDecoder->frameBufferAtIndex(0);
    EXPECT_TRUE(premulFrame && premulFrame->getStatus() == ImageFrame::FrameComplete);
    EXPECT_EQ(premulFrame->bitmap().alphaType(), kOpaque_SkAlphaType);
    unpremulFrame = unpremulDecoder->frameBufferAtIndex(0);
    EXPECT_TRUE(unpremulFrame && unpremulFrame->getStatus() == ImageFrame::FrameComplete);
    EXPECT_EQ(unpremulFrame->bitmap().alphaType(), kOpaque_SkAlphaType);
}

} // namespace blink
