// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLPAPER_WALLPAPER_LAYOUT_H_
#define COMPONENTS_WALLPAPER_WALLPAPER_LAYOUT_H_

namespace wallpaper {

// This enum is used to back a histogram, and should therefore be treated as
// append-only.
enum WallpaperLayout {
  // Center the wallpaper on the desktop without scaling it. The wallpaper
  // may be cropped.
  WALLPAPER_LAYOUT_CENTER,
  // Scale the wallpaper (while preserving its aspect ratio) to cover the
  // desktop; the wallpaper may be cropped.
  WALLPAPER_LAYOUT_CENTER_CROPPED,
  // Scale the wallpaper (without preserving its aspect ratio) to match the
  // desktop's size.
  WALLPAPER_LAYOUT_STRETCH,
  // Tile the wallpaper over the background without scaling it.
  WALLPAPER_LAYOUT_TILE,
  NUM_WALLPAPER_LAYOUT,
};

}  // namespace wallpaper
#endif  // COMPONENTS_WALLPAPER_WALLPAPER_LAYOUT_H_
