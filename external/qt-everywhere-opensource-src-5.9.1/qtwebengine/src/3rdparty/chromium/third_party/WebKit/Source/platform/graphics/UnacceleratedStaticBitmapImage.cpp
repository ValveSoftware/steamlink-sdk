// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/UnacceleratedStaticBitmapImage.h"

#include "third_party/skia/include/core/SkImage.h"

namespace blink {

PassRefPtr<UnacceleratedStaticBitmapImage>
UnacceleratedStaticBitmapImage::create(sk_sp<SkImage> image) {
  return adoptRef(new UnacceleratedStaticBitmapImage(std::move(image)));
}

UnacceleratedStaticBitmapImage::UnacceleratedStaticBitmapImage(
    sk_sp<SkImage> image)
    : m_image(std::move(image)) {
  DCHECK(m_image);
}

UnacceleratedStaticBitmapImage::~UnacceleratedStaticBitmapImage() {}

IntSize UnacceleratedStaticBitmapImage::size() const {
  return IntSize(m_image->width(), m_image->height());
}

bool UnacceleratedStaticBitmapImage::currentFrameKnownToBeOpaque(MetadataMode) {
  return m_image->isOpaque();
}

void UnacceleratedStaticBitmapImage::draw(SkCanvas* canvas,
                                          const SkPaint& paint,
                                          const FloatRect& dstRect,
                                          const FloatRect& srcRect,
                                          RespectImageOrientationEnum,
                                          ImageClampingMode clampMode) {
  StaticBitmapImage::drawHelper(canvas, paint, dstRect, srcRect, clampMode,
                                m_image);
}

sk_sp<SkImage> UnacceleratedStaticBitmapImage::imageForCurrentFrame() {
  return m_image;
}

}  // namespace blink
