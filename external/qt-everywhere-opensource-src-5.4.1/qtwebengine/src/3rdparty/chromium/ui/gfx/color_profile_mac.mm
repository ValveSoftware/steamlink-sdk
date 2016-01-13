// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_profile.h"

#include "base/mac/mac_util.h"

namespace gfx {

bool GetDisplayColorProfile(const gfx::Rect& bounds,
                            std::vector<char>* profile) {
  if (bounds.IsEmpty())
    return false;
  // TODO(noel): implement.
  return false;
}

void ReadColorProfile(std::vector<char>* profile) {
  CGColorSpaceRef monitor_color_space(base::mac::GetSystemColorSpace());
  base::ScopedCFTypeRef<CFDataRef> icc_profile(
      CGColorSpaceCopyICCProfile(monitor_color_space));
  if (!icc_profile)
    return;
  size_t length = CFDataGetLength(icc_profile);
  if (gfx::InvalidColorProfileLength(length))
    return;
  const unsigned char* data = CFDataGetBytePtr(icc_profile);
  profile->assign(data, data + length);
}

}  // namespace gfx
