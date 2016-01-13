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

#include "config.h"

#include "platform/graphics/ImageFrameGenerator.h"

#include "platform/SharedBuffer.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/DiscardablePixelRef.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/graphics/ScaledImageFragment.h"
#include "platform/image-decoders/ImageDecoder.h"

#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"

namespace WebCore {

// Creates a SkPixelRef such that the memory for pixels is given by an external body.
// This is used to write directly to the memory given by Skia during decoding.
class ImageFrameGenerator::ExternalMemoryAllocator : public SkBitmap::Allocator {
public:
    ExternalMemoryAllocator(const SkImageInfo& info, void* pixels, size_t rowBytes)
        : m_info(info)
        , m_pixels(pixels)
        , m_rowBytes(rowBytes)
    {
    }

    virtual bool allocPixelRef(SkBitmap* dst, SkColorTable* ctable) OVERRIDE
    {
        const SkImageInfo& info = dst->info();
        if (kUnknown_SkColorType == info.colorType())
            return false;

        if (info != m_info || m_rowBytes != dst->rowBytes())
            return false;

        if (!dst->installPixels(m_info, m_pixels, m_rowBytes))
            return false;
        dst->lockPixels();
        return true;
    }

private:
    SkImageInfo m_info;
    void* m_pixels;
    size_t m_rowBytes;
};

ImageFrameGenerator::ImageFrameGenerator(const SkISize& fullSize, PassRefPtr<SharedBuffer> data, bool allDataReceived, bool isMultiFrame)
    : m_fullSize(fullSize)
    , m_isMultiFrame(isMultiFrame)
    , m_decodeFailedAndEmpty(false)
    , m_decodeCount(ScaledImageFragment::FirstPartialImage)
    , m_discardableAllocator(adoptPtr(new DiscardablePixelRefAllocator()))
{
    setData(data.get(), allDataReceived);
}

ImageFrameGenerator::~ImageFrameGenerator()
{
    ImageDecodingStore::instance()->removeCacheIndexedByGenerator(this);
}

void ImageFrameGenerator::setData(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    m_data.setData(data.get(), allDataReceived);
}

void ImageFrameGenerator::copyData(RefPtr<SharedBuffer>* data, bool* allDataReceived)
{
    SharedBuffer* buffer = 0;
    m_data.data(&buffer, allDataReceived);
    if (buffer)
        *data = buffer->copy();
}

const ScaledImageFragment* ImageFrameGenerator::decodeAndScale(const SkISize& scaledSize, size_t index)
{
    // Prevents concurrent decode or scale operations on the same image data.
    // Multiple LazyDecodingPixelRefs can call this method at the same time.
    MutexLocker lock(m_decodeMutex);
    if (m_decodeFailedAndEmpty)
        return 0;

    const ScaledImageFragment* cachedImage = 0;

    cachedImage = tryToLockCompleteCache(scaledSize, index);
    if (cachedImage)
        return cachedImage;

    TRACE_EVENT2("webkit", "ImageFrameGenerator::decodeAndScale", "generator", this, "decodeCount", static_cast<int>(m_decodeCount));

    cachedImage = tryToResumeDecode(scaledSize, index);
    if (cachedImage)
        return cachedImage;
    return 0;
}

bool ImageFrameGenerator::decodeAndScale(const SkImageInfo& info, size_t index, void* pixels, size_t rowBytes)
{
    // This method is called to populate a discardable memory owned by Skia.

    // Prevents concurrent decode or scale operations on the same image data.
    MutexLocker lock(m_decodeMutex);

    // This implementation does not support scaling so check the requested size.
    SkISize scaledSize = SkISize::Make(info.fWidth, info.fHeight);
    ASSERT(m_fullSize == scaledSize);

    if (m_decodeFailedAndEmpty)
        return 0;

    TRACE_EVENT2("webkit", "ImageFrameGenerator::decodeAndScale", "generator", this, "decodeCount", static_cast<int>(m_decodeCount));

    // Don't use discardable memory for decoding if Skia is providing output
    // memory. Instead use ExternalMemoryAllocator such that we can
    // write directly to the memory given by Skia.
    //
    // TODO:
    // This is not pretty because this class is used in two different code
    // paths: discardable memory decoding on Android and discardable memory
    // in Skia. Once the transition to caching in Skia is complete we can get
    // rid of the logic that handles discardable memory.
    m_discardableAllocator.clear();
    m_externalAllocator = adoptPtr(new ExternalMemoryAllocator(info, pixels, rowBytes));

    const ScaledImageFragment* cachedImage = tryToResumeDecode(scaledSize, index);
    if (!cachedImage)
        return false;

    // Don't keep the allocator because it contains a pointer to memory
    // that we do not own.
    m_externalAllocator.clear();

    ASSERT(cachedImage->bitmap().width() == scaledSize.width());
    ASSERT(cachedImage->bitmap().height() == scaledSize.height());

    bool result = true;
    // Check to see if decoder has written directly to the memory provided
    // by Skia. If not make a copy.
    if (cachedImage->bitmap().getPixels() != pixels)
        result = cachedImage->bitmap().copyPixelsTo(pixels, rowBytes * info.fHeight, rowBytes);
    ImageDecodingStore::instance()->unlockCache(this, cachedImage);
    return result;
}

const ScaledImageFragment* ImageFrameGenerator::tryToLockCompleteCache(const SkISize& scaledSize, size_t index)
{
    const ScaledImageFragment* cachedImage = 0;
    if (ImageDecodingStore::instance()->lockCache(this, scaledSize, index, &cachedImage))
        return cachedImage;
    return 0;
}

const ScaledImageFragment* ImageFrameGenerator::tryToResumeDecode(const SkISize& scaledSize, size_t index)
{
    TRACE_EVENT1("webkit", "ImageFrameGenerator::tryToResumeDecodeAndScale", "index", static_cast<int>(index));

    ImageDecoder* decoder = 0;
    const bool resumeDecoding = ImageDecodingStore::instance()->lockDecoder(this, m_fullSize, &decoder);
    ASSERT(!resumeDecoding || decoder);

    OwnPtr<ScaledImageFragment> fullSizeImage = decode(index, &decoder);

    if (!decoder)
        return 0;

    // If we are not resuming decoding that means the decoder is freshly
    // created and we have ownership. If we are resuming decoding then
    // the decoder is owned by ImageDecodingStore.
    OwnPtr<ImageDecoder> decoderContainer;
    if (!resumeDecoding)
        decoderContainer = adoptPtr(decoder);

    if (!fullSizeImage) {
        // If decode has failed and resulted an empty image we can save work
        // in the future by returning early.
        m_decodeFailedAndEmpty = !m_isMultiFrame && decoder->failed();

        if (resumeDecoding)
            ImageDecodingStore::instance()->unlockDecoder(this, decoder);
        return 0;
    }

    const ScaledImageFragment* cachedImage = ImageDecodingStore::instance()->insertAndLockCache(this, fullSizeImage.release());

    // If the image generated is complete then there is no need to keep
    // the decoder. The exception is multi-frame decoder which can generate
    // multiple complete frames.
    const bool removeDecoder = cachedImage->isComplete() && !m_isMultiFrame;

    if (resumeDecoding) {
        if (removeDecoder)
            ImageDecodingStore::instance()->removeDecoder(this, decoder);
        else
            ImageDecodingStore::instance()->unlockDecoder(this, decoder);
    } else if (!removeDecoder) {
        ImageDecodingStore::instance()->insertDecoder(this, decoderContainer.release(), DiscardablePixelRef::isDiscardable(cachedImage->bitmap().pixelRef()));
    }

    return cachedImage;
}

PassOwnPtr<ScaledImageFragment> ImageFrameGenerator::decode(size_t index, ImageDecoder** decoder)
{
    TRACE_EVENT2("webkit", "ImageFrameGenerator::decode", "width", m_fullSize.width(), "height", m_fullSize.height());

    ASSERT(decoder);
    SharedBuffer* data = 0;
    bool allDataReceived = false;
    bool newDecoder = false;
    m_data.data(&data, &allDataReceived);

    // Try to create an ImageDecoder if we are not given one.
    if (!*decoder) {
        newDecoder = true;
        if (m_imageDecoderFactory)
            *decoder = m_imageDecoderFactory->create().leakPtr();

        if (!*decoder)
            *decoder = ImageDecoder::create(*data, ImageSource::AlphaPremultiplied, ImageSource::GammaAndColorProfileApplied).leakPtr();

        if (!*decoder)
            return nullptr;
    }

    // This variable is set to true if we can skip a memcpy of the decoded bitmap.
    bool canSkipBitmapCopy = false;

    if (!m_isMultiFrame && newDecoder && allDataReceived) {
        // If we're using an external memory allocator that means we're decoding
        // directly into the output memory and we can save one memcpy.
        canSkipBitmapCopy = true;
        if (m_externalAllocator)
            (*decoder)->setMemoryAllocator(m_externalAllocator.get());
        else
            (*decoder)->setMemoryAllocator(m_discardableAllocator.get());
    }
    (*decoder)->setData(data, allDataReceived);
    // If this call returns a newly allocated DiscardablePixelRef, then
    // ImageFrame::m_bitmap and the contained DiscardablePixelRef are locked.
    // They will be unlocked when ImageDecoder is destroyed since ImageDecoder
    // owns the ImageFrame. Partially decoded SkBitmap is thus inserted into the
    // ImageDecodingStore while locked.
    ImageFrame* frame = (*decoder)->frameBufferAtIndex(index);
    (*decoder)->setData(0, false); // Unref SharedBuffer from ImageDecoder.
    (*decoder)->clearCacheExceptFrame(index);
    (*decoder)->setMemoryAllocator(0);

    if (!frame || frame->status() == ImageFrame::FrameEmpty)
        return nullptr;

    // A cache object is considered complete if we can decode a complete frame.
    // Or we have received all data. The image might not be fully decoded in
    // the latter case.
    const bool isCacheComplete = frame->status() == ImageFrame::FrameComplete || allDataReceived;
    SkBitmap fullSizeBitmap = frame->getSkBitmap();
    if (fullSizeBitmap.isNull())
        return nullptr;

    {
        MutexLocker lock(m_alphaMutex);
        if (index >= m_hasAlpha.size()) {
            const size_t oldSize = m_hasAlpha.size();
            m_hasAlpha.resize(index + 1);
            for (size_t i = oldSize; i < m_hasAlpha.size(); ++i)
                m_hasAlpha[i] = true;
        }
        m_hasAlpha[index] = !fullSizeBitmap.isOpaque();
    }
    ASSERT(fullSizeBitmap.width() == m_fullSize.width() && fullSizeBitmap.height() == m_fullSize.height());

    // We early out and do not copy the memory if decoder writes directly to
    // the memory provided by Skia and the decode was complete.
    if (canSkipBitmapCopy && isCacheComplete)
        return ScaledImageFragment::createComplete(m_fullSize, index, fullSizeBitmap);

    // If the image is progressively decoded we need to return a copy.
    // This is to avoid future decode operations writing to the same bitmap.
    // FIXME: Note that discardable allocator is used. This is because the code
    // is still used in the Android discardable memory path. When this code is
    // used in the Skia discardable memory path |m_discardableAllocator| is empty.
    // This is confusing and should be cleaned up when we can deprecate the use
    // case for Android discardable memory.
    SkBitmap copyBitmap;
    if (!fullSizeBitmap.copyTo(&copyBitmap, fullSizeBitmap.colorType(), m_discardableAllocator.get()))
        return nullptr;

    if (isCacheComplete)
        return ScaledImageFragment::createComplete(m_fullSize, index, copyBitmap);
    return ScaledImageFragment::createPartial(m_fullSize, index, nextGenerationId(), copyBitmap);
}

bool ImageFrameGenerator::hasAlpha(size_t index)
{
    MutexLocker lock(m_alphaMutex);
    if (index < m_hasAlpha.size())
        return m_hasAlpha[index];
    return true;
}

} // namespace
