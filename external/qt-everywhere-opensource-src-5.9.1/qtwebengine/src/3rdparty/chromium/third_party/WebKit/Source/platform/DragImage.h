/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef DragImage_h
#define DragImage_h

#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include <memory>

class SkImage;

namespace blink {

class FontDescription;
class Image;
class KURL;

class PLATFORM_EXPORT DragImage {
  USING_FAST_MALLOC(DragImage);
  WTF_MAKE_NONCOPYABLE(DragImage);

 public:
  static std::unique_ptr<DragImage> create(
      Image*,
      RespectImageOrientationEnum = DoNotRespectImageOrientation,
      float deviceScaleFactor = 1,
      InterpolationQuality = InterpolationHigh,
      float opacity = 1,
      FloatSize imageScale = FloatSize(1, 1));

  static std::unique_ptr<DragImage> create(const KURL&,
                                           const String& label,
                                           const FontDescription& systemFont,
                                           float deviceScaleFactor);
  ~DragImage();

  static FloatSize clampedImageScale(const IntSize&,
                                     const IntSize&,
                                     const IntSize& maxSize);

  const SkBitmap& bitmap() { return m_bitmap; }
  float resolutionScale() const { return m_resolutionScale; }
  IntSize size() const { return IntSize(m_bitmap.width(), m_bitmap.height()); }

  void scale(float scaleX, float scaleY);

  static sk_sp<SkImage> resizeAndOrientImage(
      sk_sp<SkImage>,
      ImageOrientation,
      FloatSize imageScale = FloatSize(1, 1),
      float opacity = 1.0,
      InterpolationQuality = InterpolationNone);

 private:
  DragImage(const SkBitmap&, float resolutionScale, InterpolationQuality);

  SkBitmap m_bitmap;
  float m_resolutionScale;
  InterpolationQuality m_interpolationQuality;
};

}  // namespace blink

#endif  // DragImage_h
