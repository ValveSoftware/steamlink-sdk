// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CANVAS_PAINT_WIN_H_
#define UI_GFX_CANVAS_PAINT_WIN_H_

#include "skia/ext/platform_canvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size.h"
#include "ui/gfx/win/dpi.h"

namespace gfx {

// A class designed to help with WM_PAINT operations on Windows. It will create
// the bitmap and canvas with the correct size and transform for the dirty rect.
// The bitmap will be automatically painted to the screen on destruction.
//
// You MUST call isEmpty before painting to determine if anything needs
// painting. Sometimes the dirty rect can actually be empty, and this makes
// the bitmap functions we call unhappy. The caller should not paint in this
// case.
//
// Therefore, all you need to do is:
//   case WM_PAINT: {
//     PAINTSTRUCT ps;
//     HDC hdc = BeginPaint(hwnd, &ps);
//     gfx::CanvasSkiaPaint canvas(hwnd, hdc, ps);
//     if (!canvas.isEmpty()) {
//       ... paint to the canvas ...
//     }
//     EndPaint(hwnd, &ps);
//     return 0;
//   }
// Note: The created context is always inialized to (0, 0, 0, 0).
class GFX_EXPORT CanvasSkiaPaint : public Canvas {
 public:
  // This constructor assumes the canvas is opaque.
  CanvasSkiaPaint(HWND hwnd, HDC dc, const PAINTSTRUCT& ps);
  virtual ~CanvasSkiaPaint();

  // Creates a CanvasSkiaPaint for the specified region that paints to the
  // specified dc.
  CanvasSkiaPaint(HDC dc, bool opaque, int x, int y, int w, int h);

  // Returns the rectangle that is invalid.
  virtual gfx::Rect GetInvalidRect() const;

  // Returns true if the invalid region is empty. The caller should call this
  // function to determine if anything needs painting.
  bool is_empty() const {
    return ps_.rcPaint.right - ps_.rcPaint.left == 0 ||
           ps_.rcPaint.bottom - ps_.rcPaint.top == 0;
  };

  // Use to access the Windows painting parameters, especially useful for
  // getting the bounding rect for painting: paintstruct().rcPaint
  const PAINTSTRUCT& paint_struct() const { return ps_; }

  // Returns the DC that will be painted to
  HDC paint_dc() const { return paint_dc_; }

 private:
  void Init(bool opaque);

  HWND hwnd_;
  HDC paint_dc_;
  PAINTSTRUCT ps_;

  // Disallow copy and assign.
  DISALLOW_COPY_AND_ASSIGN(CanvasSkiaPaint);
};

}  // namespace gfx

#endif  // UI_GFX_CANVAS_PAINT_WIN_H_
