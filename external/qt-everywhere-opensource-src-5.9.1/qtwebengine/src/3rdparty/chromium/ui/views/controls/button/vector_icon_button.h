// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_VECTOR_ICON_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_VECTOR_ICON_BUTTON_H_

#include "base/macros.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
enum class VectorIconId;
}

namespace views {

class VectorIconButtonDelegate;

// A button that has an image and no text, with the image defined by a vector
// icon identifier.
class VIEWS_EXPORT VectorIconButton : public views::ImageButton {
 public:
  explicit VectorIconButton(VectorIconButtonDelegate* delegate);
  ~VectorIconButton() override;

  // Sets the icon to display and provides a callback which should return the
  // text color from which to derive this icon's color.
  void SetIcon(gfx::VectorIconId id);

  // views::ImageButton:
  void OnThemeChanged() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

 private:
  VectorIconButtonDelegate* delegate_;
  gfx::VectorIconId id_;

  DISALLOW_COPY_AND_ASSIGN(VectorIconButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_VECTOR_ICON_BUTTON_H_
