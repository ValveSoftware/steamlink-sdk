/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef DecodingImageGenerator_h
#define DecodingImageGenerator_h

#include "platform/PlatformExport.h"
#include "platform/image-decoders/SegmentReader.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

class SkData;

namespace blink {

class ImageFrameGenerator;

// Implements SkImageGenerator, used by SkPixelRef to populate a discardable
// memory with a decoded image frame. ImageFrameGenerator does the actual
// decoding.
class PLATFORM_EXPORT DecodingImageGenerator final : public SkImageGenerator {
  USING_FAST_MALLOC(DecodingImageGenerator);
  WTF_MAKE_NONCOPYABLE(DecodingImageGenerator);

 public:
  // Make SkImageGenerator::kNeedNewImageUniqueID accessible.
  enum { kNeedNewImageUniqueID = SkImageGenerator::kNeedNewImageUniqueID };

  static SkImageGenerator* create(SkData*);

  DecodingImageGenerator(PassRefPtr<ImageFrameGenerator>,
                         const SkImageInfo&,
                         PassRefPtr<SegmentReader>,
                         bool allDataReceived,
                         size_t index,
                         uint32_t uniqueID = kNeedNewImageUniqueID);
  ~DecodingImageGenerator() override;

  void setCanYUVDecode(bool yes) { m_canYUVDecode = yes; }

 protected:
  SkData* onRefEncodedData(GrContext* ctx) override;

  bool onGetPixels(const SkImageInfo&,
                   void* pixels,
                   size_t rowBytes,
                   SkPMColor table[],
                   int* tableCount) override;

  bool onQueryYUV8(SkYUVSizeInfo*, SkYUVColorSpace*) const override;

  bool onGetYUV8Planes(const SkYUVSizeInfo&, void* planes[3]) override;

 private:
  RefPtr<ImageFrameGenerator> m_frameGenerator;
  const RefPtr<SegmentReader> m_data;  // Data source.
  const bool m_allDataReceived;
  const size_t m_frameIndex;
  bool m_canYUVDecode;
};

}  // namespace blink

#endif  // DecodingImageGenerator_h_
