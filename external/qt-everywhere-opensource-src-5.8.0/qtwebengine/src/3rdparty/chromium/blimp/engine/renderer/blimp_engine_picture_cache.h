// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_RENDERER_BLIMP_ENGINE_PICTURE_CACHE_H_
#define BLIMP_ENGINE_RENDERER_BLIMP_ENGINE_PICTURE_CACHE_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "blimp/common/compositor/reference_tracker.h"
#include "cc/blimp/engine_picture_cache.h"
#include "cc/blimp/picture_data.h"
#include "third_party/skia/include/core/SkPicture.h"

class SkPixelSerializer;

namespace blimp {
namespace engine {

// BlimpEnginePictureCache provides functionality for caching SkPictures before
// they are sent from the engine to the client. The cache is cleared after
// every time it is flushed which happens when CalculateCacheUpdateAndFlush()
// is called. The expected state of what the client already has cached is
// tracked. It is required to update this cache when an SkPicture
// starts being used and when it is not longer in use by calling
// MarkPictureForRegistration and MarkPictureForUnregistration respectively.
// The lifetime of a cache matches the lifetime of a specific compositor.
// All interaction with this class should happen on the main thread.
class BlimpEnginePictureCache : public cc::EnginePictureCache {
 public:
  explicit BlimpEnginePictureCache(SkPixelSerializer* pixel_serializer);
  ~BlimpEnginePictureCache() override;

  // cc::EnginePictureCache implementation.
  void MarkUsed(const SkPicture* picture) override;
  std::vector<cc::PictureData> CalculateCacheUpdateAndFlush() override;

 private:
  // Serializes the SkPicture and adds it to |pictures_|.
  void Put(const SkPicture* picture);

  // A serializer that be used to pass in to SkPicture::serialize(...) for
  // serializing the SkPicture to a stream.
  SkPixelSerializer* pixel_serializer_;

  // The current cache of pictures. Used for temporarily storing pictures until
  // the next call to CalculateCacheUpdateAndFlush(), at which point this map
  // is cleared.
  std::unordered_map<uint32_t, cc::PictureData> pictures_;

  // The reference tracker maintains the reference count of used SkPictures.
  ReferenceTracker reference_tracker_;

  DISALLOW_COPY_AND_ASSIGN(BlimpEnginePictureCache);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_RENDERER_BLIMP_ENGINE_PICTURE_CACHE_H_
