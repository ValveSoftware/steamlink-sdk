// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_PROFILE_H_
#define UI_GFX_COLOR_PROFILE_H_

#include <vector>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

static const size_t kMinProfileLength = 128;
static const size_t kMaxProfileLength = 4 * 1024 * 1024;

class GFX_EXPORT ColorProfile {
 public:
  // On Windows, this reads a file from disk so it shouldn't be run on the UI
  // or IO thread.
  ColorProfile();
  ~ColorProfile();

  const std::vector<char>& profile() const { return profile_; }

 private:
  std::vector<char> profile_;

  DISALLOW_COPY_AND_ASSIGN(ColorProfile);
};

inline bool InvalidColorProfileLength(size_t length) {
  return (length < kMinProfileLength) || (length > kMaxProfileLength);
}

// Return the color profile of the display nearest the screen bounds. On Win32,
// this may read a file from disk, so it shouldn't be run on the UI/IO threads.
// If the given bounds are empty, or are off-screen, return false meaning there
// is no color profile associated with the bounds.
GFX_EXPORT bool GetDisplayColorProfile(const gfx::Rect& bounds,
                                       std::vector<char>* profile);
}  // namespace gfx

#endif  // UI_GFX_COLOR_PROFILE_H_
