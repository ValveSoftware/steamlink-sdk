// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focusable_rounded_border_mac.h"

#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme_mac.h"

namespace {

const int kThickness = 1;

}  // namespace

namespace views {

FocusableRoundedBorder::FocusableRoundedBorder() {
  // TODO(ellyjones): These insets seem like they shouldn't be big enough, but
  // they are, and insetting by corner_radius_ instead produces gargantuan
  // padding. Why is that?
  SetInsets(kThickness, kThickness, kThickness, kThickness);
}

FocusableRoundedBorder::~FocusableRoundedBorder() {}

// For now, this is similar to RoundedRectBorder::Paint(), but this method will
// likely diverge in future.
// TODO(ellyjones): Diverge it by adding soft focus rings.
void FocusableRoundedBorder::Paint(const View& view, gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(kThickness);
  paint.setColor(GetCurrentColor(view));
  paint.setAntiAlias(true);

  float half_thickness = kThickness / 2.0f;
  gfx::RectF bounds(view.GetLocalBounds());
  bounds.Inset(half_thickness, half_thickness);
  canvas->DrawRoundRect(bounds, ui::NativeThemeMac::kButtonCornerRadius,
                        paint);
}

}  // namespace views
