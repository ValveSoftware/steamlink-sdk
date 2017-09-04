// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnacceleratedStaticBitmapImage_h
#define UnacceleratedStaticBitmapImage_h

#include "platform/graphics/StaticBitmapImage.h"

namespace blink {

class PLATFORM_EXPORT UnacceleratedStaticBitmapImage final
    : public StaticBitmapImage {
 public:
  ~UnacceleratedStaticBitmapImage() override;
  static PassRefPtr<UnacceleratedStaticBitmapImage> create(sk_sp<SkImage>);

  bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) override;
  IntSize size() const override;
  sk_sp<SkImage> imageForCurrentFrame() override;
  // In our current design, the SkImage in this class is always *not*
  // texture-backed.
  bool isTextureBacked() { return false; }

  void draw(SkCanvas*,
            const SkPaint&,
            const FloatRect& dstRect,
            const FloatRect& srcRect,
            RespectImageOrientationEnum,
            ImageClampingMode) override;

 private:
  UnacceleratedStaticBitmapImage(sk_sp<SkImage>);
  sk_sp<SkImage> m_image;
};

}  // namespace blink

#endif
