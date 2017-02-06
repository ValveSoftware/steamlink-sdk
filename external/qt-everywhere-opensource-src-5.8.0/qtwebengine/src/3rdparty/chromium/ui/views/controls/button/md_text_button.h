// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_

#include <memory>

#include "ui/views/controls/button/label_button.h"

namespace views {

namespace internal {
class MdFocusRing;
}  // namespace internal

// A button class that implements the Material Design text button spec.
class VIEWS_EXPORT MdTextButton : public LabelButton {
 public:
  // Creates a normal STYLE_BUTTON LabelButton in pre-MD, or an MdTextButton
  // in MD mode.
  static LabelButton* CreateStandardButton(ButtonListener* listener,
                                           const base::string16& text);
  // As above, but only creates an MdTextButton if MD is enabled in the
  // secondary UI (as opposed to just "top chrome"/"primary" UI).
  static LabelButton* CreateSecondaryUiButton(ButtonListener* listener,
                                              const base::string16& text);
  static LabelButton* CreateSecondaryUiBlueButton(ButtonListener* listener,
                                                  const base::string16& text);
  static MdTextButton* CreateMdButton(ButtonListener* listener,
                                      const base::string16& text);

  // Paint an MD-style focus ring on the given canvas at the given bounds.
  static void PaintMdFocusRing(gfx::Canvas* canvas,
                               View* view,
                               int thickness,
                               SkAlpha alpha);

  void SetCallToAction(bool cta);

  // LabelButton:
  void Layout() override;
  void OnFocus() override;
  void OnBlur() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;
  bool ShouldShowInkDropForFocus() const override;
  void SetEnabledTextColors(SkColor color) override;
  void UpdateStyleToIndicateDefaultStatus() override;

 private:
  MdTextButton(ButtonListener* listener);
  ~MdTextButton() override;

  void UpdateColors();

  // The MD-style focus ring. This is not done via a FocusPainter
  // because it needs to paint to a layer so it can extend beyond the bounds of
  // |this|.
  internal::MdFocusRing* focus_ring_;

  // True if this button uses call-to-action styling.
  bool is_cta_;

  DISALLOW_COPY_AND_ASSIGN(MdTextButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
