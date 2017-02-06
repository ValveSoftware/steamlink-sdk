// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_client_picture_cache.h"

#include <utility>
#include <vector>

#include "third_party/skia/include/core/SkStream.h"

namespace blimp {
namespace client {
namespace {

// Helper function to deserialize the content of |picture_data| into an
// SkPicture.
sk_sp<const SkPicture> DeserializePicture(
    SkPicture::InstallPixelRefProc pixel_deserializer,
    const cc::PictureData& picture_data) {
  SkMemoryStream stream(picture_data.data);
  return SkPicture::MakeFromStream(&stream, pixel_deserializer);
}

}  // namespace

BlimpClientPictureCache::BlimpClientPictureCache(
    SkPicture::InstallPixelRefProc pixel_deserializer)
    : pixel_deserializer_(pixel_deserializer) {}

BlimpClientPictureCache::~BlimpClientPictureCache() = default;

sk_sp<const SkPicture> BlimpClientPictureCache::GetPicture(
    uint32_t engine_picture_id) {
  return GetPictureFromCache(engine_picture_id);
}

void BlimpClientPictureCache::ApplyCacheUpdate(
    const std::vector<cc::PictureData>& cache_update) {
  // Add new pictures from the |cache_update| to |pictures_|.
  for (const cc::PictureData& picture_data : cache_update) {
    DCHECK(pictures_.find(picture_data.unique_id) == pictures_.end());
    sk_sp<const SkPicture> deserialized_picture =
        DeserializePicture(pixel_deserializer_, picture_data);

    pictures_[picture_data.unique_id] = std::move(deserialized_picture);

#if DCHECK_IS_ON()
    last_added_.insert(picture_data.unique_id);
#endif  // DCHECK_IS_ON()
  }
}

void BlimpClientPictureCache::Flush() {
  // Calculate which pictures can now be removed. |added| is only used for
  // verifying that what we calculated matches the new items that have been
  // inserted into the cache.
  std::vector<uint32_t> added;
  std::vector<uint32_t> removed;
  reference_tracker_.CommitRefCounts(&added, &removed);

  VerifyCacheUpdateMatchesReferenceTrackerData(added);

  RemoveUnusedPicturesFromCache(removed);
  reference_tracker_.ClearRefCounts();
}

void BlimpClientPictureCache::MarkUsed(uint32_t engine_picture_id) {
  reference_tracker_.IncrementRefCount(engine_picture_id);
}

void BlimpClientPictureCache::RemoveUnusedPicturesFromCache(
    const std::vector<uint32_t>& removed) {
  for (const auto& engine_picture_id : removed) {
    auto entry = pictures_.find(engine_picture_id);
    DCHECK(entry != pictures_.end());
    pictures_.erase(entry);
  }
}

sk_sp<const SkPicture> BlimpClientPictureCache::GetPictureFromCache(
    uint32_t engine_picture_id) {
  DCHECK(pictures_.find(engine_picture_id) != pictures_.end());
  return pictures_[engine_picture_id];
}

void BlimpClientPictureCache::VerifyCacheUpdateMatchesReferenceTrackerData(
    const std::vector<uint32_t>& new_tracked_items) {
#if DCHECK_IS_ON()
  DCHECK_EQ(new_tracked_items.size(), last_added_.size());
  DCHECK(std::unordered_set<uint32_t>(new_tracked_items.begin(),
                                      new_tracked_items.end()) == last_added_);
  last_added_.clear();
#endif  // DCHECK_IS_ON()
}

}  // namespace client
}  // namespace blimp
