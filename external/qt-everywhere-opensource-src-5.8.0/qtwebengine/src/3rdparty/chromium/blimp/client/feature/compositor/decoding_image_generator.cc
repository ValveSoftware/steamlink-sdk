// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/decoding_image_generator.h"

#include "base/numerics/safe_conversions.h"
#include "blimp/client/feature/compositor/blimp_image_decoder.h"
#include "blimp/client/feature/compositor/blob_image_serialization_processor.h"
#include "blimp/common/proto/blob_cache.pb.h"
#include "third_party/libwebp/webp/decode.h"
#include "third_party/libwebp/webp/demux.h"
#include "third_party/skia/include/core/SkData.h"

namespace blimp {
namespace client {

SkImageGenerator* DecodingImageGenerator::create(SkData* data) {
  BlobCacheImageMetadata parsed_metadata;
  int signed_size = base::checked_cast<int>(data->size());
  if (!parsed_metadata.ParseFromArray(data->data(), signed_size)) {
    // Failed to parse proto, so will fail to decode later as well. Inform
    // Skia by giving back an empty SkImageInfo.
    return new DecodingImageGenerator(SkImageInfo::MakeN32Premul(0, 0),
                                      data->data(), data->size());
  }

  return new DecodingImageGenerator(
      SkImageInfo::MakeN32Premul(parsed_metadata.width(),
                                 parsed_metadata.height()),
      data->data(), data->size());
}

DecodingImageGenerator::DecodingImageGenerator(const SkImageInfo info,
                                               const void* data,
                                               size_t size)
    : SkImageGenerator(info) {
  if (!BlobImageSerializationProcessor::current()->GetAndDecodeBlob(
          data, size, &decoded_bitmap_)) {
    DLOG(FATAL) << "GetAndDecodeBlob() failed.";
  }
}

DecodingImageGenerator::~DecodingImageGenerator() {}

bool DecodingImageGenerator::onGetPixels(const SkImageInfo& info,
                                         void* pixels,
                                         size_t rowBytes,
                                         SkPMColor table[],
                                         int* tableCount) {
  SkAutoLockPixels bitmapLock(decoded_bitmap_);
  if (decoded_bitmap_.getPixels() != pixels) {
    return decoded_bitmap_.copyPixelsTo(pixels, rowBytes * info.height(),
                                        rowBytes);
  }
  return true;
}

bool DecodingImageGenerator::onQueryYUV8(SkYUVSizeInfo* sizeInfo,
                                         SkYUVColorSpace* colorSpace) const {
  return false;
}

bool DecodingImageGenerator::onGetYUV8Planes(const SkYUVSizeInfo& sizeInfo,
                                             void* planes[3]) {
  return false;
}

}  // namespace client
}  // namespace blimp
