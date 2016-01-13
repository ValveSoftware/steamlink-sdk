// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focusable_border.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme.h"

namespace {

// Define the size of the insets
const int kTopInsetSize = 4;
const int kLeftInsetSize = 4;
const int kBottomInsetSize = 4;
const int kRightInsetSize = 4;

}  // namespace

namespace views {

FocusableBorder::FocusableBorder()
    : insets_(kTopInsetSize, kLeftInsetSize,
              kBottomInsetSize, kRightInsetSize),
      override_color_(SK_ColorWHITE),
      use_default_color_(true) {
}

void FocusableBorder::SetColor(SkColor color) {
  override_color_ = color;
  use_default_color_ = false;
}

void FocusableBorder::UseDefaultColor() {
  use_default_color_ = true;
}

void FocusableBorder::Paint(const View& view, gfx::Canvas* canvas) {
  SkPath path;
  path.addRect(gfx::RectToSkRect(view.GetLocalBounds()), SkPath::kCW_Direction);
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  SkColor color = override_color_;
  if (use_default_color_) {
    color = view.GetNativeTheme()->GetSystemColor(
        view.HasFocus() ? ui::NativeTheme::kColorId_FocusedBorderColor :
                          ui::NativeTheme::kColorId_UnfocusedBorderColor);
  }

  paint.setColor(color);
  paint.setStrokeWidth(SkIntToScalar(2));

  canvas->DrawPath(path, paint);
}

gfx::Insets FocusableBorder::GetInsets() const {
  return insets_;
}

gfx::Size FocusableBorder::GetMinimumSize() const {
  return gfx::Size();
}

void FocusableBorder::SetInsets(int top, int left, int bottom, int right) {
  insets_.Set(top, left, bottom, right);
}

}  // namespace views
