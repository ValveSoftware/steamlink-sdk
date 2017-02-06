// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
#define UI_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/label_button.h"

class SkPaint;

namespace views {

// A native themed class representing a checkbox.  This class does not use
// platform specific objects to replicate the native platforms looks and feel.
class VIEWS_EXPORT Checkbox : public LabelButton {
 public:
  static const char kViewClassName[];

  explicit Checkbox(const base::string16& label);
  ~Checkbox() override;

  // Sets a listener for this checkbox. Checkboxes aren't required to have them
  // since their state can be read independently of them being toggled.
  void set_listener(ButtonListener* listener) { listener_ = listener; }

  // Sets/Gets whether or not the checkbox is checked.
  virtual void SetChecked(bool checked);
  bool checked() const { return checked_; }

 protected:
  // Returns whether MD is enabled; exists for the sake of brevity.
  static bool UseMd();

  // Overridden from LabelButton:
  void Layout() override;
  const char* GetClassName() const override;
  void GetAccessibleState(ui::AXViewState* state) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;
  gfx::ImageSkia GetImage(ButtonState for_state) const override;

  // Set the image shown for each button state depending on whether it is
  // [checked] or [focused].
  void SetCustomImage(bool checked,
                      bool focused,
                      ButtonState for_state,
                      const gfx::ImageSkia& image);

  // Paints a focus indicator for the view.
  virtual void PaintFocusRing(gfx::Canvas* canvas, const SkPaint& paint);

 private:
  // Overridden from Button:
  void NotifyClick(const ui::Event& event) override;

  ui::NativeTheme::Part GetThemePart() const override;
  void GetExtraParams(ui::NativeTheme::ExtraParams* params) const override;

  // True if the checkbox is checked.
  bool checked_;

  // The images for each button state.
  gfx::ImageSkia images_[2][2][STATE_COUNT];

  DISALLOW_COPY_AND_ASSIGN(Checkbox);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
