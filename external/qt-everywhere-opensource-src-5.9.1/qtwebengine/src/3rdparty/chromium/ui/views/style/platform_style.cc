// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/shadow_value.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/scrollbar/scroll_bar_views.h"

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define DESKTOP_LINUX
#endif

namespace views {
namespace {

#if !defined(DESKTOP_LINUX) && !defined(OS_MACOSX)
// Default text and shadow colors for STYLE_BUTTON.
const SkColor kStyleButtonTextColor = SK_ColorBLACK;
const SkColor kStyleButtonShadowColor = SK_ColorWHITE;
#endif

}  // namespace

#if !defined(OS_MACOSX)

const int PlatformStyle::kComboboxNormalArrowPadding = 7;
const int PlatformStyle::kMinLabelButtonWidth = 70;
const int PlatformStyle::kMinLabelButtonHeight = 33;
const bool PlatformStyle::kDefaultLabelButtonHasBoldFont = true;
const bool PlatformStyle::kDialogDefaultButtonCanBeCancel = true;
const bool PlatformStyle::kTextDragVerticallyDragsToEnd = false;
const CustomButton::NotifyAction PlatformStyle::kMenuNotifyActivationAction =
    CustomButton::NOTIFY_ON_RELEASE;
const bool PlatformStyle::kTreeViewHasFocusRing = false;
const bool PlatformStyle::kTreeViewSelectionPaintsEntireRow = false;
const bool PlatformStyle::kUseRipples = true;

// static
gfx::ImageSkia PlatformStyle::CreateComboboxArrow(bool is_enabled,
                                                  Combobox::Style style) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return *rb.GetImageSkiaNamed(IDR_MENU_DROPARROW);
}

// static
std::unique_ptr<ScrollBar> PlatformStyle::CreateScrollBar(bool is_horizontal) {
  return base::MakeUnique<ScrollBarViews>(is_horizontal);
}

// static
SkColor PlatformStyle::TextColorForButton(
    const ButtonColorByState& color_by_state,
    const LabelButton& button) {
  return color_by_state[button.state()];
}

// static
void PlatformStyle::OnTextfieldEditFailed() {}

#endif  // OS_MACOSX

#if !defined(DESKTOP_LINUX) && !defined(OS_MACOSX)
// static
void PlatformStyle::ApplyLabelButtonTextStyle(
    Label* label,
    ButtonColorByState* color_by_state) {
  ButtonColorByState& colors = *color_by_state;
  colors[Button::STATE_NORMAL] = kStyleButtonTextColor;
  colors[Button::STATE_HOVERED] = kStyleButtonTextColor;
  colors[Button::STATE_PRESSED] = kStyleButtonTextColor;

  label->SetShadows(gfx::ShadowValues(
      1, gfx::ShadowValue(gfx::Vector2d(0, 1), 0, kStyleButtonShadowColor)));
}
#endif

#if !defined(DESKTOP_LINUX)
// static
std::unique_ptr<Border> PlatformStyle::CreateThemedLabelButtonBorder(
    LabelButton* button) {
  return button->CreateDefaultBorder();
}
#endif

}  // namespace views
