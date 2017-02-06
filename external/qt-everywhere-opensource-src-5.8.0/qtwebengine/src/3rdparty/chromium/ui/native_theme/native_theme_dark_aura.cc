// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_dark_aura.h"

#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"

namespace ui {

NativeThemeDarkAura* NativeThemeDarkAura::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeDarkAura, s_native_theme, ());
  return &s_native_theme;
}

SkColor NativeThemeDarkAura::GetSystemColor(ColorId color_id) const {
  if (!ui::MaterialDesignController::IsModeMaterial())
    return NativeThemeAura::GetSystemColor(color_id);

  static const SkColor kButtonEnabledColor = SK_ColorWHITE;
  static const SkColor kLinkEnabledColor = gfx::kGoogleBlue300;

  static const SkColor kTextfieldDefaultColor = SK_ColorWHITE;
  static const SkColor kTextfieldDefaultBackground =
      SkColorSetRGB(0x62, 0x62, 0x62);
  static const SkColor kTextfieldSelectionBackgroundFocused =
      SkColorSetA(gfx::kGoogleBlue700, 0xCC);

  static const SkColor kResultsTableNormalBackground =
      SkColorSetRGB(0x28, 0x28, 0x28);
  static const SkColor kResultsTableText = SK_ColorWHITE;
  static const SkColor kResultsTableDimmedText =
      SkColorSetA(kResultsTableText, 0x80);

  switch (color_id) {
    // Button
    case kColorId_ButtonEnabledColor:
      return kButtonEnabledColor;
    case kColorId_CallToActionColor:
      return kLinkEnabledColor;

    // Link
    case kColorId_LinkEnabled:
    case kColorId_LinkPressed:
      return kLinkEnabledColor;

    // Textfield
    case kColorId_TextfieldDefaultColor:
    case kColorId_TextfieldSelectionColor:
      return kTextfieldDefaultColor;
    case kColorId_TextfieldDefaultBackground:
      return kTextfieldDefaultBackground;
    case kColorId_TextfieldSelectionBackgroundFocused:
      return kTextfieldSelectionBackgroundFocused;

    // Results Tables
    case kColorId_ResultsTableNormalBackground:
      return kResultsTableNormalBackground;
    case kColorId_ResultsTableNormalText:
    case kColorId_ResultsTableHoveredText:
    case kColorId_ResultsTableSelectedText:
      return kResultsTableText;
    case kColorId_ResultsTableNormalDimmedText:
    case kColorId_ResultsTableHoveredDimmedText:
    case kColorId_ResultsTableSelectedDimmedText:
      return kResultsTableDimmedText;

    // FocusableBorder
    case kColorId_FocusedBorderColor:
      return gfx::kGoogleBlue300;

    // Intentional pass-throughs to NativeThemeAura.
    case kColorId_TextOnCallToActionColor:
    case kColorId_ResultsTableHoveredBackground:
    case kColorId_ResultsTableSelectedBackground:
    case kColorId_ResultsTableNormalUrl:
    case kColorId_ResultsTableHoveredUrl:
    case kColorId_ResultsTableSelectedUrl:
      return NativeThemeAura::GetSystemColor(color_id);

    // Any other color is not defined and shouldn't be used in a dark theme.
    case kColorId_WindowBackground:
    case kColorId_DialogBackground:
    case kColorId_BubbleBackground:
    case kColorId_UnfocusedBorderColor:
    case kColorId_ButtonBackgroundColor:
    case kColorId_ButtonDisabledColor:
    case kColorId_ButtonHighlightColor:
    case kColorId_ButtonHoverColor:
    case kColorId_ButtonHoverBackgroundColor:
    case kColorId_BlueButtonEnabledColor:
    case kColorId_BlueButtonDisabledColor:
    case kColorId_BlueButtonPressedColor:
    case kColorId_BlueButtonHoverColor:
    case kColorId_BlueButtonShadowColor:
    case kColorId_EnabledMenuItemForegroundColor:
    case kColorId_DisabledMenuItemForegroundColor:
    case kColorId_DisabledEmphasizedMenuItemForegroundColor:
    case kColorId_SelectedMenuItemForegroundColor:
    case kColorId_FocusedMenuItemBackgroundColor:
    case kColorId_HoverMenuItemBackgroundColor:
    case kColorId_MenuSeparatorColor:
    case kColorId_MenuBackgroundColor:
    case kColorId_MenuBorderColor:
    case kColorId_EnabledMenuButtonBorderColor:
    case kColorId_FocusedMenuButtonBorderColor:
    case kColorId_HoverMenuButtonBorderColor:
    case kColorId_LabelEnabledColor:
    case kColorId_LabelDisabledColor:
    case kColorId_LabelBackgroundColor:
    case kColorId_LinkDisabled:
    case kColorId_TextfieldReadOnlyColor:
    case kColorId_TextfieldReadOnlyBackground:
    case kColorId_TooltipBackground:
    case kColorId_TooltipText:
    case kColorId_TreeBackground:
    case kColorId_TreeText:
    case kColorId_TreeSelectedText:
    case kColorId_TreeSelectedTextUnfocused:
    case kColorId_TreeSelectionBackgroundFocused:
    case kColorId_TreeSelectionBackgroundUnfocused:
    case kColorId_TreeArrow:
    case kColorId_TableBackground:
    case kColorId_TableText:
    case kColorId_TableSelectedText:
    case kColorId_TableSelectedTextUnfocused:
    case kColorId_TableSelectionBackgroundFocused:
    case kColorId_TableSelectionBackgroundUnfocused:
    case kColorId_TableGroupingIndicatorColor:
    case kColorId_ResultsTablePositiveText:
    case kColorId_ResultsTablePositiveHoveredText:
    case kColorId_ResultsTablePositiveSelectedText:
    case kColorId_ResultsTableNegativeText:
    case kColorId_ResultsTableNegativeHoveredText:
    case kColorId_ResultsTableNegativeSelectedText:
    case kColorId_ThrobberSpinningColor:
    case kColorId_ThrobberWaitingColor:
    case kColorId_ThrobberLightColor:
    case kColorId_NumColors:
      return gfx::kPlaceholderColor;
  }

  NOTREACHED();
  return gfx::kPlaceholderColor;
}

NativeThemeDarkAura::NativeThemeDarkAura() {}

NativeThemeDarkAura::~NativeThemeDarkAura() {}

}  // namespace ui
