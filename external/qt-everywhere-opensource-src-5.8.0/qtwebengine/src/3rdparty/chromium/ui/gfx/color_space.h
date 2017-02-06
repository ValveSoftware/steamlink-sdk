// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_SPACE_H_
#define UI_GFX_COLOR_SPACE_H_

#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class GFX_EXPORT ColorSpace {
 public:
  ColorSpace();
  ColorSpace(ColorSpace&& other);
  ColorSpace(const ColorSpace& other);
  ColorSpace& operator=(const ColorSpace& other);
  ~ColorSpace();
  bool operator==(const ColorSpace& other) const;

  // Returns the color profile of the monitor that can best represent color.
  // This profile should be used for creating content that does not know on
  // which monitor it will be displayed.
  static ColorSpace FromBestMonitor();
  static ColorSpace FromICCProfile(const std::vector<char>& icc_profile);

  const std::vector<char>& GetICCProfile() const { return icc_profile_; }

#if defined(OS_WIN)
  // This will read monitor ICC profiles from disk and cache the results for the
  // other functions to read. This should not be called on the UI or IO thread.
  static void UpdateCachedProfilesOnBackgroundThread();
  static bool CachedProfilesNeedUpdate();
#endif

  static bool IsValidProfileLength(size_t length);

 private:
  std::vector<char> icc_profile_;
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_SPACE_H_
