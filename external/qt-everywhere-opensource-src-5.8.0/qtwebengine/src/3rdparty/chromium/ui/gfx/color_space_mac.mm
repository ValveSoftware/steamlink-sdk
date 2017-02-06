// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace gfx {

// static
ColorSpace ColorSpace::FromBestMonitor() {
  ColorSpace color_space;
  CGColorSpaceRef monitor_color_space(base::mac::GetSystemColorSpace());
  base::ScopedCFTypeRef<CFDataRef> icc_profile(
      CGColorSpaceCopyICCProfile(monitor_color_space));
  if (!icc_profile)
    return color_space;
  size_t length = CFDataGetLength(icc_profile);
  if (!IsValidProfileLength(length))
    return color_space;
  const unsigned char* data = CFDataGetBytePtr(icc_profile);
  color_space.icc_profile_.assign(data, data + length);
  return color_space;
}

}  // namespace gfx
