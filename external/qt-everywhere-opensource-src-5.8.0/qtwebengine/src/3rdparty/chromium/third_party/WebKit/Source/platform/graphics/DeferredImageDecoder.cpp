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

#include "platform/graphics/DeferredImageDecoder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/graphics/DecodingImageGenerator.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/graphics/ImageFrameGenerator.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-decoders/SegmentReader.h"
#include "third_party/skia/include/core/SkImage.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

struct DeferredFrameData {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    WTF_MAKE_NONCOPYABLE(DeferredFrameData);
public:
    DeferredFrameData()
        : m_orientation(DefaultImageOrientation)
        , m_duration(0)
        , m_isComplete(false)
        , m_frameBytes(0)
        , m_uniqueID(DecodingImageGenerator::kNeedNewImageUniqueID)
    {}

    ImageOrientation m_orientation;
    float m_duration;
    bool m_isComplete;
    size_t m_frameBytes;
    uint32_t m_uniqueID;
};

std::unique_ptr<DeferredImageDecoder> DeferredImageDecoder::create(const SharedBuffer& data, ImageDecoder::AlphaOption alphaOption, ImageDecoder::GammaAndColorProfileOption colorOptions)
{
    std::unique_ptr<ImageDecoder> actualDecoder = ImageDecoder::create(data, alphaOption, colorOptions);

    if (!actualDecoder)
        return nullptr;

    return wrapUnique(new DeferredImageDecoder(std::move(actualDecoder)));
}

std::unique_ptr<DeferredImageDecoder> DeferredImageDecoder::createForTesting(std::unique_ptr<ImageDecoder> actualDecoder)
{
    return wrapUnique(new DeferredImageDecoder(std::move(actualDecoder)));
}

DeferredImageDecoder::DeferredImageDecoder(std::unique_ptr<ImageDecoder> actualDecoder)
    : m_allDataReceived(false)
    , m_actualDecoder(std::move(actualDecoder))
    , m_repetitionCount(cAnimationNone)
    , m_hasColorProfile(false)
    , m_canYUVDecode(false)
    , m_hasHotSpot(false)
{
}

DeferredImageDecoder::~DeferredImageDecoder()
{
}

String DeferredImageDecoder::filenameExtension() const
{
    return m_actualDecoder ? m_actualDecoder->filenameExtension() : m_filenameExtension;
}

PassRefPtr<SkImage> DeferredImageDecoder::createFrameAtIndex(size_t index)
{
    if (m_frameGenerator && m_frameGenerator->decodeFailed())
        return nullptr;

    prepareLazyDecodedFrames();

    if (index < m_frameData.size()) {
        DeferredFrameData* frameData = &m_frameData[index];
        if (m_actualDecoder)
            frameData->m_frameBytes = m_actualDecoder->frameBytesAtIndex(index);
        else
            frameData->m_frameBytes = m_size.area() * sizeof(ImageFrame::PixelData);
        // ImageFrameGenerator has the latest known alpha state. There will be a
        // performance boost if this frame is opaque.
        DCHECK(m_frameGenerator);
        return createFrameImageAtIndex(index, !m_frameGenerator->hasAlpha(index));
    }

    if (!m_actualDecoder || m_actualDecoder->failed())
        return nullptr;

    ImageFrame* frame = m_actualDecoder->frameBufferAtIndex(index);
    if (!frame || frame->getStatus() == ImageFrame::FrameEmpty)
        return nullptr;

    return fromSkSp(SkImage::MakeFromBitmap(frame->bitmap()));
}

void DeferredImageDecoder::setData(SharedBuffer& data, bool allDataReceived)
{
    if (m_actualDecoder) {
        m_allDataReceived = allDataReceived;
        m_actualDecoder->setData(&data, allDataReceived);
        prepareLazyDecodedFrames();
    }

    if (m_frameGenerator) {
        if (!m_rwBuffer)
            m_rwBuffer = wrapUnique(new SkRWBuffer(data.size()));

        const char* segment = 0;
        for (size_t length = data.getSomeData(segment, m_rwBuffer->size());
            length; length = data.getSomeData(segment, m_rwBuffer->size()))
            m_rwBuffer->append(segment, length);
    }
}

bool DeferredImageDecoder::isSizeAvailable()
{
    // m_actualDecoder is 0 only if image decoding is deferred and that means
    // the image header decoded successfully and the size is available.
    return m_actualDecoder ? m_actualDecoder->isSizeAvailable() : true;
}

bool DeferredImageDecoder::hasColorProfile() const
{
    return m_actualDecoder ? m_actualDecoder->hasColorProfile() : m_hasColorProfile;
}

IntSize DeferredImageDecoder::size() const
{
    return m_actualDecoder ? m_actualDecoder->size() : m_size;
}

IntSize DeferredImageDecoder::frameSizeAtIndex(size_t index) const
{
    // FIXME: LocalFrame size is assumed to be uniform. This might not be true for
    // future supported codecs.
    return m_actualDecoder ? m_actualDecoder->frameSizeAtIndex(index) : m_size;
}

size_t DeferredImageDecoder::frameCount()
{
    return m_actualDecoder ? m_actualDecoder->frameCount() : m_frameData.size();
}

int DeferredImageDecoder::repetitionCount() const
{
    return m_actualDecoder ? m_actualDecoder->repetitionCount() : m_repetitionCount;
}

size_t DeferredImageDecoder::clearCacheExceptFrame(size_t clearExceptFrame)
{
    if (m_actualDecoder)
        return m_actualDecoder->clearCacheExceptFrame(clearExceptFrame);
    size_t frameBytesCleared = 0;
    for (size_t i = 0; i < m_frameData.size(); ++i) {
        if (i != clearExceptFrame) {
            frameBytesCleared += m_frameData[i].m_frameBytes;
            m_frameData[i].m_frameBytes = 0;
        }
    }
    return frameBytesCleared;
}

bool DeferredImageDecoder::frameHasAlphaAtIndex(size_t index) const
{
    if (m_actualDecoder)
        return m_actualDecoder->frameHasAlphaAtIndex(index);
    if (!m_frameGenerator->isMultiFrame())
        return m_frameGenerator->hasAlpha(index);
    return true;
}

bool DeferredImageDecoder::frameIsCompleteAtIndex(size_t index) const
{
    if (m_actualDecoder)
        return m_actualDecoder->frameIsCompleteAtIndex(index);
    if (index < m_frameData.size())
        return m_frameData[index].m_isComplete;
    return false;
}

float DeferredImageDecoder::frameDurationAtIndex(size_t index) const
{
    if (m_actualDecoder)
        return m_actualDecoder->frameDurationAtIndex(index);
    if (index < m_frameData.size())
        return m_frameData[index].m_duration;
    return 0;
}

size_t DeferredImageDecoder::frameBytesAtIndex(size_t index) const
{
    if (m_actualDecoder)
        return m_actualDecoder->frameBytesAtIndex(index);
    if (index < m_frameData.size())
        return m_frameData[index].m_frameBytes;
    return 0;
}

ImageOrientation DeferredImageDecoder::orientationAtIndex(size_t index) const
{
    if (m_actualDecoder)
        return m_actualDecoder->orientation();
    if (index < m_frameData.size())
        return m_frameData[index].m_orientation;
    return DefaultImageOrientation;
}

void DeferredImageDecoder::activateLazyDecoding()
{
    if (m_frameGenerator)
        return;

    m_size = m_actualDecoder->size();
    m_hasHotSpot = m_actualDecoder->hotSpot(m_hotSpot);
    m_filenameExtension = m_actualDecoder->filenameExtension();
    // JPEG images support YUV decoding: other decoders do not, WEBP could in future.
    m_canYUVDecode = RuntimeEnabledFeatures::decodeToYUVEnabled() && (m_filenameExtension == "jpg");
    m_hasColorProfile = m_actualDecoder->hasColorProfile();

    const bool isSingleFrame = m_actualDecoder->repetitionCount() == cAnimationNone || (m_allDataReceived && m_actualDecoder->frameCount() == 1u);
    const SkISize decodedSize = SkISize::Make(m_actualDecoder->decodedSize().width(), m_actualDecoder->decodedSize().height());
    m_frameGenerator = ImageFrameGenerator::create(decodedSize, !isSingleFrame);
}

void DeferredImageDecoder::prepareLazyDecodedFrames()
{
    if (!m_actualDecoder
        || !m_actualDecoder->isSizeAvailable())
        return;

    activateLazyDecoding();

    const size_t previousSize = m_frameData.size();
    m_frameData.resize(m_actualDecoder->frameCount());

    // We have encountered a broken image file. Simply bail.
    if (m_frameData.size() < previousSize)
        return;

    for (size_t i = previousSize; i < m_frameData.size(); ++i) {
        m_frameData[i].m_duration = m_actualDecoder->frameDurationAtIndex(i);
        m_frameData[i].m_orientation = m_actualDecoder->orientation();
        m_frameData[i].m_isComplete = m_actualDecoder->frameIsCompleteAtIndex(i);
    }

    // The last lazy decoded frame created from previous call might be
    // incomplete so update its state.
    if (previousSize) {
        const size_t lastFrame = previousSize - 1;
        m_frameData[lastFrame].m_isComplete = m_actualDecoder->frameIsCompleteAtIndex(lastFrame);
    }

    if (m_allDataReceived) {
        m_repetitionCount = m_actualDecoder->repetitionCount();
        m_actualDecoder.reset();
        // Hold on to m_rwBuffer, which is still needed by createFrameAtIndex.
    }
}

inline SkImageInfo imageInfoFrom(const SkISize& decodedSize, bool knownToBeOpaque)
{
    return SkImageInfo::MakeN32(decodedSize.width(), decodedSize.height(), knownToBeOpaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType);
}

PassRefPtr<SkImage> DeferredImageDecoder::createFrameImageAtIndex(size_t index, bool knownToBeOpaque)
{
    const SkISize& decodedSize = m_frameGenerator->getFullSize();
    ASSERT(decodedSize.width() > 0);
    ASSERT(decodedSize.height() > 0);

    RefPtr<SkROBuffer> roBuffer = adoptRef(m_rwBuffer->newRBufferSnapshot());
    RefPtr<SegmentReader> segmentReader = SegmentReader::createFromSkROBuffer(roBuffer.release());
    DecodingImageGenerator* generator = new DecodingImageGenerator(m_frameGenerator, imageInfoFrom(decodedSize, knownToBeOpaque), segmentReader.release(), m_allDataReceived, index, m_frameData[index].m_uniqueID);
    RefPtr<SkImage> image = fromSkSp(SkImage::MakeFromGenerator(generator)); // SkImage takes ownership of the generator.
    if (!image)
        return nullptr;

    // We can consider decoded bitmap constant and reuse uniqueID only after all
    // data is received.  We reuse it also for multiframe images when image data
    // is partially received but the frame data is fully received.
    if (m_allDataReceived || m_frameData[index].m_isComplete) {
        DCHECK(m_frameData[index].m_uniqueID == DecodingImageGenerator::kNeedNewImageUniqueID || m_frameData[index].m_uniqueID == image->uniqueID());
        m_frameData[index].m_uniqueID = image->uniqueID();
    }

    generator->setCanYUVDecode(m_canYUVDecode);

    return image.release();
}

bool DeferredImageDecoder::hotSpot(IntPoint& hotSpot) const
{
    if (m_actualDecoder)
        return m_actualDecoder->hotSpot(hotSpot);
    if (m_hasHotSpot)
        hotSpot = m_hotSpot;
    return m_hasHotSpot;
}

} // namespace blink

namespace WTF {
template<> struct VectorTraits<blink::DeferredFrameData> : public SimpleClassVectorTraits<blink::DeferredFrameData> {
    STATIC_ONLY(VectorTraits);
    static const bool canInitializeWithMemset = false; // Not all DeferredFrameData members initialize to 0.
};
}
