// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_IMAGE_HIJACK_CANVAS_H_
#define CC_PLAYBACK_IMAGE_HIJACK_CANVAS_H_

#include "base/macros.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"

namespace cc {

class ImageDecodeController;

class ImageHijackCanvas : public SkNWayCanvas {
 public:
  ImageHijackCanvas(int width,
                    int height,
                    ImageDecodeController* image_decode_controller);

 private:
  // Ensure that pictures are unpacked by this canvas, instead of being
  // forwarded to the raster canvas.
  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix* matrix,
                     const SkPaint* paint) override;

  void onDrawImage(const SkImage* image,
                   SkScalar x,
                   SkScalar y,
                   const SkPaint* paint) override;
  void onDrawImageRect(const SkImage* image,
                       const SkRect* src,
                       const SkRect& dst,
                       const SkPaint* paint,
                       SrcRectConstraint constraint) override;
  void onDrawImageNine(const SkImage* image,
                       const SkIRect& center,
                       const SkRect& dst,
                       const SkPaint* paint) override;

  ImageDecodeController* image_decode_controller_;

  DISALLOW_COPY_AND_ASSIGN(ImageHijackCanvas);
};

}  // namespace cc

#endif  // CC_PLAYBACK_IMAGE_HIJACK_CANVAS_H_
