// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#include "base/memory/ptr_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/label_button.h"
#import "ui/views/controls/scrollbar/cocoa_scroll_bar.h"

#import <Cocoa/Cocoa.h>

namespace views {

const int PlatformStyle::kComboboxNormalArrowPadding = 0;
const int PlatformStyle::kMinLabelButtonWidth = 32;
const int PlatformStyle::kMinLabelButtonHeight = 30;
const bool PlatformStyle::kDefaultLabelButtonHasBoldFont = false;
const bool PlatformStyle::kDialogDefaultButtonCanBeCancel = false;
const bool PlatformStyle::kTextDragVerticallyDragsToEnd = true;
const bool PlatformStyle::kTreeViewHasFocusRing = true;
const bool PlatformStyle::kTreeViewSelectionPaintsEntireRow = true;
const bool PlatformStyle::kUseRipples = false;

const CustomButton::NotifyAction PlatformStyle::kMenuNotifyActivationAction =
    CustomButton::NOTIFY_ON_PRESS;

// static
gfx::ImageSkia PlatformStyle::CreateComboboxArrow(bool is_enabled,
                                                  Combobox::Style style) {
  // TODO(ellyjones): IDR_MENU_DROPARROW is a cross-platform image that doesn't
  // look right on Mac. See https://crbug.com/384071.
  if (style == Combobox::STYLE_ACTION) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    return *rb.GetImageSkiaNamed(IDR_MENU_DROPARROW);
  }
  const int kComboboxArrowWidth = 24;
  return gfx::CreateVectorIcon(
      is_enabled ? gfx::VectorIconId::COMBOBOX_ARROW_MAC_ENABLED
                 : gfx::VectorIconId::COMBOBOX_ARROW_MAC_DISABLED,
      kComboboxArrowWidth, SK_ColorBLACK);
}

// static
std::unique_ptr<ScrollBar> PlatformStyle::CreateScrollBar(bool is_horizontal) {
  return base::MakeUnique<CocoaScrollBar>(is_horizontal);
}

// static
SkColor PlatformStyle::TextColorForButton(
    const ButtonColorByState& color_by_state,
    const LabelButton& button) {
  Button::ButtonState state = button.state();
  if (button.style() == Button::STYLE_BUTTON && button.is_default()) {
    // For convenience, we currently assume Mac wants the color corresponding to
    // the pressed state for default buttons.
    state = Button::STATE_PRESSED;
  }
  return color_by_state[state];
}

// static
void PlatformStyle::ApplyLabelButtonTextStyle(
    views::Label* label,
    ButtonColorByState* color_by_state) {
  ButtonColorByState& colors = *color_by_state;
  colors[Button::STATE_PRESSED] = SK_ColorWHITE;
}

// static
void PlatformStyle::OnTextfieldEditFailed() {
  NSBeep();
}

}  // namespace views
