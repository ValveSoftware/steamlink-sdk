/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/ImageFrameGenerator.h"

#include "SkData.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "third_party/skia/include/core/SkYUVSizeInfo.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

static bool compatibleInfo(const SkImageInfo& src, const SkImageInfo& dst)
{
    if (src == dst)
        return true;

    // It is legal to write kOpaque_SkAlphaType pixels into a kPremul_SkAlphaType buffer.
    // This can happen when DeferredImageDecoder allocates an kOpaque_SkAlphaType image
    // generator based on cached frame info, while the ImageFrame-allocated dest bitmap
    // stays kPremul_SkAlphaType.
    if (src.alphaType() == kOpaque_SkAlphaType && dst.alphaType() == kPremul_SkAlphaType) {
        const SkImageInfo& tmp = src.makeAlphaType(kPremul_SkAlphaType);
        return tmp == dst;
    }

    return false;
}

// Creates a SkPixelRef such that the memory for pixels is given by an external body.
// This is used to write directly to the memory given by Skia during decoding.
class ExternalMemoryAllocator final : public SkBitmap::Allocator {
    USING_FAST_MALLOC(ExternalMemoryAllocator);
    WTF_MAKE_NONCOPYABLE(ExternalMemoryAllocator);
public:
    ExternalMemoryAllocator(const SkImageInfo& info, void* pixels, size_t rowBytes)
        : m_info(info)
        , m_pixels(pixels)
        , m_rowBytes(rowBytes)
    {
    }

    bool allocPixelRef(SkBitmap* dst, SkColorTable* ctable) override
    {
        const SkImageInfo& info = dst->info();
        if (kUnknown_SkColorType == info.colorType())
            return false;

        if (!compatibleInfo(m_info, info) || m_rowBytes != dst->rowBytes())
            return false;

        if (!dst->installPixels(info, m_pixels, m_rowBytes))
            return false;
        dst->lockPixels();
        return true;
    }

private:
    SkImageInfo m_info;
    void* m_pixels;
    size_t m_rowBytes;
};

static bool updateYUVComponentSizes(ImageDecoder* decoder, SkISize componentSizes[3], size_t componentWidthBytes[3])
{
    if (!decoder->canDecodeToYUV())
        return false;

    IntSize size = decoder->decodedYUVSize(0);
    componentSizes[0].set(size.width(), size.height());
    componentWidthBytes[0] = decoder->decodedYUVWidthBytes(0);
    size = decoder->decodedYUVSize(1);
    componentSizes[1].set(size.width(), size.height());
    componentWidthBytes[1] = decoder->decodedYUVWidthBytes(1);
    size = decoder->decodedYUVSize(2);
    componentSizes[2].set(size.width(), size.height());
    componentWidthBytes[2] = decoder->decodedYUVWidthBytes(2);
    return true;
}

ImageFrameGenerator::ImageFrameGenerator(const SkISize& fullSize, bool isMultiFrame)
    : m_fullSize(fullSize)
    , m_isMultiFrame(isMultiFrame)
    , m_decodeFailed(false)
    , m_yuvDecodingFailed(false)
    , m_frameCount(0)
{
}

ImageFrameGenerator::~ImageFrameGenerator()
{
    ImageDecodingStore::instance().removeCacheIndexedByGenerator(this);
}

bool ImageFrameGenerator::decodeAndScale(SegmentReader* data, bool allDataReceived, size_t index, const SkImageInfo& info, void* pixels, size_t rowBytes)
{
    if (m_decodeFailed)
        return false;

    TRACE_EVENT1("blink", "ImageFrameGenerator::decodeAndScale", "frame index", static_cast<int>(index));

    RefPtr<ExternalMemoryAllocator> externalAllocator = adoptRef(new ExternalMemoryAllocator(info, pixels, rowBytes));

    // This implementation does not support scaling so check the requested size.
    SkISize scaledSize = SkISize::Make(info.width(), info.height());
    ASSERT(m_fullSize == scaledSize);

    // TODO (scroggo): Convert tryToResumeDecode() and decode() to take a
    // PassRefPtr<SkBitmap::Allocator> instead of a bare pointer.
    SkBitmap bitmap = tryToResumeDecode(data, allDataReceived, index, scaledSize, externalAllocator.get());
    if (bitmap.isNull())
        return false;

    // Check to see if the decoder has written directly to the pixel memory
    // provided. If not, make a copy.
    ASSERT(bitmap.width() == scaledSize.width());
    ASSERT(bitmap.height() == scaledSize.height());
    SkAutoLockPixels bitmapLock(bitmap);
    if (bitmap.getPixels() != pixels)
        return bitmap.copyPixelsTo(pixels, rowBytes * info.height(), rowBytes);
    return true;
}

bool ImageFrameGenerator::decodeToYUV(SegmentReader* data, size_t index, const SkISize componentSizes[3], void* planes[3], const size_t rowBytes[3])
{
    // TODO (scroggo): The only interesting thing this uses from the ImageFrameGenerator is m_decodeFailed.
    // Move this into DecodingImageGenerator, which is the only class that calls it.
    if (m_decodeFailed)
        return false;

    TRACE_EVENT1("blink", "ImageFrameGenerator::decodeToYUV", "frame index", static_cast<int>(index));

    if (!planes || !planes[0] || !planes[1] || !planes[2]
        || !rowBytes || !rowBytes[0] || !rowBytes[1] || !rowBytes[2]) {
        return false;
    }

    std::unique_ptr<ImageDecoder> decoder = ImageDecoder::create(*data, ImageDecoder::AlphaPremultiplied, ImageDecoder::GammaAndColorProfileApplied);
    // getYUVComponentSizes was already called and was successful, so ImageDecoder::create must succeed.
    ASSERT(decoder);

    decoder->setData(data, true);

    std::unique_ptr<ImagePlanes> imagePlanes = wrapUnique(new ImagePlanes(planes, rowBytes));
    decoder->setImagePlanes(std::move(imagePlanes));

    ASSERT(decoder->canDecodeToYUV());

    if (decoder->decodeToYUV()) {
        setHasAlpha(0, false); // YUV is always opaque
        return true;
    }

    ASSERT(decoder->failed());
    m_yuvDecodingFailed = true;
    return false;
}

SkBitmap ImageFrameGenerator::tryToResumeDecode(SegmentReader* data, bool allDataReceived, size_t index, const SkISize& scaledSize, SkBitmap::Allocator* allocator)
{
    TRACE_EVENT1("blink", "ImageFrameGenerator::tryToResumeDecode", "frame index", static_cast<int>(index));

    ImageDecoder* decoder = 0;

    // Lock the mutex, so only one thread can use the decoder at once.
    MutexLocker lock(m_decodeMutex);
    const bool resumeDecoding = ImageDecodingStore::instance().lockDecoder(this, m_fullSize, &decoder);
    ASSERT(!resumeDecoding || decoder);

    SkBitmap fullSizeImage;
    bool complete = decode(data, allDataReceived, index, &decoder, &fullSizeImage, allocator);

    if (!decoder)
        return SkBitmap();

    // If we are not resuming decoding that means the decoder is freshly
    // created and we have ownership. If we are resuming decoding then
    // the decoder is owned by ImageDecodingStore.
    std::unique_ptr<ImageDecoder> decoderContainer;
    if (!resumeDecoding)
        decoderContainer = wrapUnique(decoder);

    if (fullSizeImage.isNull()) {
        // If decoding has failed, we can save work in the future by
        // ignoring further requests to decode the image.
        m_decodeFailed = decoder->failed();
        if (resumeDecoding)
            ImageDecodingStore::instance().unlockDecoder(this, decoder);
        return SkBitmap();
    }

    bool removeDecoder = false;
    if (complete) {
        // Free as much memory as possible.  For single-frame images, we can
        // just delete the decoder entirely.  For multi-frame images, we keep
        // the decoder around in order to preserve decoded information such as
        // the required previous frame indexes, but if we've reached the last
        // frame we can at least delete all the cached frames.  (If we were to
        // do this before reaching the last frame, any subsequent requested
        // frames which relied on the current frame would trigger extra
        // re-decoding of all frames in the dependency chain.)
        if (!m_isMultiFrame)
            removeDecoder = true;
        else if (index == m_frameCount - 1)
            decoder->clearCacheExceptFrame(kNotFound);
    }

    if (resumeDecoding) {
        if (removeDecoder)
            ImageDecodingStore::instance().removeDecoder(this, decoder);
        else
            ImageDecodingStore::instance().unlockDecoder(this, decoder);
    } else if (!removeDecoder) {
        ImageDecodingStore::instance().insertDecoder(this, std::move(decoderContainer));
    }
    return fullSizeImage;
}

void ImageFrameGenerator::setHasAlpha(size_t index, bool hasAlpha)
{
    MutexLocker lock(m_alphaMutex);
    if (index >= m_hasAlpha.size()) {
        const size_t oldSize = m_hasAlpha.size();
        m_hasAlpha.resize(index + 1);
        for (size_t i = oldSize; i < m_hasAlpha.size(); ++i)
            m_hasAlpha[i] = true;
    }
    m_hasAlpha[index] = hasAlpha;
}

bool ImageFrameGenerator::decode(SegmentReader* data, bool allDataReceived, size_t index, ImageDecoder** decoder, SkBitmap* bitmap, SkBitmap::Allocator* allocator)
{
    ASSERT(m_decodeMutex.locked());
    TRACE_EVENT2("blink", "ImageFrameGenerator::decode", "width", m_fullSize.width(), "height", m_fullSize.height());

    // Try to create an ImageDecoder if we are not given one.
    ASSERT(decoder);
    bool newDecoder = false;
    if (!*decoder) {
        newDecoder = true;
        if (m_imageDecoderFactory)
            *decoder = m_imageDecoderFactory->create().release();

        if (!*decoder)
            *decoder = ImageDecoder::create(*data, ImageDecoder::AlphaPremultiplied, ImageDecoder::GammaAndColorProfileApplied).release();

        if (!*decoder)
            return false;
    }

    if (!m_isMultiFrame && newDecoder && allDataReceived) {
        // If we're using an external memory allocator that means we're decoding
        // directly into the output memory and we can save one memcpy.
        ASSERT(allocator);
        (*decoder)->setMemoryAllocator(allocator);
    }

    (*decoder)->setData(data, allDataReceived);
    ImageFrame* frame = (*decoder)->frameBufferAtIndex(index);

    // For multi-frame image decoders, we need to know how many frames are
    // in that image in order to release the decoder when all frames are
    // decoded. frameCount() is reliable only if all data is received and set in
    // decoder, particularly with GIF.
    if (allDataReceived)
        m_frameCount = (*decoder)->frameCount();

    (*decoder)->setData(PassRefPtr<SegmentReader>(nullptr), false); // Unref SegmentReader from ImageDecoder.
    (*decoder)->clearCacheExceptFrame(index);
    (*decoder)->setMemoryAllocator(0);

    if (!frame || frame->getStatus() == ImageFrame::FrameEmpty)
        return false;

    // A cache object is considered complete if we can decode a complete frame.
    // Or we have received all data. The image might not be fully decoded in
    // the latter case.
    const bool isDecodeComplete = frame->getStatus() == ImageFrame::FrameComplete || allDataReceived;

    SkBitmap fullSizeBitmap = frame->bitmap();
    if (!fullSizeBitmap.isNull()) {
        ASSERT(fullSizeBitmap.width() == m_fullSize.width() && fullSizeBitmap.height() == m_fullSize.height());
        setHasAlpha(index, !fullSizeBitmap.isOpaque());
    }

    *bitmap = fullSizeBitmap;
    return isDecodeComplete;
}

bool ImageFrameGenerator::hasAlpha(size_t index)
{
    MutexLocker lock(m_alphaMutex);
    if (index < m_hasAlpha.size())
        return m_hasAlpha[index];
    return true;
}

bool ImageFrameGenerator::getYUVComponentSizes(SegmentReader* data, SkYUVSizeInfo* sizeInfo)
{
    TRACE_EVENT2("blink", "ImageFrameGenerator::getYUVComponentSizes", "width", m_fullSize.width(), "height", m_fullSize.height());

    if (m_yuvDecodingFailed)
        return false;

    std::unique_ptr<ImageDecoder> decoder = ImageDecoder::create(*data, ImageDecoder::AlphaPremultiplied, ImageDecoder::GammaAndColorProfileApplied);
    if (!decoder)
        return false;

    // Setting a dummy ImagePlanes object signals to the decoder that we want to do YUV decoding.
    decoder->setData(data, true);
    std::unique_ptr<ImagePlanes> dummyImagePlanes = wrapUnique(new ImagePlanes);
    decoder->setImagePlanes(std::move(dummyImagePlanes));

    return updateYUVComponentSizes(decoder.get(), sizeInfo->fSizes, sizeInfo->fWidthBytes);
}

} // namespace blink
