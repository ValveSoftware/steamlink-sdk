// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
#define UI_NATIVE_THEME_NATIVE_THEME_MAC_H_

#include "base/macros.h"
#include "ui/native_theme/native_theme_base.h"
#include "ui/native_theme/native_theme_export.h"

namespace ui {

// Mac implementation of native theme support.
class NATIVE_THEME_EXPORT NativeThemeMac : public NativeThemeBase {
 public:
  static const int kButtonCornerRadius = 3;

  // Type of gradient to use on a button background. Use HIGHLIGHTED for the
  // default button of a window and all combobox controls, but only when the
  // window is active.
  enum class ButtonBackgroundType {
    DISABLED,
    HIGHLIGHTED,
    NORMAL,
    PRESSED,
    COUNT
  };

  static NativeThemeMac* instance();

  // Overridden from NativeTheme:
  SkColor GetSystemColor(ColorId color_id) const override;

  // Overridden from NativeThemeBase:
  void PaintMenuPopupBackground(
      SkCanvas* canvas,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background) const override;
  void PaintMenuItemBackground(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuItemExtraParams& menu_item) const override;

  // Creates a shader appropriate for painting the background of a button.
  static sk_sp<SkShader> GetButtonBackgroundShader(ButtonBackgroundType type,
                                                   int height);

  // Creates a shader for the button border. This should be painted over with
  // the background after insetting the rounded rect.
  static sk_sp<SkShader> GetButtonBorderShader(ButtonBackgroundType type,
                                               int height);

  // Paints the styled button shape used for default controls on Mac. The basic
  // style is used for dialog buttons, comboboxes, and tabbed pane tabs.
  // Depending on the control part being drawn, the left or the right side can
  // be given rounded corners.
  static void PaintStyledGradientButton(SkCanvas* canvas,
                                        const gfx::Rect& bounds,
                                        ButtonBackgroundType type,
                                        bool round_left,
                                        bool round_right,
                                        bool focus);

 private:
  NativeThemeMac();
  ~NativeThemeMac() override;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeMac);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
