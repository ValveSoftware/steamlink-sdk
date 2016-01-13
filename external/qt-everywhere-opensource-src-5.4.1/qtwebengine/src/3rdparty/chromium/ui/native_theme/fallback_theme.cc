// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/fallback_theme.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/common_theme.h"

namespace ui {

FallbackTheme::FallbackTheme() {
}

FallbackTheme::~FallbackTheme() {
}

SkColor FallbackTheme::GetSystemColor(ColorId color_id) const {
  // This implementation returns hardcoded colors.

  static const SkColor kInvalidColorIdColor = SkColorSetRGB(255, 0, 128);
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
  // Label:
  static const SkColor kLabelEnabledColor = kButtonEnabledColor;
  static const SkColor kLabelBackgroundColor = SK_ColorWHITE;
  // Textfield:
  static const SkColor kTextfieldDefaultColor = SK_ColorBLACK;
  static const SkColor kTextfieldDefaultBackground = SK_ColorWHITE;
  static const SkColor kTextfieldReadOnlyColor = SK_ColorDKGRAY;
  static const SkColor kTextfieldReadOnlyBackground = SK_ColorWHITE;
  static const SkColor kTextfieldSelectionBackgroundFocused =
      SkColorSetARGB(0x54, 0x60, 0xA8, 0xEB);
  static const SkColor kTextfieldSelectionColor =
      color_utils::AlphaBlend(SK_ColorBLACK,
          kTextfieldSelectionBackgroundFocused, 0xdd);
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
  static const SkColor kResultsTableHoveredBackground =
      color_utils::AlphaBlend(kTextfieldSelectionBackgroundFocused,
                              kTextfieldDefaultBackground, 0x40);
  static const SkColor kResultsTableNormalText = color_utils::AlphaBlend(
      SK_ColorBLACK, kTextfieldDefaultBackground, 0xDD);
  static const SkColor kResultsTableHoveredText = color_utils::AlphaBlend(
      SK_ColorBLACK, kResultsTableHoveredBackground, 0xDD);
  static const SkColor kResultsTableSelectedText = color_utils::AlphaBlend(
      SK_ColorBLACK, kTextfieldSelectionBackgroundFocused, 0xDD);
  static const SkColor kResultsTableNormalDimmedText = color_utils::AlphaBlend(
      SK_ColorBLACK, kTextfieldDefaultBackground, 0xBB);
  static const SkColor kResultsTableHoveredDimmedText = color_utils::AlphaBlend(
      SK_ColorBLACK, kResultsTableHoveredBackground, 0xBB);
  static const SkColor kResultsTableSelectedDimmedText =
      color_utils::AlphaBlend(
          SK_ColorBLACK, kTextfieldSelectionBackgroundFocused, 0xBB);
  static const SkColor kResultsTableSelectedOrHoveredUrl =
      SkColorSetARGB(0xff, 0x00, 0x66, 0x22);
  static const SkColor kResultsTableNormalDivider = color_utils::AlphaBlend(
      kResultsTableNormalText, kTextfieldDefaultBackground, 0x34);
  static const SkColor kResultsTableHoveredDivider = color_utils::AlphaBlend(
      kResultsTableHoveredText, kResultsTableHoveredBackground, 0x34);
  static const SkColor kResultsTabSelectedDivider = color_utils::AlphaBlend(
      kResultsTableSelectedText, kTextfieldSelectionBackgroundFocused, 0x34);

  SkColor color;
  if (CommonThemeGetSystemColor(color_id, &color))
    return color;

  switch (color_id) {
    // Windows
    case kColorId_WindowBackground:
      return kWindowBackgroundColor;

    // Dialogs
    case kColorId_DialogBackground:
      return kDialogBackgroundColor;

    // FocusableBorder
    case kColorId_FocusedBorderColor:
      return kFocusedBorderColor;
    case kColorId_UnfocusedBorderColor:
      return kUnfocusedBorderColor;

    // Button
    case kColorId_ButtonBackgroundColor:
      return kButtonBackgroundColor;
    case kColorId_ButtonEnabledColor:
      return kButtonEnabledColor;
    case kColorId_ButtonHighlightColor:
      return kButtonHighlightColor;
    case kColorId_ButtonHoverColor:
      return kButtonHoverColor;

    // Label
    case kColorId_LabelEnabledColor:
      return kLabelEnabledColor;
    case kColorId_LabelDisabledColor:
      return GetSystemColor(kColorId_ButtonDisabledColor);
    case kColorId_LabelBackgroundColor:
      return kLabelBackgroundColor;

    // Textfield
    case kColorId_TextfieldDefaultColor:
      return kTextfieldDefaultColor;
    case kColorId_TextfieldDefaultBackground:
      return kTextfieldDefaultBackground;
    case kColorId_TextfieldReadOnlyColor:
      return kTextfieldReadOnlyColor;
    case kColorId_TextfieldReadOnlyBackground:
      return kTextfieldReadOnlyBackground;
    case kColorId_TextfieldSelectionColor:
      return kTextfieldSelectionColor;
    case kColorId_TextfieldSelectionBackgroundFocused:
      return kTextfieldSelectionBackgroundFocused;

    // Tooltip
    case kColorId_TooltipBackground:
      return kTooltipBackground;
    case kColorId_TooltipText:
      return kTooltipTextColor;

    // Tree
    case kColorId_TreeBackground:
      return kTreeBackground;
    case kColorId_TreeText:
      return kTreeTextColor;
    case kColorId_TreeSelectedText:
    case kColorId_TreeSelectedTextUnfocused:
      return kTreeSelectedTextColor;
    case kColorId_TreeSelectionBackgroundFocused:
    case kColorId_TreeSelectionBackgroundUnfocused:
      return kTreeSelectionBackgroundColor;
    case kColorId_TreeArrow:
      return kTreeArrowColor;

    // Table
    case kColorId_TableBackground:
      return kTableBackground;
    case kColorId_TableText:
      return kTableTextColor;
    case kColorId_TableSelectedText:
    case kColorId_TableSelectedTextUnfocused:
      return kTableSelectedTextColor;
    case kColorId_TableSelectionBackgroundFocused:
    case kColorId_TableSelectionBackgroundUnfocused:
      return kTableSelectionBackgroundColor;
    case kColorId_TableGroupingIndicatorColor:
      return kTableGroupingIndicatorColor;

    // Results Tables
    case kColorId_ResultsTableNormalBackground:
      return kTextfieldDefaultBackground;
    case kColorId_ResultsTableHoveredBackground:
      return kResultsTableHoveredBackground;
    case kColorId_ResultsTableSelectedBackground:
      return kTextfieldSelectionBackgroundFocused;
    case kColorId_ResultsTableNormalText:
      return kResultsTableNormalText;
    case kColorId_ResultsTableHoveredText:
      return kResultsTableHoveredText;
    case kColorId_ResultsTableSelectedText:
      return kResultsTableSelectedText;
    case kColorId_ResultsTableNormalDimmedText:
      return kResultsTableNormalDimmedText;
    case kColorId_ResultsTableHoveredDimmedText:
      return kResultsTableHoveredDimmedText;
    case kColorId_ResultsTableSelectedDimmedText:
      return kResultsTableSelectedDimmedText;
    case kColorId_ResultsTableNormalUrl:
      return kTextfieldSelectionColor;
    case kColorId_ResultsTableHoveredUrl:
    case kColorId_ResultsTableSelectedUrl:
      return kResultsTableSelectedOrHoveredUrl;
    case kColorId_ResultsTableNormalDivider:
      return kResultsTableNormalDivider;
    case kColorId_ResultsTableHoveredDivider:
      return kResultsTableHoveredDivider;
    case kColorId_ResultsTableSelectedDivider:
      return kResultsTabSelectedDivider;

    default:
      NOTREACHED();
      break;
  }

  return kInvalidColorIdColor;
}

}  // namespace ui
