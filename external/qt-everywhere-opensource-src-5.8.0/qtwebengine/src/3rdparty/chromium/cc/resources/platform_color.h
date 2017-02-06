// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PLATFORM_COLOR_H_
#define CC_RESOURCES_PLATFORM_COLOR_H_

#include "base/logging.h"
#include "base/macros.h"
#include "cc/resources/resource_format.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace cc {

class PlatformColor {
 public:
  enum SourceDataFormat {
    SOURCE_FORMAT_RGBA8,
    SOURCE_FORMAT_BGRA8
  };

  static SourceDataFormat Format() {
    return SK_B32_SHIFT ? SOURCE_FORMAT_RGBA8 : SOURCE_FORMAT_BGRA8;
  }

  // Returns the most efficient texture format for this platform.
  static ResourceFormat BestTextureFormat() {
    switch (Format()) {
      case SOURCE_FORMAT_BGRA8:
        return BGRA_8888;
      case SOURCE_FORMAT_RGBA8:
        return RGBA_8888;
    }
    NOTREACHED();
    return RGBA_8888;
  }

  // Returns the most efficient supported texture format for this platform.
  static ResourceFormat BestSupportedTextureFormat(bool supports_bgra8888) {
    switch (Format()) {
      case SOURCE_FORMAT_BGRA8:
        return (supports_bgra8888) ? BGRA_8888 : RGBA_8888;
      case SOURCE_FORMAT_RGBA8:
        return RGBA_8888;
    }
    NOTREACHED();
    return RGBA_8888;
  }

  // Return true if the given 32bpp resource format has the same component order
  // as the platform color data format.
  static bool SameComponentOrder(ResourceFormat format) {
    switch (format) {
      case RGBA_8888:
        return Format() == SOURCE_FORMAT_RGBA8;
      case BGRA_8888:
        return Format() == SOURCE_FORMAT_BGRA8;
      case ALPHA_8:
      case LUMINANCE_8:
      case RGB_565:
      case RGBA_4444:
      case ETC1:
      case RED_8:
      case LUMINANCE_F16:
        NOTREACHED();
        return false;
    }

    NOTREACHED();
    return false;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PlatformColor);
};

}  // namespace cc

#endif  // CC_RESOURCES_PLATFORM_COLOR_H_
