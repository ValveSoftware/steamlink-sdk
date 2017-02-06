// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DRAW_IMAGE_H_
#define CC_PLAYBACK_DRAW_IMAGE_H_

#include "cc/base/cc_export.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {

// TODO(vmpstr): This should probably be DISALLOW_COPY_AND_ASSIGN and transport
// it around using a pointer, since it became kind of large. Profile.
class CC_EXPORT DrawImage {
 public:
  DrawImage();
  DrawImage(sk_sp<const SkImage> image,
            const SkIRect& src_rect,
            SkFilterQuality filter_quality,
            const SkMatrix& matrix);
  DrawImage(const DrawImage& other);
  ~DrawImage();

  const sk_sp<const SkImage>& image() const { return image_; }
  const SkSize& scale() const { return scale_; }
  const SkIRect src_rect() const { return src_rect_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }
  bool matrix_is_decomposable() const { return matrix_is_decomposable_; }
  const SkMatrix& matrix() const { return matrix_; }

  DrawImage ApplyScale(float scale) const {
    return ApplyScale(SkSize::Make(scale, scale));
  }

  DrawImage ApplyScale(const SkSize& scale) const {
    SkMatrix scaled_matrix = matrix_;
    scaled_matrix.postScale(scale.width(), scale.height());
    return DrawImage(image_, src_rect_, filter_quality_, scaled_matrix);
  }

 private:
  sk_sp<const SkImage> image_;
  SkIRect src_rect_;
  SkFilterQuality filter_quality_;
  SkMatrix matrix_;
  SkSize scale_;
  bool matrix_is_decomposable_;
};

}  // namespace cc

#endif  // CC_PLAYBACK_DRAW_IMAGE_H_
