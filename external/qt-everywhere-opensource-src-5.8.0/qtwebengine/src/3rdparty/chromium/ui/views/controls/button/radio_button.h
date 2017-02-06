// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/checkbox.h"

namespace views {

// A native themed class representing a radio button.  This class does not use
// platform specific objects to replicate the native platforms looks and feel.
class VIEWS_EXPORT RadioButton : public Checkbox {
 public:
  // The button's class name.
  static const char kViewClassName[];

  RadioButton(const base::string16& label, int group_id);
  ~RadioButton() override;

  // Overridden from View:
  const char* GetClassName() const override;
  void GetAccessibleState(ui::AXViewState* state) override;
  View* GetSelectedViewForGroup(int group) override;
  bool IsGroupFocusTraversable() const override;
  void OnFocus() override;

  // Overridden from Button:
  void NotifyClick(const ui::Event& event) override;

  // Overridden from LabelButton:
  ui::NativeTheme::Part GetThemePart() const override;
  gfx::ImageSkia GetImage(ButtonState for_state) const override;

  // Overridden from Checkbox:
  void SetChecked(bool checked) override;
  void PaintFocusRing(gfx::Canvas* canvas, const SkPaint& paint) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(RadioButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
