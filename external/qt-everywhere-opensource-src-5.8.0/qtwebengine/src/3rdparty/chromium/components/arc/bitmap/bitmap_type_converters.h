// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_BITMAP_BITMAP_TYPE_CONVERTERS_H_
#define COMPONENTS_ARC_BITMAP_BITMAP_TYPE_CONVERTERS_H_

#include "components/arc/common/bitmap.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace mojo {

template <>
struct TypeConverter<SkBitmap, arc::mojom::ArcBitmapPtr> {
  static SkBitmap Convert(const arc::mojom::ArcBitmapPtr& bitmap);
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_BITMAP_BITMAP_TYPE_CONVERTERS_H_
