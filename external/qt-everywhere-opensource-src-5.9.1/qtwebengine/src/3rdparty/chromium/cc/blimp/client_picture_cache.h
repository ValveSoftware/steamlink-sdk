// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_CLIENT_PICTURE_CACHE_H_
#define CC_BLIMP_CLIENT_PICTURE_CACHE_H_

#include <vector>

#include "third_party/skia/include/core/SkRefCnt.h"

class SkPicture;

namespace cc {
struct PictureData;

// ClientPictureCache provides functionaltiy for marking when SkPictures are in
// use an when they stop being in use. PictureCacheUpdates are provided
// with all new SkPictures that a client needs and the respective unique IDs
// used on the engine. The SkPictures are kept in memory until they are no
// longer in use.
class ClientPictureCache {
 public:
  virtual ~ClientPictureCache() {}

  // MarkUsed must be called when the client has started using an SkPicture that
  // was sent to from the engine.
  virtual void MarkUsed(uint32_t engine_picture_id) = 0;

  // Retrieves an SkPicture from the in-memory cache. It is required that the
  // provided ID is the same as the unique ID that is given through the cache
  // update.
  virtual sk_sp<const SkPicture> GetPicture(uint32_t engine_picture_id) = 0;

  // Called when a PictureCacheUpdate has been retrieved from the engine. This
  // must contain all the new SkPictures that are in use.
  virtual void ApplyCacheUpdate(const std::vector<PictureData>& pictures) = 0;

  // Flushes the current state of the cache, removing unused SkPictures. Must be
  // called after deserialization is finished.
  virtual void Flush() = 0;
};

}  // namespace cc

#endif  // CC_BLIMP_CLIENT_PICTURE_CACHE_H_
