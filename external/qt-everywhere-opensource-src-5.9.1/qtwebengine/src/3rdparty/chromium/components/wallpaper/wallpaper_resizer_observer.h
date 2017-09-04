// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLPAPER_WALLPAPER_RESIZER_OBSERVER_H_
#define COMPONENTS_WALLPAPER_WALLPAPER_RESIZER_OBSERVER_H_

#include "components/wallpaper/wallpaper_export.h"

namespace wallpaper {

class WALLPAPER_EXPORT WallpaperResizerObserver {
 public:
  // Invoked when the wallpaper is resized.
  virtual void OnWallpaperResized() = 0;

 protected:
  virtual ~WallpaperResizerObserver() {}
};

}  // namespace wallpaper

#endif  // COMPONENTS_WALLPAPER_WALLPAPER_RESIZER_OBSERVER_H_
