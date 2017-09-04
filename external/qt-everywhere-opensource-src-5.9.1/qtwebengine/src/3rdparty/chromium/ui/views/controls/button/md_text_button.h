// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_

#include <memory>

#include "base/optional.h"
#include "ui/views/controls/button/label_button.h"

namespace views {

// A button class that implements the Material Design text button spec.
class VIEWS_EXPORT MdTextButton : public LabelButton {
 public:
  // As above, but only creates an MdTextButton if MD is enabled in the
  // secondary UI (as opposed to just "top chrome"/"primary" UI).
  static LabelButton* CreateSecondaryUiButton(ButtonListener* listener,
                                              const base::string16& text);
  static LabelButton* CreateSecondaryUiBlueButton(ButtonListener* listener,
                                                  const base::string16& text);
  static MdTextButton* Create(ButtonListener* listener,
                              const base::string16& text);

  ~MdTextButton() override;

  // See |is_prominent_|.
  void SetProminent(bool is_prominent);

  // See |bg_color_override_|.
  void SetBgColorOverride(const base::Optional<SkColor>& color);

  // View:
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // LabelButton:
  void OnFocus() override;
  void OnBlur() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;
  std::unique_ptr<InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;
  void SetEnabledTextColors(SkColor color) override;
  void SetText(const base::string16& text) override;
  void AdjustFontSize(int size_delta) override;
  void UpdateStyleToIndicateDefaultStatus() override;
  void StateChanged() override;

 protected:
  // LabelButton:
  void SetFontList(const gfx::FontList& font_list) override;

 private:
  explicit MdTextButton(ButtonListener* listener);

  void UpdatePadding();
  void UpdateColors();

  // True if this button uses prominent styling (blue fill, etc.).
  bool is_prominent_;

  // When set, this provides the background color.
  base::Optional<SkColor> bg_color_override_;

  DISALLOW_COPY_AND_ASSIGN(MdTextButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
