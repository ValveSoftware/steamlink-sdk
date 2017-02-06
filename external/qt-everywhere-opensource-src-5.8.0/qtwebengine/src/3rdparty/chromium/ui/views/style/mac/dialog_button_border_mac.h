// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_STYLE_MAC_DIALOG_BUTTON_BORDER_MAC_H_
#define UI_VIEWS_STYLE_MAC_DIALOG_BUTTON_BORDER_MAC_H_

#include "base/macros.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/views_export.h"

namespace views {

class LabelButton;

// Skia port of the default button style used for dialogs on Chrome Mac.
// Originally provided by ConstrainedWindowButton, which used Quartz-backed
// Cocoa drawing routines.
class VIEWS_EXPORT DialogButtonBorderMac : public LabelButtonBorder {
 public:
  DialogButtonBorderMac();
  ~DialogButtonBorderMac() override;

  // Whether the given |button| should get a highlighted background.
  static bool ShouldRenderDefault(const LabelButton& button);

  // views::Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Size GetMinimumSize() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DialogButtonBorderMac);
};

}  // namespace views

#endif  // UI_VIEWS_STYLE_MAC_DIALOG_BUTTON_BORDER_MAC_H_
