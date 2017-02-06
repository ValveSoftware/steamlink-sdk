// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_ENGINE_PICTURE_CACHE_H_
#define CC_BLIMP_ENGINE_PICTURE_CACHE_H_

#include <vector>

class SkPicture;

namespace cc {
struct PictureData;

// EnginePictureCache provides functionaltiy for marking when SkPictures are in
// use an when they stop being in use. Based on this, a PictureCacheUpdate can
// be retrieved that includes all the new items since last time it was called.
// This PictureCacheUpdate is then supposed to be sent to the client.
class EnginePictureCache {
 public:
  virtual ~EnginePictureCache() {}

  // MarkUsed must be called when an SkPicture needs to available on the client.
  virtual void MarkUsed(const SkPicture* picture) = 0;

  // Called when a PictureCacheUpdate is going to be sent to the client. This
  // must contain all the new SkPictures that are in use since last call.
  virtual std::vector<PictureData> CalculateCacheUpdateAndFlush() = 0;
};

}  // namespace cc

#endif  // CC_BLIMP_ENGINE_PICTURE_CACHE_H_
