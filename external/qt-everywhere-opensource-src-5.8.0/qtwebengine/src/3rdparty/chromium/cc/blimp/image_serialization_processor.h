// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_IMAGE_SERIALIZATION_PROCESSOR_H_
#define CC_BLIMP_IMAGE_SERIALIZATION_PROCESSOR_H_

#include "base/memory/ptr_util.h"
#include "cc/blimp/client_picture_cache.h"
#include "cc/blimp/engine_picture_cache.h"
#include "third_party/skia/include/core/SkPicture.h"

class SkPixelSerializer;

namespace cc {

// ImageSerializationProcessor provides functionality to serialize,
// deserialize and cache Skia images.
class ImageSerializationProcessor {
 public:
  virtual ~ImageSerializationProcessor() {}

  virtual std::unique_ptr<EnginePictureCache> CreateEnginePictureCache() = 0;
  virtual std::unique_ptr<ClientPictureCache> CreateClientPictureCache() = 0;
};

}  // namespace cc

#endif  // CC_BLIMP_IMAGE_SERIALIZATION_PROCESSOR_H_
