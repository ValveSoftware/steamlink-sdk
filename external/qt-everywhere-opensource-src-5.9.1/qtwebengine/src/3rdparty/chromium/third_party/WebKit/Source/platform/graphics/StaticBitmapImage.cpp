// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/StaticBitmapImage.h"

#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/UnacceleratedStaticBitmapImage.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace blink {

PassRefPtr<StaticBitmapImage> StaticBitmapImage::create(sk_sp<SkImage> image) {
  if (!image)
    return nullptr;
  if (image->isTextureBacked())
    return AcceleratedStaticBitmapImage::createFromSharedContextImage(
        std::move(image));
  return UnacceleratedStaticBitmapImage::create(std::move(image));
}

void StaticBitmapImage::drawHelper(SkCanvas* canvas,
                                   const SkPaint& paint,
                                   const FloatRect& dstRect,
                                   const FloatRect& srcRect,
                                   ImageClampingMode clampMode,
                                   sk_sp<SkImage> image) {
  FloatRect adjustedSrcRect = srcRect;
  adjustedSrcRect.intersect(SkRect::Make(image->bounds()));

  if (dstRect.isEmpty() || adjustedSrcRect.isEmpty())
    return;  // Nothing to draw.

  canvas->drawImageRect(image.get(), adjustedSrcRect, dstRect, &paint,
                        WebCoreClampingModeToSkiaRectConstraint(clampMode));

  if (ImageObserver* observer = getImageObserver())
    observer->didDraw(this);
}

}  // namespace blink
