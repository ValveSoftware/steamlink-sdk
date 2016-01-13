// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_FORMAT_H_
#define CC_RESOURCES_RESOURCE_FORMAT_H_

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

// Keep in sync with arrays below.
enum ResourceFormat {
  RGBA_8888,
  RGBA_4444,
  BGRA_8888,
  LUMINANCE_8,
  RGB_565,
  ETC1,
  RESOURCE_FORMAT_MAX = ETC1,
};

SkColorType ResourceFormatToSkColorType(ResourceFormat format);

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_FORMAT_H_
