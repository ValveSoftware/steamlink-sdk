// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BACKGROUND_H_
#define UI_VIEWS_BACKGROUND_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // defined(OS_WIN)

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
}

namespace views {

class Painter;
class View;

/////////////////////////////////////////////////////////////////////////////
//
// Background class
//
// A background implements a way for views to paint a background. The
// background can be either solid or based on a gradient. Of course,
// Background can be subclassed to implement various effects.
//
// Any View can have a background. See View::SetBackground() and
// View::OnPaintBackground()
//
/////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT Background {
 public:
  Background();
  virtual ~Background();

  // Creates a background that fills the canvas in the specified color.
  static Background* CreateSolidBackground(SkColor color);

  // Creates a background that fills the canvas in the specified color.
  static Background* CreateSolidBackground(int r, int g, int b) {
    return CreateSolidBackground(SkColorSetRGB(r, g, b));
  }

  // Creates a background that fills the canvas in the specified color.
  static Background* CreateSolidBackground(int r, int g, int b, int a) {
    return CreateSolidBackground(SkColorSetARGB(a, r, g, b));
  }

  // Creates a background that contains a vertical gradient that varies
  // from |color1| to |color2|
  static Background* CreateVerticalGradientBackground(SkColor color1,
                                                      SkColor color2);

  // Creates a background that contains a vertical gradient. The gradient can
  // have multiple |colors|. The |pos| array contains the relative positions of
  // each corresponding color. |colors| and |pos| must be the same size. The
  // first element in |pos| must be 0.0 and the last element must be 1.0.
  // |count| contains the number of elements in |colors| and |pos|.
  static Background* CreateVerticalMultiColorGradientBackground(SkColor* colors,
                                                                SkScalar* pos,
                                                                size_t count);

  // Creates Chrome's standard panel background
  static Background* CreateStandardPanelBackground();

  // Creates a Background from the specified Painter. If owns_painter is
  // true, the Painter is deleted when the Border is deleted.
  static Background* CreateBackgroundPainter(bool owns_painter,
                                             Painter* painter);

  // Render the background for the provided view
  virtual void Paint(gfx::Canvas* canvas, View* view) const = 0;

  // Set a solid, opaque color to be used when drawing backgrounds of native
  // controls.  Unfortunately alpha=0 is not an option.
  void SetNativeControlColor(SkColor color);

  // Returns the "background color".  This is equivalent to the color set in
  // SetNativeControlColor().  For solid backgrounds, this is the color; for
  // gradient backgrounds, it's the midpoint of the gradient; for painter
  // backgrounds, this is not useful (returns a default color).
  SkColor get_color() const { return color_; }

#if defined(OS_WIN)
  // TODO(port): Make GetNativeControlBrush portable (currently uses HBRUSH).

  // Get the brush that was specified by SetNativeControlColor
  HBRUSH GetNativeControlBrush() const;
#endif  // defined(OS_WIN)

 private:
  SkColor color_;
#if defined(OS_WIN)
  // TODO(port): Create portable replacement for HBRUSH.
  mutable HBRUSH native_control_brush_;
#endif  // defined(OS_WIN)

  DISALLOW_COPY_AND_ASSIGN(Background);
};

}  // namespace views

#endif  // UI_VIEWS_BACKGROUND_H_
