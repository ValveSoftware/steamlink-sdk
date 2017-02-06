// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/bitmap/bitmap_type_converters.h"

namespace mojo {

SkBitmap TypeConverter<SkBitmap, arc::mojom::ArcBitmapPtr>::Convert(
    const arc::mojom::ArcBitmapPtr& arcBitmap) {
  if (arcBitmap.is_null())
    return SkBitmap();

  SkImageInfo info = SkImageInfo::Make(
      arcBitmap->width, arcBitmap->height,
      kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  if (info.getSafeSize(info.minRowBytes()) > arcBitmap->pixel_data.size())
      return SkBitmap();

  // Create the SkBitmap object which wraps the arc bitmap pixels.
  SkBitmap bitmap;
  if (!bitmap.installPixels(info,
          const_cast<uint8_t*>(arcBitmap->pixel_data.storage().data()),
          info.minRowBytes())) {
    return SkBitmap();
  }

  // Copy the pixels with converting color type.
  SkBitmap nativeColorBitmap;
  if (!bitmap.copyTo(&nativeColorBitmap, kN32_SkColorType))
    return SkBitmap();

  return nativeColorBitmap;
}

}  // namespace mojo
