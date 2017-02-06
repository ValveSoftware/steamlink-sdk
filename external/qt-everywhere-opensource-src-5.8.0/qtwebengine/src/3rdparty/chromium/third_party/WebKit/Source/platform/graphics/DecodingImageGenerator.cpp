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

#include "platform/graphics/DecodingImageGenerator.h"

#include "platform/PlatformInstrumentation.h"
#include "platform/SharedBuffer.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/ImageFrameGenerator.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/SegmentReader.h"
#include "third_party/skia/include/core/SkData.h"
#include <memory>

namespace blink {

DecodingImageGenerator::DecodingImageGenerator(PassRefPtr<ImageFrameGenerator> frameGenerator, const SkImageInfo& info, PassRefPtr<SegmentReader> data, bool allDataReceived, size_t index, uint32_t uniqueID)
    : SkImageGenerator(info, uniqueID)
    , m_frameGenerator(frameGenerator)
    , m_data(data)
    , m_allDataReceived(allDataReceived)
    , m_frameIndex(index)
    , m_canYUVDecode(false)
{
}

DecodingImageGenerator::~DecodingImageGenerator()
{
}

SkData* DecodingImageGenerator::onRefEncodedData(GrContext* ctx)
{
    TRACE_EVENT0("blink", "DecodingImageGenerator::refEncodedData");

    // The GPU only wants the data if it has all been received, since the GPU
    // only wants a complete texture. getAsSkData() may require copying, so
    // skip it and just return nullptr to avoid a slowdown. (See
    // crbug.com/568016 for details about such a slowdown.)
    // TODO (scroggo): Stop relying on the internal knowledge of how Skia uses
    // this. skbug.com/5485
    if (ctx && !m_allDataReceived)
        return nullptr;

    // Other clients are serializers, which want the data even if it requires
    // copying, and even if the data is incomplete. (Otherwise they would
    // potentially need to decode the partial image in order to re-encode it.)
    return m_data->getAsSkData().leakRef();
}

bool DecodingImageGenerator::onGetPixels(const SkImageInfo& info, void* pixels, size_t rowBytes, SkPMColor table[], int* tableCount)
{
    TRACE_EVENT1("blink", "DecodingImageGenerator::getPixels", "frame index", static_cast<int>(m_frameIndex));

    // Implementation doesn't support scaling yet so make sure we're not given a different size.
    if (info.width() != getInfo().width() || info.height() != getInfo().height())
        return false;

    if (info.colorType() != getInfo().colorType()) {
        // blink::ImageFrame may have changed the owning SkBitmap to kOpaque_SkAlphaType after fully decoding the image frame,
        // so if we see a request for opaque, that is ok even if our initial alpha type was not opaque.
        return false;
    }

    PlatformInstrumentation::willDecodeLazyPixelRef(uniqueID());
    bool decoded = m_frameGenerator->decodeAndScale(m_data.get(), m_allDataReceived, m_frameIndex, getInfo(), pixels, rowBytes);
    PlatformInstrumentation::didDecodeLazyPixelRef();

    return decoded;
}

bool DecodingImageGenerator::onQueryYUV8(SkYUVSizeInfo* sizeInfo, SkYUVColorSpace* colorSpace) const
{
    // YUV decoding does not currently support progressive decoding. See comment in ImageFrameGenerator.h.
    if (!m_canYUVDecode || !m_allDataReceived)
        return false;

    TRACE_EVENT1("blink", "DecodingImageGenerator::queryYUV8", "sizes", static_cast<int>(m_frameIndex));

    if (colorSpace)
        *colorSpace = kJPEG_SkYUVColorSpace;

    return m_frameGenerator->getYUVComponentSizes(m_data.get(), sizeInfo);
}

bool DecodingImageGenerator::onGetYUV8Planes(const SkYUVSizeInfo& sizeInfo, void* planes[3])
{
    // YUV decoding does not currently support progressive decoding. See comment in ImageFrameGenerator.h.
    ASSERT(m_canYUVDecode && m_allDataReceived);

    TRACE_EVENT1("blink", "DecodingImageGenerator::getYUV8Planes", "frame index", static_cast<int>(m_frameIndex));

    PlatformInstrumentation::willDecodeLazyPixelRef(uniqueID());
    bool decoded = m_frameGenerator->decodeToYUV(m_data.get(), m_frameIndex, sizeInfo.fSizes, planes, sizeInfo.fWidthBytes);
    PlatformInstrumentation::didDecodeLazyPixelRef();

    return decoded;
}

SkImageGenerator* DecodingImageGenerator::create(SkData* data)
{
    // We just need the size of the image, so we have to temporarily create an ImageDecoder. Since
    // we only need the size, it doesn't really matter about premul or not, or gamma settings.
    std::unique_ptr<ImageDecoder> decoder = ImageDecoder::create(static_cast<const char*>(data->data()), data->size(),
        ImageDecoder::AlphaPremultiplied, ImageDecoder::GammaAndColorProfileApplied);
    if (!decoder)
        return 0;

    // Blink does not know Skia has already adopted |data|.
    WTF::adopted(data);
    RefPtr<SegmentReader> segmentReader = SegmentReader::createFromSkData(data);
    decoder->setData(segmentReader.get(), true);
    if (!decoder->isSizeAvailable())
        return 0;

    const IntSize size = decoder->size();
    const SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());

    RefPtr<ImageFrameGenerator> frame = ImageFrameGenerator::create(SkISize::Make(size.width(), size.height()), false);
    if (!frame)
        return 0;

    return new DecodingImageGenerator(frame, info, segmentReader.release(), true, 0);
}

} // namespace blink
