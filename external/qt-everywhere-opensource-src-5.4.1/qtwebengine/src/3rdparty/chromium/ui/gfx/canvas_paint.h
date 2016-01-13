// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CANVAS_PAINT_H_
#define UI_GFX_CANVAS_PAINT_H_

#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

class Canvas;
class Rect;

class CanvasPaint {
 public:
  // Creates a canvas that paints to |view| when it is destroyed. The canvas is
  // sized to the client area of |view|.
  GFX_EXPORT static CanvasPaint* CreateCanvasPaint(gfx::NativeView view);

  virtual ~CanvasPaint() {}

  // Returns true if the canvas has an invalid rect that needs to be repainted.
  virtual bool IsValid() const = 0;

  // Returns the rectangle that is invalid.
  virtual gfx::Rect GetInvalidRect() const = 0;

  // Returns the underlying Canvas.
  virtual Canvas* AsCanvas() = 0;
};

}  // namespace gfx

#endif  // UI_GFX_CANVAS_PAINT_H_
