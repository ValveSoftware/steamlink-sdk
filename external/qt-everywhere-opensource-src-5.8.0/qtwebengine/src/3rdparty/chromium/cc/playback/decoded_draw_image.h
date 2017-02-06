// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DECODED_DRAW_IMAGE_H_
#define CC_PLAYBACK_DECODED_DRAW_IMAGE_H_

#include <cfloat>
#include <cmath>

#include "cc/base/cc_export.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"

namespace cc {

class CC_EXPORT DecodedDrawImage {
 public:
  DecodedDrawImage(sk_sp<const SkImage> image,
                   const SkSize& src_rect_offset,
                   const SkSize& scale_adjustment,
                   SkFilterQuality filter_quality);
  DecodedDrawImage(sk_sp<const SkImage> image, SkFilterQuality filter_quality);
  DecodedDrawImage(const DecodedDrawImage& other);
  ~DecodedDrawImage();

  const sk_sp<const SkImage>& image() const { return image_; }
  const SkSize& src_rect_offset() const { return src_rect_offset_; }
  const SkSize& scale_adjustment() const { return scale_adjustment_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }
  bool is_scale_adjustment_identity() const {
    return std::abs(scale_adjustment_.width() - 1.f) < FLT_EPSILON &&
           std::abs(scale_adjustment_.height() - 1.f) < FLT_EPSILON;
  }

  void set_at_raster_decode(bool at_raster_decode) {
    at_raster_decode_ = at_raster_decode;
  }
  bool is_at_raster_decode() const { return at_raster_decode_; }

 private:
  sk_sp<const SkImage> image_;
  const SkSize src_rect_offset_;
  const SkSize scale_adjustment_;
  const SkFilterQuality filter_quality_;
  bool at_raster_decode_;
};

}  // namespace cc

#endif  // CC_PLAYBACK_DECODED_DRAW_IMAGE_H_
