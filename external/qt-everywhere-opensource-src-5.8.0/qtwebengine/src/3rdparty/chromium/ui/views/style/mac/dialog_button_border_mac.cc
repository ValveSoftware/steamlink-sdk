// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/mac/dialog_button_border_mac.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme_mac.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button.h"

using ui::NativeThemeMac;

namespace views {
namespace {

// Default border insets, to provide text padding.
const int kPaddingX = 19;
const int kPaddingY = 7;

NativeThemeMac::ButtonBackgroundType PaintTypeFromButton(
    const LabelButton& button) {
  if (!button.enabled() || button.state() == Button::STATE_DISABLED)
    return NativeThemeMac::ButtonBackgroundType::DISABLED;
  if (button.state() == Button::STATE_PRESSED)
    return NativeThemeMac::ButtonBackgroundType::PRESSED;
  if (DialogButtonBorderMac::ShouldRenderDefault(button))
    return NativeThemeMac::ButtonBackgroundType::HIGHLIGHTED;
  return NativeThemeMac::ButtonBackgroundType::NORMAL;
}

}  // namespace

DialogButtonBorderMac::DialogButtonBorderMac() {
  set_insets(gfx::Insets(kPaddingY, kPaddingX, kPaddingY, kPaddingX));
}

DialogButtonBorderMac::~DialogButtonBorderMac() {}

// static
bool DialogButtonBorderMac::ShouldRenderDefault(const LabelButton& button) {
  // TODO(tapted): Check whether the Widget is active, and only return true here
  // if it is. Plumbing this requires default buttons to also observe Widget
  // activations to ensure text and background colors are properly invalidated.
  return button.is_default();
}

void DialogButtonBorderMac::Paint(const View& view, gfx::Canvas* canvas) {
  // Actually, |view| should be a LabelButton as well, but don't rely too much
  // on RTTI.
  DCHECK(CustomButton::AsCustomButton(&view));
  const LabelButton& button = static_cast<const LabelButton&>(view);

  ui::NativeThemeMac::PaintStyledGradientButton(
      canvas->sk_canvas(), view.GetLocalBounds(), PaintTypeFromButton(button),
      true, true, button.HasFocus());
}

gfx::Size DialogButtonBorderMac::GetMinimumSize() const {
  // Overridden by PlatformStyle. Here, just ensure the minimum size is
  // consistent with the padding.
  return gfx::Size(2 * kPaddingX, 2 * kPaddingY);
}

}  // namespace views
