// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/draw_image.h"

namespace cc {
namespace {

// Helper funciton to extract a scale from the matrix. Returns true on success
// and false on failure.
bool ExtractScale(const SkMatrix& matrix, SkSize* scale) {
  *scale = SkSize::Make(matrix.getScaleX(), matrix.getScaleY());
  if (matrix.getType() & SkMatrix::kAffine_Mask) {
    if (!matrix.decomposeScale(scale)) {
      scale->set(1, 1);
      return false;
    }
  }
  return true;
}

}  // namespace

DrawImage::DrawImage()
    : image_(nullptr),
      src_rect_(SkIRect::MakeXYWH(0, 0, 0, 0)),
      filter_quality_(kNone_SkFilterQuality),
      matrix_(SkMatrix::I()),
      scale_(SkSize::Make(1.f, 1.f)),
      matrix_is_decomposable_(true) {}

DrawImage::DrawImage(sk_sp<const SkImage> image,
                     const SkIRect& src_rect,
                     SkFilterQuality filter_quality,
                     const SkMatrix& matrix)
    : image_(std::move(image)),
      src_rect_(src_rect),
      filter_quality_(filter_quality),
      matrix_(matrix) {
  matrix_is_decomposable_ = ExtractScale(matrix_, &scale_);
}

DrawImage::DrawImage(const DrawImage& other) = default;

DrawImage::~DrawImage() = default;

}  // namespace cc
