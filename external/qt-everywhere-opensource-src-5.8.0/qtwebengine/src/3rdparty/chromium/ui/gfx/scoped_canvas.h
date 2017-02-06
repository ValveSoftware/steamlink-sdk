// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCOPED_CANVAS_H_
#define UI_GFX_SCOPED_CANVAS_H_

#include "base/macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class Rect;

// Saves the drawing state, and restores the state when going out of scope.
class GFX_EXPORT ScopedCanvas {
 public:
  explicit ScopedCanvas(gfx::Canvas* canvas) : canvas_(canvas) {
    if (canvas_)
      canvas_->Save();
  }
  ~ScopedCanvas() {
    if (canvas_)
      canvas_->Restore();
  }

 private:
  gfx::Canvas* canvas_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCanvas);
};

// Saves the drawing state, and applies an RTL transform for the supplied rect
// if the UI is in RTL layout. The resulting transform is such that anything
// drawn inside the supplied rect will be flipped horizontally.
class GFX_EXPORT ScopedRTLFlipCanvas {
 public:
  ScopedRTLFlipCanvas(gfx::Canvas* canvas, const gfx::Rect& rect);
  ~ScopedRTLFlipCanvas() {}

 private:
  ScopedCanvas canvas_;

  DISALLOW_COPY_AND_ASSIGN(ScopedRTLFlipCanvas);
};

}  // namespace gfx

#endif  // UI_GFX_SCOPED_CANVAS_H_
