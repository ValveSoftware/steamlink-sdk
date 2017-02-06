// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SET_WALLPAPER_DELEGATE_H_
#define COMPONENTS_ARC_SET_WALLPAPER_DELEGATE_H_

#include <stdint.h>

#include <vector>

namespace arc {

// Delegate to allow setting the wallpaper.
class SetWallpaperDelegate {
 public:
  virtual ~SetWallpaperDelegate() = default;

  // Sets an image represented in JPEG format as the wallpaper.
  virtual void SetWallpaper(const std::vector<uint8_t>& jpeg_data) = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SET_WALLPAPER_DELEGATE_H_
