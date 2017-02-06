// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blob_image_serialization_processor.h"

#include <stddef.h>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "blimp/client/feature/compositor/blimp_client_picture_cache.h"
#include "blimp/client/feature/compositor/blimp_image_decoder.h"
#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/common/blob_cache/id_util.h"
#include "blimp/common/proto/blob_cache.pb.h"
#include "blimp/net/blob_channel/blob_channel_receiver.h"

namespace blimp {
namespace client {

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

// static
bool BlobImageSerializationProcessor::InstallPixelRefProc(const void* input,
                                                          size_t input_size,
                                                          SkBitmap* bitmap) {
  return current()->GetAndDecodeBlob(input, input_size, bitmap);
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
  return base::WrapUnique(new BlimpClientPictureCache(
      &BlobImageSerializationProcessor::InstallPixelRefProc));
}

}  // namespace client
}  // namespace blimp
