// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_INTERFACES_BITMAP_ARRAY_STRUCT_TRAITS_H_
#define SKIA_PUBLIC_INTERFACES_BITMAP_ARRAY_STRUCT_TRAITS_H_

#include <vector>

#include "skia/public/interfaces/bitmap_array.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace mojo {

template <>
struct StructTraits<skia::mojom::BitmapArrayDataView, std::vector<SkBitmap>> {
  static const std::vector<SkBitmap>& bitmaps(
      const std::vector<SkBitmap>& bitmaps) {
    return bitmaps;
  }

  static bool Read(skia::mojom::BitmapArrayDataView data,
                   std::vector<SkBitmap>* out) {
    return data.ReadBitmaps(out);
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_INTERFACES_BITMAP_ARRAY_STRUCT_TRAITS_H_
