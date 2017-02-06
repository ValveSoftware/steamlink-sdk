// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_format_utils.h"

namespace cc {

SkColorType ResourceFormatToClosestSkColorType(ResourceFormat format) {
  // Use kN32_SkColorType if there is no corresponding SkColorType.
  switch (format) {
    case RGBA_4444:
      return kARGB_4444_SkColorType;
    case RGBA_8888:
    case BGRA_8888:
      return kN32_SkColorType;
    case ALPHA_8:
      return kAlpha_8_SkColorType;
    case RGB_565:
      return kRGB_565_SkColorType;
    case LUMINANCE_8:
      return kGray_8_SkColorType;
    case ETC1:
    case RED_8:
    case LUMINANCE_F16:
      return kN32_SkColorType;
  }
  NOTREACHED();
  return kN32_SkColorType;
}

}  // namespace cc
