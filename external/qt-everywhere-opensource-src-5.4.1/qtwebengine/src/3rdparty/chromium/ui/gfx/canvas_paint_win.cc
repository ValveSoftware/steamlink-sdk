// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/rect.h"

namespace gfx {

CanvasSkiaPaint::CanvasSkiaPaint(HWND hwnd, HDC dc, const PAINTSTRUCT& ps)
    : hwnd_(hwnd),
      paint_dc_(dc) {
  memset(&ps_, 0, sizeof(ps_));
  ps_.rcPaint.left = ps.rcPaint.left;
  ps_.rcPaint.right = ps.rcPaint.right;
  ps_.rcPaint.top = ps.rcPaint.top;
  ps_.rcPaint.bottom = ps.rcPaint.bottom;
  Init(true);
}

CanvasSkiaPaint::CanvasSkiaPaint(HDC dc, bool opaque, int x, int y,
                                 int w, int h)
    : hwnd_(NULL),
      paint_dc_(dc) {
  memset(&ps_, 0, sizeof(ps_));
  ps_.rcPaint.left = x;
  ps_.rcPaint.right = x + w;
  ps_.rcPaint.top = y;
  ps_.rcPaint.bottom = y + h;
  Init(opaque);
}

CanvasSkiaPaint::~CanvasSkiaPaint() {
  if (!is_empty()) {
    skia::PlatformCanvas* canvas = platform_canvas();
    canvas->restoreToCount(1);
    // Commit the drawing to the screen
    skia::DrawToNativeContext(canvas, paint_dc_, ps_.rcPaint.left,
                              ps_.rcPaint.top, NULL);
  }
}

gfx::Rect CanvasSkiaPaint::GetInvalidRect() const {
  return gfx::Rect(paint_struct().rcPaint);
}

void CanvasSkiaPaint::Init(bool opaque) {
  // FIXME(brettw) for ClearType, we probably want to expand the bounds of
  // painting by one pixel so that the boundaries will be correct (ClearType
  // text can depend on the adjacent pixel). Then we would paint just the
  // inset pixels to the screen.
  const int width = ps_.rcPaint.right - ps_.rcPaint.left;
  const int height = ps_.rcPaint.bottom - ps_.rcPaint.top;

  RecreateBackingCanvas(gfx::Size(width, height),
      gfx::win::GetDeviceScaleFactor(),
      opaque);
  skia::PlatformCanvas* canvas = platform_canvas();

  canvas->clear(SkColorSetARGB(0, 0, 0, 0));

  // This will bring the canvas into the screen coordinate system for the
  // dirty rect
  canvas->translate(
      -ps_.rcPaint.left / gfx::win::GetDeviceScaleFactor(),
      -ps_.rcPaint.top / gfx::win::GetDeviceScaleFactor());
}

}  // namespace gfx
