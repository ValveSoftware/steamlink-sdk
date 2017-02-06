// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_CLIENT_PICTURE_CACHE_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_CLIENT_PICTURE_CACHE_H_

#include <stdint.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "blimp/common/compositor/reference_tracker.h"
#include "cc/blimp/client_picture_cache.h"
#include "cc/blimp/engine_picture_cache.h"
#include "cc/blimp/picture_data.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blimp {
namespace client {

// BlimpClientPictureCache provides functionality for caching SkPictures once
// they are received from the engine, and cleaning up once the pictures are no
// longer in use. It is required to update this cache when an SkPicture starts
// being used and when it is not longer in use by calling
// MarkPictureForRegistration and MarkPictureForUnregistration respectively.
class BlimpClientPictureCache : public cc::ClientPictureCache {
 public:
  explicit BlimpClientPictureCache(
      SkPicture::InstallPixelRefProc pixel_deserializer);
  ~BlimpClientPictureCache() override;

  // cc::ClientPictureCache implementation.
  sk_sp<const SkPicture> GetPicture(uint32_t engine_picture_id) override;
  void ApplyCacheUpdate(
      const std::vector<cc::PictureData>& cache_update) override;
  void Flush() override;
  void MarkUsed(uint32_t engine_picture_id) override;

 private:
  // Removes all pictures specified in |picture_ids| from |pictures_|. The
  // picture IDs passed to this function must refer to the pictures that are no
  // longer in use.
  void RemoveUnusedPicturesFromCache(const std::vector<uint32_t>& picture_ids);

  // Retrieves the SkPicture with the given |picture_id| from the cache. The
  // given |picture_id| is the unique ID that the engine used to identify the
  // picture in the cc::PictureCacheUpdate that contained the image.
  sk_sp<const SkPicture> GetPictureFromCache(uint32_t picture_id);

  // Verify that the incoming cache update matches the new items that were added
  // to the |reference_tracker_|.
  void VerifyCacheUpdateMatchesReferenceTrackerData(
      const std::vector<uint32_t>& new_tracked_items);

#if DCHECK_IS_ON()
  // A set of the items that were added when the last cache update was applied.
  // Used for verifying that the calculation from the registry matches the
  // expectations.
  std::unordered_set<uint32_t> last_added_;
#endif  // DCHECK_IS_ON()

  // A function pointer valid to use for deserializing images when
  // using SkPicture::CreateFromStream to create an SkPicture from a stream.
  SkPicture::InstallPixelRefProc pixel_deserializer_;

  // The current cache of SkPictures. The key is the unique ID used by the
  // engine, and the value is the SkPicture itself.
  std::unordered_map<uint32_t, sk_sp<const SkPicture>> pictures_;

  // The reference tracker maintains the reference count of used SkPictures.
  ReferenceTracker reference_tracker_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientPictureCache);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_CLIENT_PICTURE_CACHE_H_
