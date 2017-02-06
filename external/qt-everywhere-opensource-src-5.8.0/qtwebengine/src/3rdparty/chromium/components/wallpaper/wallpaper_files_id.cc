// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallpaper/wallpaper_files_id.h"

namespace wallpaper {

WallpaperFilesId::WallpaperFilesId() {}

WallpaperFilesId::WallpaperFilesId(const std::string& id) : id_(id) {}

// static
WallpaperFilesId WallpaperFilesId::FromString(const std::string& id) {
  return WallpaperFilesId(id);
}

bool WallpaperFilesId::is_valid() const {
  return !id_.empty();
}

}  // namespace wallpaper
