// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_bar.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/gfx/canvas.h"

namespace {

// Progress bar's border width.
const int kBorderWidth = 1;

// Corner radius for the progress bar's border.
const int kCornerRadius = 2;

// The width of the highlight at the right of the progress bar.
const int kHighlightWidth = 18;

const SkColor kBackgroundColor = SkColorSetRGB(230, 230, 230);
const SkColor kBackgroundBorderColor = SkColorSetRGB(208, 208, 208);
const SkColor kBarBorderColor = SkColorSetRGB(65, 137, 237);
const SkColor kBarTopColor = SkColorSetRGB(110, 188, 249);
const SkColor kBarColorStart = SkColorSetRGB(86, 167, 247);
const SkColor kBarColorEnd = SkColorSetRGB(76, 148, 245);
const SkColor kBarHighlightEnd = SkColorSetRGB(114, 206, 251);
const SkColor kDisabledBarBorderColor = SkColorSetRGB(191, 191, 191);
const SkColor kDisabledBarColorStart = SkColorSetRGB(224, 224, 224);
const SkColor kDisabledBarColorEnd = SkColorSetRGB(212, 212, 212);

void AddRoundRectPathWithPadding(int x, int y,
                                 int w, int h,
                                 int corner_radius,
                                 SkScalar padding,
                                 SkPath* path) {
  DCHECK(path);
  SkRect rect;
  rect.set(
      SkIntToScalar(x) + padding, SkIntToScalar(y) + padding,
      SkIntToScalar(x + w) - padding, SkIntToScalar(y + h) - padding);
  path->addRoundRect(
      rect,
      SkIntToScalar(corner_radius) - padding,
      SkIntToScalar(corner_radius) - padding);
}

void AddRoundRectPath(int x, int y,
                      int w, int h,
                      int corner_radius,
                      SkPath* path) {
  AddRoundRectPathWithPadding(x, y, w, h, corner_radius, SK_ScalarHalf, path);
}

void FillRoundRect(gfx::Canvas* canvas,
                   int x, int y,
                   int w, int h,
                   int corner_radius,
                   const SkColor colors[],
                   const SkScalar points[],
                   int count,
                   bool gradient_horizontal) {
  SkPath path;
  AddRoundRectPath(x, y, w, h, corner_radius, &path);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);

  SkPoint p[2];
  p[0].iset(x, y);
  if (gradient_horizontal) {
    p[1].iset(x + w, y);
  } else {
    p[1].iset(x, y + h);
  }
  paint.setShader(SkGradientShader::MakeLinear(p, colors, points, count,
                                               SkShader::kClamp_TileMode));

  canvas->DrawPath(path, paint);
}

void FillRoundRect(gfx::Canvas* canvas,
                   int x, int y,
                   int w, int h,
                   int corner_radius,
                   SkColor gradient_start_color,
                   SkColor gradient_end_color,
                   bool gradient_horizontal) {
  if (gradient_start_color != gradient_end_color) {
    SkColor colors[2] = { gradient_start_color, gradient_end_color };
    FillRoundRect(canvas, x, y, w, h, corner_radius,
                  colors, NULL, 2, gradient_horizontal);
  } else {
    SkPath path;
    AddRoundRectPath(x, y, w, h, corner_radius, &path);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(gradient_start_color);
    canvas->DrawPath(path, paint);
  }
}

void StrokeRoundRect(gfx::Canvas* canvas,
                     int x, int y,
                     int w, int h,
                     int corner_radius,
                     SkColor stroke_color,
                     int stroke_width) {
  SkPath path;
  AddRoundRectPath(x, y, w, h, corner_radius, &path);
  SkPaint paint;
  paint.setShader(NULL);
  paint.setColor(stroke_color);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStrokeWidth(SkIntToScalar(stroke_width));
  canvas->DrawPath(path, paint);
}

}  // namespace

namespace views {

// static
const char ProgressBar::kViewClassName[] = "ProgressBar";

ProgressBar::ProgressBar()
    : min_display_value_(0.0),
      max_display_value_(1.0),
      current_value_(0.0) {
}

ProgressBar::~ProgressBar() {
}

double ProgressBar::GetNormalizedValue() const {
  const double capped_value = std::min(
      std::max(current_value_, min_display_value_), max_display_value_);
  return (capped_value - min_display_value_) /
      (max_display_value_ - min_display_value_);
}

void ProgressBar::SetDisplayRange(double min_display_value,
                                  double max_display_value) {
  if (min_display_value != min_display_value_ ||
      max_display_value != max_display_value_) {
    DCHECK(min_display_value < max_display_value);
    min_display_value_ = min_display_value;
    max_display_value_ = max_display_value;
    SchedulePaint();
  }
}

void ProgressBar::SetValue(double value) {
  if (value != current_value_) {
    current_value_ = value;
    SchedulePaint();
  }
}

void ProgressBar::SetTooltipText(const base::string16& tooltip_text) {
  tooltip_text_ = tooltip_text;
}

bool ProgressBar::GetTooltipText(const gfx::Point& p,
                                 base::string16* tooltip) const {
  DCHECK(tooltip);
  *tooltip = tooltip_text_;
  return !tooltip_text_.empty();
}

void ProgressBar::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_PROGRESS_INDICATOR;
  state->AddStateFlag(ui::AX_STATE_READ_ONLY);
}

gfx::Size ProgressBar::GetPreferredSize() const {
  gfx::Size pref_size(100, 11);
  gfx::Insets insets = GetInsets();
  pref_size.Enlarge(insets.width(), insets.height());
  return pref_size;
}

const char* ProgressBar::GetClassName() const {
  return kViewClassName;
}

void ProgressBar::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect content_bounds = GetContentsBounds();
  int bar_left = content_bounds.x();
  int bar_top = content_bounds.y();
  int bar_width = content_bounds.width();
  int bar_height = content_bounds.height();

  const int progress_width =
      static_cast<int>(bar_width * GetNormalizedValue() + 0.5);

  // Draw background.
  FillRoundRect(canvas,
                bar_left, bar_top, bar_width, bar_height,
                kCornerRadius,
                kBackgroundColor, kBackgroundColor,
                false);
  StrokeRoundRect(canvas,
                  bar_left, bar_top,
                  bar_width, bar_height,
                  kCornerRadius,
                  kBackgroundBorderColor,
                  kBorderWidth);

  if (progress_width > 1) {
    // Draw inner if wide enough.
    if (progress_width > kBorderWidth * 2) {
      canvas->Save();

      SkPath inner_path;
      AddRoundRectPathWithPadding(
          bar_left, bar_top, progress_width, bar_height,
          kCornerRadius,
          0,
          &inner_path);
      canvas->ClipPath(inner_path, false);

      const SkColor bar_colors[] = {
        kBarTopColor,
        kBarTopColor,
        kBarColorStart,
        kBarColorEnd,
        kBarColorEnd,
      };
      // We want a thin 1-pixel line for kBarTopColor.
      SkScalar scalar_height = SkIntToScalar(bar_height);
      SkScalar highlight_width = 1 / scalar_height;
      SkScalar border_width = kBorderWidth / scalar_height;
      const SkScalar bar_points[] = {
        0,
        border_width,
        border_width + highlight_width,
        SK_Scalar1 - border_width,
        SK_Scalar1,
      };

      const SkColor disabled_bar_colors[] = {
        kDisabledBarColorStart,
        kDisabledBarColorStart,
        kDisabledBarColorEnd,
        kDisabledBarColorEnd,
      };

      const SkScalar disabled_bar_points[] = {
        0,
        border_width,
        SK_Scalar1 - border_width,
        SK_Scalar1
      };

      // Do not start from (kBorderWidth, kBorderWidth) because it makes gaps
      // between the inner and the border.
      FillRoundRect(canvas,
                    bar_left, bar_top,
                    progress_width, bar_height,
                    kCornerRadius,
                    enabled() ? bar_colors : disabled_bar_colors,
                    enabled() ? bar_points : disabled_bar_points,
                    enabled() ? arraysize(bar_colors) :
                        arraysize(disabled_bar_colors),
                    false);

      if (enabled()) {
        // Draw the highlight to the right.
        const SkColor highlight_colors[] = {
          SkColorSetA(kBarHighlightEnd, 0),
          kBarHighlightEnd,
          kBarHighlightEnd,
        };
        const SkScalar highlight_points[] = {
            0, SK_Scalar1 - kBorderWidth / scalar_height, SK_Scalar1,
        };
        SkPaint paint;
        paint.setStyle(SkPaint::kFill_Style);
        paint.setFlags(SkPaint::kAntiAlias_Flag);

        SkPoint p[2];
        int highlight_left =
            std::max(0, progress_width - kHighlightWidth - kBorderWidth);
        p[0].iset(highlight_left, 0);
        p[1].iset(progress_width, 0);
        paint.setShader(SkGradientShader::MakeLinear(
            p, highlight_colors, highlight_points, arraysize(highlight_colors),
            SkShader::kClamp_TileMode));
        paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
        canvas->DrawRect(gfx::Rect(highlight_left, 0,
                                   kHighlightWidth + kBorderWidth, bar_height),
                         paint);
      }

      canvas->Restore();
    }

    // Draw bar stroke
    StrokeRoundRect(canvas,
                    bar_left, bar_top, progress_width, bar_height,
                    kCornerRadius,
                    enabled() ? kBarBorderColor : kDisabledBarBorderColor,
                    kBorderWidth);
  }
}

}  // namespace views
