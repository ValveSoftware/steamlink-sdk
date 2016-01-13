// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_util.h"

#include "base/logging.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"

namespace ui {

void ScaleAndRotateCursorBitmapAndHotpoint(float scale,
                                           gfx::Display::Rotation rotation,
                                           SkBitmap* bitmap,
                                           gfx::Point* hotpoint) {
  switch (rotation) {
    case gfx::Display::ROTATE_0:
      break;
    case gfx::Display::ROTATE_90:
      hotpoint->SetPoint(bitmap->height() - hotpoint->y(), hotpoint->x());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_90_CW);
      break;
    case gfx::Display::ROTATE_180:
      hotpoint->SetPoint(
          bitmap->width() - hotpoint->x(), bitmap->height() - hotpoint->y());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_180_CW);
      break;
    case gfx::Display::ROTATE_270:
      hotpoint->SetPoint(hotpoint->y(), bitmap->width() - hotpoint->x());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_270_CW);
      break;
  }

  if (scale < FLT_EPSILON) {
    NOTREACHED() << "Scale must be larger than 0.";
    scale = 1.0f;
  }

  if (scale == 1.0f)
    return;

  gfx::Size scaled_size = gfx::ToFlooredSize(
      gfx::ScaleSize(gfx::Size(bitmap->width(), bitmap->height()), scale));

  *bitmap = skia::ImageOperations::Resize(
      *bitmap,
      skia::ImageOperations::RESIZE_BETTER,
      scaled_size.width(),
      scaled_size.height());
  *hotpoint = gfx::ToFlooredPoint(gfx::ScalePoint(*hotpoint, scale));
}

}  // namespace ui
