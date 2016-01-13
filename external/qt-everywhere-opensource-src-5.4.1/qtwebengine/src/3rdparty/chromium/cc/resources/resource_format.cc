// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_format.h"

namespace cc {

SkColorType ResourceFormatToSkColorType(ResourceFormat format) {
  switch (format) {
    case RGBA_4444:
      return kARGB_4444_SkColorType;
    case RGBA_8888:
    case BGRA_8888:
      return kPMColor_SkColorType;
    case ETC1:
    case LUMINANCE_8:
    case RGB_565:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return kPMColor_SkColorType;
}

}  // namespace cc
