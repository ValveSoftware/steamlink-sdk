// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_INTERFACES_BITMAP_SKBITMAP_STRUCT_TRAITS_H_
#define SKIA_PUBLIC_INTERFACES_BITMAP_SKBITMAP_STRUCT_TRAITS_H_

#include "mojo/public/cpp/bindings/array_traits.h"
#include "skia/public/interfaces/bitmap.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace mojo {

// A buffer used to read pixel data directly from BitmapDataView to SkBitmap.
using BitmapBuffer = CArray<uint8_t>;

// Struct traits to use SkBitmap for skia::mojom::Bitmap in Chrome C++ code.
template <>
struct StructTraits<skia::mojom::BitmapDataView, SkBitmap> {
  static bool IsNull(const SkBitmap& b);
  static void SetToNull(SkBitmap* b);
  static skia::mojom::ColorType color_type(const SkBitmap& b);
  static skia::mojom::AlphaType alpha_type(const SkBitmap& b);
  static skia::mojom::ColorProfileType profile_type(const SkBitmap& b);
  static uint32_t width(const SkBitmap& b);
  static uint32_t height(const SkBitmap& b);
  static uint64_t row_bytes(const SkBitmap& b);
  static BitmapBuffer pixel_data(const SkBitmap& b);
  static bool Read(skia::mojom::BitmapDataView data, SkBitmap* b);
  static void* SetUpContext(const SkBitmap& b);
  static void TearDownContext(const SkBitmap& b, void* context);
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_INTERFACES_BITMAP_SKBITMAP_STRUCT_TRAITS_H_
