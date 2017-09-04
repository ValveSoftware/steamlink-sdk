// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/decoded_draw_image.h"

namespace cc {

DecodedDrawImage::DecodedDrawImage(sk_sp<const SkImage> image,
                                   const SkSize& src_rect_offset,
                                   const SkSize& scale_adjustment,
                                   SkFilterQuality filter_quality)
    : image_(std::move(image)),
      src_rect_offset_(src_rect_offset),
      scale_adjustment_(scale_adjustment),
      filter_quality_(filter_quality),
      at_raster_decode_(false) {}

DecodedDrawImage::DecodedDrawImage(sk_sp<const SkImage> image,
                                   SkFilterQuality filter_quality)
    : DecodedDrawImage(std::move(image),
                       SkSize::Make(0.f, 0.f),
                       SkSize::Make(1.f, 1.f),
                       filter_quality) {}

DecodedDrawImage::DecodedDrawImage(const DecodedDrawImage& other) = default;

DecodedDrawImage::~DecodedDrawImage() = default;

}  // namespace cc
