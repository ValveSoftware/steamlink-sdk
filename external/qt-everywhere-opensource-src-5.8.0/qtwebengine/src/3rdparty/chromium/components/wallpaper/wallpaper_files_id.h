// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLPAPER_WALLPAPER_FILES_ID_H_
#define COMPONENTS_WALLPAPER_WALLPAPER_FILES_ID_H_

#include <string>

#include "components/wallpaper/wallpaper_export.h"

namespace wallpaper {

class WALLPAPER_EXPORT WallpaperFilesId {
 public:
  WallpaperFilesId();

  // This should be used for deserialization only.
  static WallpaperFilesId FromString(const std::string& data);

  const std::string& id() const { return id_; }

  // Returns true if id is not empty.
  bool is_valid() const;

 private:
  WallpaperFilesId(const std::string& id);
  std::string id_;
};

}  // namespace wallpaper

#endif  // COMPONENTS_WALLPAPER_WALLPAPER_FILES_ID_H_
