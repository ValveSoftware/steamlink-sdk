// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/common_theme.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/resources/grit/ui_resources.h"

namespace ui {

SkColor GetAuraColor(NativeTheme::ColorId color_id,
                     const NativeTheme* base_theme) {
  // MD colors.
  if (ui::MaterialDesignController::IsModeMaterial()) {
    // Dialogs:
    static const SkColor kDialogBackgroundColorMd = SK_ColorWHITE;
    // Buttons:
    static const SkColor kButtonEnabledColorMd = gfx::kChromeIconGrey;
    // MenuItem:
    static const SkColor kMenuHighlightBackgroundColorMd =
        SkColorSetARGB(0x14, 0x00, 0x00, 0x00);
    static const SkColor kSelectedMenuItemForegroundColorMd = SK_ColorBLACK;
    // Link:
    static const SkColor kLinkEnabledColorMd = gfx::kGoogleBlue700;
    // Results tables:
    static const SkColor kResultsTableTextMd = SK_ColorBLACK;
    static const SkColor kResultsTableDimmedTextMd =
        SkColorSetRGB(0x64, 0x64, 0x64);

    switch (color_id) {
      // Dialogs
      case NativeTheme::kColorId_DialogBackground:
      case NativeTheme::kColorId_BubbleBackground:
        return kDialogBackgroundColorMd;

      // Buttons
      case NativeTheme::kColorId_ButtonEnabledColor:
      case NativeTheme::kColorId_ButtonHoverColor:
        return kButtonEnabledColorMd;

      // MenuItem
      case NativeTheme::kColorId_FocusedMenuItemBackgroundColor:
        return kMenuHighlightBackgroundColorMd;
      case NativeTheme::kColorId_SelectedMenuItemForegroundColor:
        return kSelectedMenuItemForegroundColorMd;
      // Link
      case NativeTheme::kColorId_LinkEnabled:
      case NativeTheme::kColorId_LinkPressed:
        // Normal and pressed share a color.
        return kLinkEnabledColorMd;

      // FocusableBorder
      case NativeTheme::kColorId_FocusedBorderColor:
        return gfx::kGoogleBlue500;
      case NativeTheme::kColorId_UnfocusedBorderColor:
        return SkColorSetA(SK_ColorBLACK, 0x66);

      // Results Tables
      case NativeTheme::kColorId_ResultsTableHoveredBackground:
        return SkColorSetA(base_theme->GetSystemColor(
                               NativeTheme::kColorId_ResultsTableNormalText),
                           0x0D);
      case NativeTheme::kColorId_ResultsTableSelectedBackground:
        return SkColorSetA(base_theme->GetSystemColor(
                               NativeTheme::kColorId_ResultsTableNormalText),
                           0x14);
      case NativeTheme::kColorId_ResultsTableNormalText:
      case NativeTheme::kColorId_ResultsTableHoveredText:
      case NativeTheme::kColorId_ResultsTableSelectedText:
        return kResultsTableTextMd;
      case NativeTheme::kColorId_ResultsTableNormalDimmedText:
      case NativeTheme::kColorId_ResultsTableHoveredDimmedText:
      case NativeTheme::kColorId_ResultsTableSelectedDimmedText:
        return kResultsTableDimmedTextMd;
      case NativeTheme::kColorId_ResultsTableNormalUrl:
      case NativeTheme::kColorId_ResultsTableHoveredUrl:
      case NativeTheme::kColorId_ResultsTableSelectedUrl:
        return base_theme->GetSystemColor(NativeTheme::kColorId_LinkEnabled);

      default:
        break;
    }
  }

  // Pre-MD colors.
  // Windows:
  static const SkColor kWindowBackgroundColor = SK_ColorWHITE;
  // Dialogs:
  static const SkColor kDialogBackgroundColor = SkColorSetRGB(251, 251, 251);
  // FocusableBorder:
  static const SkColor kFocusedBorderColor = SkColorSetRGB(0x4D, 0x90, 0xFE);
  static const SkColor kUnfocusedBorderColor = SkColorSetRGB(0xD9, 0xD9, 0xD9);
  // Button:
  static const SkColor kButtonBackgroundColor = SkColorSetRGB(0xDE, 0xDE, 0xDE);
  static const SkColor kButtonEnabledColor = SkColorSetRGB(0x22, 0x22, 0x22);
  static const SkColor kButtonHighlightColor = SkColorSetRGB(0, 0, 0);
  static const SkColor kButtonHoverColor = kButtonEnabledColor;
  static const SkColor kButtonHoverBackgroundColor =
      SkColorSetRGB(0xEA, 0xEA, 0xEA);
  static const SkColor kBlueButtonEnabledColor = SK_ColorWHITE;
  static const SkColor kBlueButtonDisabledColor = SK_ColorWHITE;
  static const SkColor kBlueButtonPressedColor = SK_ColorWHITE;
  static const SkColor kBlueButtonHoverColor = SK_ColorWHITE;
  static const SkColor kBlueButtonShadowColor = SkColorSetRGB(0x53, 0x8C, 0xEA);
  static const SkColor kCallToActionColor = gfx::kGoogleBlue500;
  static const SkColor kTextOnCallToActionColor = SK_ColorWHITE;
  // MenuItem:
  static const SkColor kMenuBackgroundColor = SK_ColorWHITE;
  static const SkColor kMenuHighlightBackgroundColor =
      SkColorSetRGB(0x42, 0x81, 0xF4);
  static const SkColor kMenuBorderColor = SkColorSetRGB(0xBA, 0xBA, 0xBA);
  static const SkColor kEnabledMenuButtonBorderColor =
      SkColorSetARGB(0x24, 0x00, 0x00, 0x00);
  static const SkColor kFocusedMenuButtonBorderColor =
      SkColorSetARGB(0x48, 0x00, 0x00, 0x00);
  static const SkColor kHoverMenuButtonBorderColor =
      SkColorSetARGB(0x48, 0x00, 0x00, 0x00);
  static const SkColor kMenuSeparatorColor = SkColorSetRGB(0xE9, 0xE9, 0xE9);
  static const SkColor kEnabledMenuItemForegroundColor = SK_ColorBLACK;
  static const SkColor kDisabledMenuItemForegroundColor =
      SkColorSetRGB(0xA1, 0xA1, 0x92);
  static const SkColor kHoverMenuItemBackgroundColor =
      SkColorSetARGB(0xCC, 0xFF, 0xFF, 0xFF);
  // Label:
  static const SkColor kLabelEnabledColor = kButtonEnabledColor;
  static const SkColor kLabelBackgroundColor = SK_ColorWHITE;
  // Link:
  static const SkColor kLinkDisabledColor = SK_ColorBLACK;
  static const SkColor kLinkEnabledColor = SkColorSetRGB(0, 51, 153);
  static const SkColor kLinkPressedColor = SK_ColorRED;
  // Textfield:
  static const SkColor kTextfieldDefaultColor = SK_ColorBLACK;
  static const SkColor kTextfieldDefaultBackground = SK_ColorWHITE;
  static const SkColor kTextfieldReadOnlyColor = SK_ColorDKGRAY;
  static const SkColor kTextfieldReadOnlyBackground = SK_ColorWHITE;
  static const SkColor kTextfieldSelectionBackgroundFocused =
      SkColorSetARGB(0x54, 0x60, 0xA8, 0xEB);
  static const SkColor kTextfieldSelectionColor = color_utils::AlphaBlend(
      SK_ColorBLACK, kTextfieldSelectionBackgroundFocused, 0xdd);
  // Tooltip
  static const SkColor kTooltipBackground = 0xFFFFFFCC;
  static const SkColor kTooltipTextColor = kLabelEnabledColor;
  // Tree
  static const SkColor kTreeBackground = SK_ColorWHITE;
  static const SkColor kTreeTextColor = SK_ColorBLACK;
  static const SkColor kTreeSelectedTextColor = SK_ColorBLACK;
  static const SkColor kTreeSelectionBackgroundColor =
      SkColorSetRGB(0xEE, 0xEE, 0xEE);
  static const SkColor kTreeArrowColor = SkColorSetRGB(0x7A, 0x7A, 0x7A);
  // Table
  static const SkColor kTableBackground = SK_ColorWHITE;
  static const SkColor kTableTextColor = SK_ColorBLACK;
  static const SkColor kTableSelectedTextColor = SK_ColorBLACK;
  static const SkColor kTableSelectionBackgroundColor =
      SkColorSetRGB(0xEE, 0xEE, 0xEE);
  static const SkColor kTableGroupingIndicatorColor =
      SkColorSetRGB(0xCC, 0xCC, 0xCC);
  // Results Tables
  static const SkColor kResultsTableSelectedBackground =
      kTextfieldSelectionBackgroundFocused;
  static const SkColor kResultsTableNormalText =
      color_utils::AlphaBlend(SK_ColorBLACK, kTextfieldDefaultBackground, 0xDD);
  static const SkColor kResultsTableHoveredBackground = color_utils::AlphaBlend(
      kTextfieldSelectionBackgroundFocused, kTextfieldDefaultBackground, 0x40);
  static const SkColor kResultsTableHoveredText = color_utils::AlphaBlend(
      SK_ColorBLACK, kResultsTableHoveredBackground, 0xDD);
  static const SkColor kResultsTableSelectedText = color_utils::AlphaBlend(
      SK_ColorBLACK, kTextfieldSelectionBackgroundFocused, 0xDD);
  static const SkColor kResultsTableNormalDimmedText =
      color_utils::AlphaBlend(SK_ColorBLACK, kTextfieldDefaultBackground, 0xBB);
  static const SkColor kResultsTableHoveredDimmedText = color_utils::AlphaBlend(
      SK_ColorBLACK, kResultsTableHoveredBackground, 0xBB);
  static const SkColor kResultsTableSelectedDimmedText =
      color_utils::AlphaBlend(SK_ColorBLACK,
                              kTextfieldSelectionBackgroundFocused, 0xBB);
  static const SkColor kResultsTableNormalUrl = kTextfieldSelectionColor;
  static const SkColor kResultsTableSelectedOrHoveredUrl =
      SkColorSetARGB(0xff, 0x0b, 0x80, 0x43);
  const SkColor kPositiveTextColor = SkColorSetRGB(0x0b, 0x80, 0x43);
  const SkColor kNegativeTextColor = SkColorSetRGB(0xc5, 0x39, 0x29);
  static const SkColor kResultsTablePositiveText = color_utils::AlphaBlend(
      kPositiveTextColor, kTextfieldDefaultBackground, 0xDD);
  static const SkColor kResultsTablePositiveHoveredText =
      color_utils::AlphaBlend(kPositiveTextColor,
                              kResultsTableHoveredBackground, 0xDD);
  static const SkColor kResultsTablePositiveSelectedText =
      color_utils::AlphaBlend(kPositiveTextColor,
                              kTextfieldSelectionBackgroundFocused, 0xDD);
  static const SkColor kResultsTableNegativeText = color_utils::AlphaBlend(
      kNegativeTextColor, kTextfieldDefaultBackground, 0xDD);
  static const SkColor kResultsTableNegativeHoveredText =
      color_utils::AlphaBlend(kNegativeTextColor,
                              kResultsTableHoveredBackground, 0xDD);
  static const SkColor kResultsTableNegativeSelectedText =
      color_utils::AlphaBlend(kNegativeTextColor,
                              kTextfieldSelectionBackgroundFocused, 0xDD);
  // Material spinner/throbber:
  static const SkColor kThrobberSpinningColor = gfx::kGoogleBlue500;
  static const SkColor kThrobberWaitingColor = SkColorSetRGB(0xA6, 0xA6, 0xA6);
  static const SkColor kThrobberLightColor = SkColorSetRGB(0xF4, 0xF8, 0xFD);

  switch (color_id) {
    // Windows
    case NativeTheme::kColorId_WindowBackground:
      return kWindowBackgroundColor;

    // Dialogs
    case NativeTheme::kColorId_DialogBackground:
    case NativeTheme::kColorId_BubbleBackground:
      return kDialogBackgroundColor;

    // FocusableBorder
    case NativeTheme::kColorId_FocusedBorderColor:
      return kFocusedBorderColor;
    case NativeTheme::kColorId_UnfocusedBorderColor:
      return kUnfocusedBorderColor;

    // Button
    case NativeTheme::kColorId_ButtonBackgroundColor:
      return kButtonBackgroundColor;
    case NativeTheme::kColorId_ButtonEnabledColor:
      return kButtonEnabledColor;
    case NativeTheme::kColorId_ButtonHighlightColor:
      return kButtonHighlightColor;
    case NativeTheme::kColorId_ButtonHoverColor:
      return kButtonHoverColor;
    case NativeTheme::kColorId_ButtonHoverBackgroundColor:
      return kButtonHoverBackgroundColor;
    case NativeTheme::kColorId_BlueButtonEnabledColor:
      return kBlueButtonEnabledColor;
    case NativeTheme::kColorId_BlueButtonDisabledColor:
      return kBlueButtonDisabledColor;
    case NativeTheme::kColorId_BlueButtonPressedColor:
      return kBlueButtonPressedColor;
    case NativeTheme::kColorId_BlueButtonHoverColor:
      return kBlueButtonHoverColor;
    case NativeTheme::kColorId_BlueButtonShadowColor:
      return kBlueButtonShadowColor;
    case NativeTheme::kColorId_CallToActionColor:
      return kCallToActionColor;
    case NativeTheme::kColorId_TextOnCallToActionColor:
      return kTextOnCallToActionColor;

    // MenuItem
    case NativeTheme::kColorId_MenuBorderColor:
      return kMenuBorderColor;
    case NativeTheme::kColorId_EnabledMenuButtonBorderColor:
      return kEnabledMenuButtonBorderColor;
    case NativeTheme::kColorId_FocusedMenuButtonBorderColor:
      return kFocusedMenuButtonBorderColor;
    case NativeTheme::kColorId_HoverMenuButtonBorderColor:
      return kHoverMenuButtonBorderColor;
    case NativeTheme::kColorId_MenuSeparatorColor:
      return kMenuSeparatorColor;
    case NativeTheme::kColorId_MenuBackgroundColor:
      return kMenuBackgroundColor;
    case NativeTheme::kColorId_FocusedMenuItemBackgroundColor:
      return kMenuHighlightBackgroundColor;
    case NativeTheme::kColorId_HoverMenuItemBackgroundColor:
      return kHoverMenuItemBackgroundColor;
    case NativeTheme::kColorId_EnabledMenuItemForegroundColor:
      return kEnabledMenuItemForegroundColor;
    case NativeTheme::kColorId_DisabledMenuItemForegroundColor:
      return kDisabledMenuItemForegroundColor;
    case NativeTheme::kColorId_DisabledEmphasizedMenuItemForegroundColor:
      return SK_ColorBLACK;
    case NativeTheme::kColorId_SelectedMenuItemForegroundColor:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_ButtonDisabledColor:
      return kDisabledMenuItemForegroundColor;

    // Label
    case NativeTheme::kColorId_LabelEnabledColor:
      return kLabelEnabledColor;
    case NativeTheme::kColorId_LabelDisabledColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_ButtonDisabledColor);
    case NativeTheme::kColorId_LabelBackgroundColor:
      return kLabelBackgroundColor;

    // Link
    case NativeTheme::kColorId_LinkDisabled:
      return kLinkDisabledColor;
    case NativeTheme::kColorId_LinkEnabled:
      return kLinkEnabledColor;
    case NativeTheme::kColorId_LinkPressed:
      return kLinkPressedColor;

    // Textfield
    case NativeTheme::kColorId_TextfieldDefaultColor:
      return kTextfieldDefaultColor;
    case NativeTheme::kColorId_TextfieldDefaultBackground:
      return kTextfieldDefaultBackground;
    case NativeTheme::kColorId_TextfieldReadOnlyColor:
      return kTextfieldReadOnlyColor;
    case NativeTheme::kColorId_TextfieldReadOnlyBackground:
      return kTextfieldReadOnlyBackground;
    case NativeTheme::kColorId_TextfieldSelectionColor:
      return kTextfieldSelectionColor;
    case NativeTheme::kColorId_TextfieldSelectionBackgroundFocused:
      return kTextfieldSelectionBackgroundFocused;

    // Tooltip
    case NativeTheme::kColorId_TooltipBackground:
      return kTooltipBackground;
    case NativeTheme::kColorId_TooltipText:
      return kTooltipTextColor;

    // Tree
    case NativeTheme::kColorId_TreeBackground:
      return kTreeBackground;
    case NativeTheme::kColorId_TreeText:
      return kTreeTextColor;
    case NativeTheme::kColorId_TreeSelectedText:
    case NativeTheme::kColorId_TreeSelectedTextUnfocused:
      return kTreeSelectedTextColor;
    case NativeTheme::kColorId_TreeSelectionBackgroundFocused:
    case NativeTheme::kColorId_TreeSelectionBackgroundUnfocused:
      return kTreeSelectionBackgroundColor;
    case NativeTheme::kColorId_TreeArrow:
      return kTreeArrowColor;

    // Table
    case NativeTheme::kColorId_TableBackground:
      return kTableBackground;
    case NativeTheme::kColorId_TableText:
      return kTableTextColor;
    case NativeTheme::kColorId_TableSelectedText:
    case NativeTheme::kColorId_TableSelectedTextUnfocused:
      return kTableSelectedTextColor;
    case NativeTheme::kColorId_TableSelectionBackgroundFocused:
    case NativeTheme::kColorId_TableSelectionBackgroundUnfocused:
      return kTableSelectionBackgroundColor;
    case NativeTheme::kColorId_TableGroupingIndicatorColor:
      return kTableGroupingIndicatorColor;

    // Results Tables
    case NativeTheme::kColorId_ResultsTableNormalBackground:
      return kTextfieldDefaultBackground;
    case NativeTheme::kColorId_ResultsTableHoveredBackground:
      return kResultsTableHoveredBackground;
    case NativeTheme::kColorId_ResultsTableSelectedBackground:
      return kResultsTableSelectedBackground;
    case NativeTheme::kColorId_ResultsTableNormalText:
      return kResultsTableNormalText;
    case NativeTheme::kColorId_ResultsTableHoveredText:
      return kResultsTableHoveredText;
    case NativeTheme::kColorId_ResultsTableSelectedText:
      return kResultsTableSelectedText;
    case NativeTheme::kColorId_ResultsTableNormalDimmedText:
      return kResultsTableNormalDimmedText;
    case NativeTheme::kColorId_ResultsTableHoveredDimmedText:
      return kResultsTableHoveredDimmedText;
    case NativeTheme::kColorId_ResultsTableSelectedDimmedText:
      return kResultsTableSelectedDimmedText;
    case NativeTheme::kColorId_ResultsTableNormalUrl:
      return kResultsTableNormalUrl;
    case NativeTheme::kColorId_ResultsTableHoveredUrl:
    case NativeTheme::kColorId_ResultsTableSelectedUrl:
      return kResultsTableSelectedOrHoveredUrl;
    case NativeTheme::kColorId_ResultsTablePositiveText:
      return kResultsTablePositiveText;
    case NativeTheme::kColorId_ResultsTablePositiveHoveredText:
      return kResultsTablePositiveHoveredText;
    case NativeTheme::kColorId_ResultsTablePositiveSelectedText:
      return kResultsTablePositiveSelectedText;
    case NativeTheme::kColorId_ResultsTableNegativeText:
      return kResultsTableNegativeText;
    case NativeTheme::kColorId_ResultsTableNegativeHoveredText:
      return kResultsTableNegativeHoveredText;
    case NativeTheme::kColorId_ResultsTableNegativeSelectedText:
      return kResultsTableNegativeSelectedText;

    // Material spinner/throbber
    case NativeTheme::kColorId_ThrobberSpinningColor:
      return kThrobberSpinningColor;
    case NativeTheme::kColorId_ThrobberWaitingColor:
      return kThrobberWaitingColor;
    case NativeTheme::kColorId_ThrobberLightColor:
      return kThrobberLightColor;

    case NativeTheme::kColorId_NumColors:
      break;
  }

  NOTREACHED();
  return gfx::kPlaceholderColor;
}

void CommonThemePaintMenuItemBackground(
    const NativeTheme* theme,
    SkCanvas* canvas,
    NativeTheme::State state,
    const gfx::Rect& rect,
    const NativeTheme::MenuItemExtraParams& menu_item) {
  SkPaint paint;
  switch (state) {
    case NativeTheme::kNormal:
    case NativeTheme::kDisabled:
      paint.setColor(
          theme->GetSystemColor(NativeTheme::kColorId_MenuBackgroundColor));
      break;
    case NativeTheme::kHovered:
      paint.setColor(theme->GetSystemColor(
          NativeTheme::kColorId_FocusedMenuItemBackgroundColor));
      break;
    default:
      NOTREACHED() << "Invalid state " << state;
      break;
  }
  if (menu_item.corner_radius > 0) {
    const SkScalar radius = SkIntToScalar(menu_item.corner_radius);
    canvas->drawRoundRect(gfx::RectToSkRect(rect), radius, radius, paint);
    return;
  }
  canvas->drawRect(gfx::RectToSkRect(rect), paint);
}

}  // namespace ui
