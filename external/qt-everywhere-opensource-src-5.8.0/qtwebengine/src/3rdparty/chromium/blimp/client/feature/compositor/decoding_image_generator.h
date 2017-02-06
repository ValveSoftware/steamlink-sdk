// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_DECODING_IMAGE_GENERATOR_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_DECODING_IMAGE_GENERATOR_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkImageInfo.h"

class SkData;

namespace blimp {
namespace client {

class DecodingImageGenerator : public SkImageGenerator {
 public:
  static SkImageGenerator* create(SkData* data);
  explicit DecodingImageGenerator(const SkImageInfo info,
                                  const void* data,
                                  size_t size);
  ~DecodingImageGenerator() override;

 protected:
  // SkImageGenerator implementation.
  bool onGetPixels(const SkImageInfo&,
                   void* pixels,
                   size_t rowBytes,
                   SkPMColor table[],
                   int* tableCount) override;

  bool onQueryYUV8(SkYUVSizeInfo* sizeInfo,
                   SkYUVColorSpace* colorSpace) const override;

  bool onGetYUV8Planes(const SkYUVSizeInfo&,
                       void* planes[3]) override;

 private:
  SkBitmap decoded_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(DecodingImageGenerator);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_DECODING_IMAGE_GENERATOR_H_
