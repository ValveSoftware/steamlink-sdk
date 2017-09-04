// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/background.h"

#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

#if defined(OS_WIN)
#include "skia/ext/skia_utils_win.h"
#endif

namespace views {

// SolidBackground is a trivial Background implementation that fills the
// background in a solid color.
class SolidBackground : public Background {
 public:
  explicit SolidBackground(SkColor color) {
    SetNativeControlColor(color);
  }

  void Paint(gfx::Canvas* canvas, View* view) const override {
    // Fill the background. Note that we don't constrain to the bounds as
    // canvas is already clipped for us.
    canvas->DrawColor(get_color());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SolidBackground);
};

class BackgroundPainter : public Background {
 public:
  BackgroundPainter(bool owns_painter, Painter* painter)
      : owns_painter_(owns_painter), painter_(painter) {
    DCHECK(painter);
  }

  ~BackgroundPainter() override {
    if (owns_painter_)
      delete painter_;
  }

  void Paint(gfx::Canvas* canvas, View* view) const override {
    Painter::PaintPainterAt(canvas, painter_, view->GetLocalBounds());
  }

 private:
  bool owns_painter_;
  Painter* painter_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundPainter);
};

Background::Background()
    : color_(SK_ColorWHITE)
{
}

Background::~Background() {
}

void Background::SetNativeControlColor(SkColor color) {
  color_ = color;
}

// static
Background* Background::CreateSolidBackground(SkColor color) {
  return new SolidBackground(color);
}

// static
Background* Background::CreateStandardPanelBackground() {
  // TODO(beng): Should be in NativeTheme.
  return CreateSolidBackground(SK_ColorWHITE);
}

// static
Background* Background::CreateVerticalGradientBackground(SkColor color1,
                                                         SkColor color2) {
  Background* background = CreateBackgroundPainter(
      true, Painter::CreateVerticalGradient(color1, color2));
  background->SetNativeControlColor(
      color_utils::AlphaBlend(color1, color2, 128));

  return background;
}

// static
Background* Background::CreateVerticalMultiColorGradientBackground(
    SkColor* colors,
    SkScalar* pos,
    size_t count) {
  Background* background = CreateBackgroundPainter(
      true, Painter::CreateVerticalMultiColorGradient(colors, pos, count));
  background->SetNativeControlColor(
      color_utils::AlphaBlend(colors[0], colors[count-1], 128));

  return background;
}

// static
Background* Background::CreateBackgroundPainter(bool owns_painter,
                                                Painter* painter) {
  return new BackgroundPainter(owns_painter, painter);
}

}  // namespace views
