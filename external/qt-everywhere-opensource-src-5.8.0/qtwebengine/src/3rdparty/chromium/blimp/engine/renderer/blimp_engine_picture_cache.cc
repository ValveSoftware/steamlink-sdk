// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/blimp_engine_picture_cache.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blimp {
namespace engine {

BlimpEnginePictureCache::BlimpEnginePictureCache(
    SkPixelSerializer* pixel_serializer)
    : pixel_serializer_(pixel_serializer) {}

BlimpEnginePictureCache::~BlimpEnginePictureCache() = default;

void BlimpEnginePictureCache::MarkUsed(const SkPicture* picture) {
  DCHECK(picture);
  reference_tracker_.IncrementRefCount(picture->uniqueID());

  // Do not serialize multiple times, even though the item is referred to from
  // multiple places.
  if (pictures_.find(picture->uniqueID()) != pictures_.end()) {
    return;
  }

  Put(picture);
}

std::vector<cc::PictureData>
BlimpEnginePictureCache::CalculateCacheUpdateAndFlush() {
  std::vector<uint32_t> added;
  std::vector<uint32_t> removed;
  reference_tracker_.CommitRefCounts(&added, &removed);

  // Create cache update consisting of new pictures.
  std::vector<cc::PictureData> update;
  for (const uint32_t item : added) {
    auto entry = pictures_.find(item);
    DCHECK(entry != pictures_.end());
    update.push_back(entry->second);
  }

  // All new items will be sent to the client, so clear everything.
  pictures_.clear();
  reference_tracker_.ClearRefCounts();

  return update;
}

void BlimpEnginePictureCache::Put(const SkPicture* picture) {
  SkDynamicMemoryWStream stream;
  picture->serialize(&stream, pixel_serializer_);
  DCHECK_GE(stream.bytesWritten(), 0u);

  // Store the picture data until it is sent to the client.
  pictures_.insert(
      std::make_pair(picture->uniqueID(),
                     cc::PictureData(picture->uniqueID(),
                                     sk_sp<SkData>(stream.copyToData()))));
}

}  // namespace engine
}  // namespace blimp
