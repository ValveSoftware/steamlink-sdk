// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/round_rect_painter.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"

namespace views {

RoundRectPainter::RoundRectPainter(SkColor border_color, int corner_radius)
    : border_color_(border_color),
      corner_radius_(corner_radius) {
}

RoundRectPainter::~RoundRectPainter() {
}

gfx::Size RoundRectPainter::GetMinimumSize() const {
  return gfx::Size(1, 1);
}

void RoundRectPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  SkPaint paint;
  paint.setColor(border_color_);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(1);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  gfx::Rect rect(size);
  rect.Inset(0, 0, 1, 1);
  SkRect skia_rect = gfx::RectToSkRect(rect);
  skia_rect.offset(.5, .5);
  canvas->sk_canvas()->drawRoundRect(skia_rect, SkIntToScalar(corner_radius_),
      SkIntToScalar(corner_radius_), paint);
}

}  // namespace views
