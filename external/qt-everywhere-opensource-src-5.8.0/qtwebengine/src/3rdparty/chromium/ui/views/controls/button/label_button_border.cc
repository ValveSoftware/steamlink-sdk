// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_border.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkArithmeticMode.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/sys_color_change_listener.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/native_theme_delegate.h"
#include "ui/views/resources/grit/views_resources.h"

namespace views {

namespace {

// Insets for the unified button images. This assumes that the images
// are of a 9 grid, of 5x5 size each.
const int kButtonInsets = 5;

// The text-button hot and pushed image IDs; normal is unadorned by default.
const int kTextHoveredImages[] = IMAGE_GRID(IDR_TEXTBUTTON_HOVER);
const int kTextPressedImages[] = IMAGE_GRID(IDR_TEXTBUTTON_PRESSED);

// A helper function to paint the appropriate broder images.
void PaintHelper(LabelButtonAssetBorder* border,
                 gfx::Canvas* canvas,
                 ui::NativeTheme::State state,
                 const gfx::Rect& rect,
                 const ui::NativeTheme::ExtraParams& extra) {
  Painter* painter =
      border->GetPainter(extra.button.is_focused,
                         Button::GetButtonStateFrom(state));
  // Paint any corresponding unfocused painter if there is no focused painter.
  if (!painter && extra.button.is_focused)
    painter = border->GetPainter(false, Button::GetButtonStateFrom(state));
  if (painter)
    Painter::PaintPainterAt(canvas, painter, rect);
}

}  // namespace

LabelButtonBorder::LabelButtonBorder() {}
LabelButtonBorder::~LabelButtonBorder() {}

bool LabelButtonBorder::PaintsButtonState(bool focused,
                                          Button::ButtonState state) {
  return false;
}

void LabelButtonBorder::Paint(const View& view, gfx::Canvas* canvas) {}

gfx::Insets LabelButtonBorder::GetInsets() const {
  return insets_;
}

gfx::Size LabelButtonBorder::GetMinimumSize() const {
  return gfx::Size();
}

LabelButtonAssetBorder::LabelButtonAssetBorder(Button::ButtonStyle style) {
  set_insets(GetDefaultInsetsForStyle(style));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::Insets insets(kButtonInsets);
  if (style == Button::STYLE_BUTTON) {
    SetPainter(false, Button::STATE_NORMAL,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_NORMAL), insets));
    SetPainter(false, Button::STATE_HOVERED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_HOVER), insets));
    SetPainter(false, Button::STATE_PRESSED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_PRESSED), insets));
    SetPainter(false, Button::STATE_DISABLED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_DISABLED), insets));
    SetPainter(true, Button::STATE_NORMAL,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_FOCUSED_NORMAL), insets));
    SetPainter(true, Button::STATE_HOVERED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_FOCUSED_HOVER), insets));
    SetPainter(true, Button::STATE_PRESSED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_FOCUSED_PRESSED), insets));
    SetPainter(true, Button::STATE_DISABLED,
               Painter::CreateImagePainter(
                   *rb.GetImageSkiaNamed(IDR_BUTTON_DISABLED), insets));
  } else if (style == Button::STYLE_TEXTBUTTON) {
    SetPainter(false, Button::STATE_HOVERED,
               Painter::CreateImageGridPainter(kTextHoveredImages));
    SetPainter(false, Button::STATE_PRESSED,
               Painter::CreateImageGridPainter(kTextPressedImages));
  }
}

LabelButtonAssetBorder::~LabelButtonAssetBorder() {}

// static
gfx::Insets LabelButtonAssetBorder::GetDefaultInsetsForStyle(
    Button::ButtonStyle style) {
  gfx::Insets insets;
  if (style == Button::STYLE_BUTTON) {
    insets = gfx::Insets(8, 13);
  } else if (style == Button::STYLE_TEXTBUTTON) {
    insets = gfx::Insets(5, 6);
  } else {
    NOTREACHED();
  }
  return insets;
}

bool LabelButtonAssetBorder::PaintsButtonState(bool focused,
                                               Button::ButtonState state) {
  // PaintHelper() above will paint the unfocused painter for a given state if
  // the button is focused, but there is no focused painter.
  return GetPainter(focused, state) || (focused && GetPainter(false, state));
}

void LabelButtonAssetBorder::Paint(const View& view, gfx::Canvas* canvas) {
  const NativeThemeDelegate* native_theme_delegate =
      static_cast<const LabelButton*>(&view);
  gfx::Rect rect(native_theme_delegate->GetThemePaintRect());
  ui::NativeTheme::ExtraParams extra;
  const gfx::Animation* animation = native_theme_delegate->GetThemeAnimation();
  ui::NativeTheme::State state = native_theme_delegate->GetThemeState(&extra);

  if (animation && animation->is_animating()) {
    // Linearly interpolate background and foreground painters during animation.
    const SkRect sk_rect = gfx::RectToSkRect(rect);
    canvas->sk_canvas()->saveLayer(&sk_rect, NULL);
    state = native_theme_delegate->GetBackgroundThemeState(&extra);
    PaintHelper(this, canvas, state, rect, extra);

    SkPaint paint;
    double scale = animation->GetCurrentValue();
    paint.setXfermode(SkArithmeticMode::Make(0.0f, scale, 1.0 - scale, 0.0));
    canvas->sk_canvas()->saveLayer(&sk_rect, &paint);
    state = native_theme_delegate->GetForegroundThemeState(&extra);
    PaintHelper(this, canvas, state, rect, extra);
    canvas->sk_canvas()->restore();

    canvas->sk_canvas()->restore();
  } else {
    PaintHelper(this, canvas, state, rect, extra);
  }
}

gfx::Size LabelButtonAssetBorder::GetMinimumSize() const {
  gfx::Size minimum_size;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < Button::STATE_COUNT; ++j) {
      if (painters_[i][j])
        minimum_size.SetToMax(painters_[i][j]->GetMinimumSize());
    }
  }
  return minimum_size;
}

Painter* LabelButtonAssetBorder::GetPainter(bool focused,
                                            Button::ButtonState state) {
  return painters_[focused ? 1 : 0][state].get();
}

void LabelButtonAssetBorder::SetPainter(bool focused,
                                        Button::ButtonState state,
                                        Painter* painter) {
  painters_[focused ? 1 : 0][state].reset(painter);
}

}  // namespace views
