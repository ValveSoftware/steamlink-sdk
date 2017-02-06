// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_FOCUSABLE_ROUNDED_BORDER_H_
#define UI_VIEWS_CONTROLS_FOCUSABLE_ROUNDED_BORDER_H_

#include "base/macros.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/view.h"

namespace views {

// A Border that changes colors in accordance with the system color scheme when
// the View it borders is focused.
class FocusableRoundedBorder : public FocusableBorder {
 public:
  FocusableRoundedBorder();
  ~FocusableRoundedBorder() override;

  // Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusableRoundedBorder);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_FOCUSABLE_ROUNDED_BORDER_H_
