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

#ifndef ImageFrameGenerator_h
#define ImageFrameGenerator_h

#include "platform/PlatformExport.h"
#include "platform/image-decoders/SegmentReader.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"
#include <memory>

class SkData;
struct SkYUVSizeInfo;

namespace blink {

class ImageDecoder;

class PLATFORM_EXPORT ImageDecoderFactory {
    USING_FAST_MALLOC(ImageDecoderFactory);
    WTF_MAKE_NONCOPYABLE(ImageDecoderFactory);
public:
    ImageDecoderFactory() {}
    virtual ~ImageDecoderFactory() { }
    virtual std::unique_ptr<ImageDecoder> create() = 0;
};

class PLATFORM_EXPORT ImageFrameGenerator final : public ThreadSafeRefCounted<ImageFrameGenerator> {
    WTF_MAKE_NONCOPYABLE(ImageFrameGenerator);
public:
    static PassRefPtr<ImageFrameGenerator> create(const SkISize& fullSize, bool isMultiFrame = false)
    {
        return adoptRef(new ImageFrameGenerator(fullSize, isMultiFrame));
    }

    ~ImageFrameGenerator();

    // Decodes and scales the specified frame at |index|. The dimensions and output
    // format are given in SkImageInfo. Decoded pixels are written into |pixels| with
    // a stride of |rowBytes|. Returns true if decoding was successful.
    bool decodeAndScale(SegmentReader*, bool allDataReceived, size_t index, const SkImageInfo&, void* pixels, size_t rowBytes);

    // Decodes YUV components directly into the provided memory planes.
    // Must not be called unless getYUVComponentSizes has been called and returned true.
    // YUV decoding does not currently support progressive decoding. In order to support it, ImageDecoder needs something
    // analagous to its ImageFrame cache to hold partial planes, and the GPU code needs to handle them.
    bool decodeToYUV(SegmentReader*, size_t index, const SkISize componentSizes[3], void* planes[3], const size_t rowBytes[3]);

    const SkISize& getFullSize() const { return m_fullSize; }

    bool isMultiFrame() const { return m_isMultiFrame; }
    bool decodeFailed() const { return m_decodeFailed; }

    bool hasAlpha(size_t index);

    // Must not be called unless the SkROBuffer has all the data.
    // YUV decoding does not currently support progressive decoding. See comment above on decodeToYUV.
    bool getYUVComponentSizes(SegmentReader*, SkYUVSizeInfo*);

private:
    ImageFrameGenerator(const SkISize& fullSize, bool isMultiFrame);

    friend class ImageFrameGeneratorTest;
    friend class DeferredImageDecoderTest;
    // For testing. |factory| will overwrite the default ImageDecoder creation logic if |factory->create()| returns non-zero.
    void setImageDecoderFactory(std::unique_ptr<ImageDecoderFactory> factory) { m_imageDecoderFactory = std::move(factory); }

    void setHasAlpha(size_t index, bool hasAlpha);

    SkBitmap tryToResumeDecode(SegmentReader*, bool allDataReceived, size_t index, const SkISize& scaledSize, SkBitmap::Allocator*);
    // This method should only be called while m_decodeMutex is locked.
    bool decode(SegmentReader*, bool allDataReceived, size_t index, ImageDecoder**, SkBitmap*, SkBitmap::Allocator*);

    const SkISize m_fullSize;

    const bool m_isMultiFrame;
    bool m_decodeFailed;
    bool m_yuvDecodingFailed;
    size_t m_frameCount;
    Vector<bool> m_hasAlpha;

    std::unique_ptr<ImageDecoderFactory> m_imageDecoderFactory;

    // Prevents multiple decode operations on the same data.
    Mutex m_decodeMutex;

    // Protect concurrent access to m_hasAlpha.
    Mutex m_alphaMutex;
};

} // namespace blink

#endif
