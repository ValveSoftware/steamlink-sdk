// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/blob_image_serialization_processor.h"

#include <stddef.h>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "blimp/client/core/compositor/blimp_client_picture_cache.h"
#include "blimp/client/core/compositor/blimp_image_decoder.h"
#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/common/blob_cache/id_util.h"
#include "blimp/common/proto/blob_cache.pb.h"
#include "blimp/net/blob_channel/blob_channel_receiver.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blimp {
namespace client {
namespace {
class BlobImageDeserializer final : public SkImageDeserializer {
 public:
  sk_sp<SkImage> makeFromData(SkData* data, const SkIRect* subset) override {
    return makeFromMemory(data->data(), data->size(), subset);
  }

  sk_sp<SkImage> makeFromMemory(const void* data,
                                size_t size,
                                const SkIRect* subset) override {
    sk_sp<SkImage> img;
    SkBitmap bitmap;
    const auto& processor = BlobImageSerializationProcessor::current();
    if (processor->GetAndDecodeBlob(data, size, &bitmap)) {
      img = SkImage::MakeFromBitmap(bitmap);
      if (img && subset) {
        img = img->makeSubset(*subset);
      }
    }
    return img;
  }
};
}  // namespace

BlobImageSerializationProcessor*
    BlobImageSerializationProcessor::current_instance_ = nullptr;

BlobImageSerializationProcessor* BlobImageSerializationProcessor::current() {
  DCHECK(current_instance_);
  return current_instance_;
}

BlobImageSerializationProcessor::BlobImageSerializationProcessor() {
  DCHECK(!current_instance_);
  current_instance_ = this;
}

BlobImageSerializationProcessor::~BlobImageSerializationProcessor() {
  current_instance_ = nullptr;
}

bool BlobImageSerializationProcessor::GetAndDecodeBlob(const void* input,
                                                       size_t input_size,
                                                       SkBitmap* bitmap) const {
  DCHECK(input);
  DCHECK(bitmap);

  BlobCacheImageMetadata parsed_metadata;
  if (!parsed_metadata.ParseFromArray(input, input_size)) {
    LOG(ERROR) << "Couldn't parse blob metadata structure.";
    error_delegate_->OnImageDecodeError();
    return false;
  }
  if (!IsValidBlobId(parsed_metadata.id())) {
    LOG(ERROR) << "Image payload is not a valid blob ID.";
    error_delegate_->OnImageDecodeError();
    return false;
  }

  // Get the image blob synchronously.
  // TODO(kmarshall): Add support for asynchronous Get() completion after Skia
  // supports image replacement.
  BlobDataPtr blob = blob_receiver_->Get(parsed_metadata.id());
  if (!blob) {
    LOG(ERROR) << "No blob found with ID: "
               << BlobIdToString(parsed_metadata.id());
    error_delegate_->OnImageDecodeError();
    return false;
  }

  DVLOG(1) << "GetAndDecodeBlob(" << BlobIdToString(parsed_metadata.id())
           << ")";

  return DecodeBlimpImage(reinterpret_cast<const void*>(&blob->data[0]),
                          blob->data.size(), bitmap);
}

std::unique_ptr<cc::EnginePictureCache>
BlobImageSerializationProcessor::CreateEnginePictureCache() {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<cc::ClientPictureCache>
BlobImageSerializationProcessor::CreateClientPictureCache() {
  return base::MakeUnique<BlimpClientPictureCache>(
      base::MakeUnique<BlobImageDeserializer>());
}

}  // namespace client
}  // namespace blimp
