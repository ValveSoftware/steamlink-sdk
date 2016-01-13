// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_LAYER_PAINTER_H_
#define CC_RESOURCES_LAYER_PAINTER_H_

#include "cc/base/cc_export.h"

class SkCanvas;

namespace gfx {
class Rect;
class RectF;
}

namespace cc {

class CC_EXPORT LayerPainter {
 public:
  virtual ~LayerPainter() {}
  virtual void Paint(SkCanvas* canvas,
                     const gfx::Rect& content_rect,
                     gfx::RectF* opaque) = 0;
};

}  // namespace cc

#endif  // CC_RESOURCES_LAYER_PAINTER_H_
