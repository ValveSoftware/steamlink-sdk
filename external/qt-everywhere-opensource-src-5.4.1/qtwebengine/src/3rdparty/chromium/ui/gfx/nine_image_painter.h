// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NINE_IMAGE_PAINTER_H_
#define UI_GFX_NINE_IMAGE_PAINTER_H_

#include "base/logging.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {

class Canvas;
class Insets;
class Rect;

class GFX_EXPORT NineImagePainter {
 public:
  explicit NineImagePainter(const std::vector<ImageSkia>& images);
  NineImagePainter(const ImageSkia& image, const Insets& insets);
  ~NineImagePainter();

  bool IsEmpty() const;
  Size GetMinimumSize() const;
  void Paint(Canvas* canvas, const Rect& bounds);
  void Paint(Canvas* canvas, const Rect& bounds, uint8 alpha);

 private:
  // Images are numbered as depicted below.
  //  ____________________
  // |__i0__|__i1__|__i2__|
  // |__i3__|__i4__|__i5__|
  // |__i6__|__i7__|__i8__|
  ImageSkia images_[9];

  DISALLOW_COPY_AND_ASSIGN(NineImagePainter);
};

}  // namespace gfx

#endif  // UI_GFX_NINE_IMAGE_PAINTER_H_
