// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_STYLE_PLATFORM_STYLE_H_
#define UI_VIEWS_STYLE_PLATFORM_STYLE_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/views_export.h"

namespace views {

class Border;
class FocusableBorder;
class Label;
class LabelButton;
class LabelButtonBorder;
class ScrollBar;

// Cross-platform API for providing platform-specific styling for toolkit-views.
class VIEWS_EXPORT PlatformStyle {
 public:
  // Type used by LabelButton to map button states to text colors.
  using ButtonColorByState = SkColor[Button::STATE_COUNT];

  // Padding to use on either side of the arrow for a Combobox when in
  // Combobox::STYLE_NORMAL.
  static const int kComboboxNormalArrowPadding;

  // Minimum size for platform-styled buttons (Button::STYLE_BUTTON).
  static const int kMinLabelButtonWidth;
  static const int kMinLabelButtonHeight;

  // Whether dialog-default buttons are given a bold font style.
  static const bool kDefaultLabelButtonHasBoldFont;

  // Whether dragging vertically above or below a textfield's bounds selects to
  // the left or right end of the text from the cursor, respectively.
  static const bool kTextfieldDragVerticallyDragsToEnd;

  // Creates an ImageSkia containing the image to use for the combobox arrow.
  // The |is_enabled| argument is true if the control the arrow is for is
  // enabled, and false if the control is disabled. The |style| argument is the
  // style of the combobox the arrow is being drawn for.
  static gfx::ImageSkia CreateComboboxArrow(bool is_enabled,
                                            Combobox::Style style);

  // Creates the appropriate border for a focusable Combobox.
  static std::unique_ptr<FocusableBorder> CreateComboboxBorder();

  // Creates the appropriate background for a Combobox.
  static std::unique_ptr<Background> CreateComboboxBackground(
      int shoulder_width);

  // Creates the default label button border for the given |style|. Used when a
  // custom default border is not provided for a particular LabelButton class.
  static std::unique_ptr<LabelButtonBorder> CreateLabelButtonBorder(
      Button::ButtonStyle style);

  // Creates the default scrollbar for the given orientation.
  static std::unique_ptr<ScrollBar> CreateScrollBar(bool is_horizontal);

  // Returns the current text color for the current button state.
  static SkColor TextColorForButton(const ButtonColorByState& color_by_state,
                                    const LabelButton& button);

  // Applies platform styles to |label| and fills |color_by_state| with the text
  // colors for normal, pressed, hovered, and disabled states, if the colors for
  // Button::STYLE_BUTTON buttons differ from those provided by ui::NativeTheme.
  static void ApplyLabelButtonTextStyle(Label* label,
                                        ButtonColorByState* color_by_state);

  // Applies the current system theme to the default border created by |button|.
  static std::unique_ptr<Border> CreateThemedLabelButtonBorder(
      LabelButton* button);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PlatformStyle);
};

}  // namespace views

#endif  // UI_VIEWS_STYLE_PLATFORM_STYLE_H_
