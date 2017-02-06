// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_FOCUSABLE_BORDER_H_
#define UI_VIEWS_CONTROLS_FOCUSABLE_BORDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/border.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
class Insets;
}

namespace views {

// A Border class to draw a focused border around a field (e.g textfield).
class VIEWS_EXPORT FocusableBorder : public Border {
 public:
  FocusableBorder();
  ~FocusableBorder() override;

  // Sets the insets of the border.
  void SetInsets(int top, int left, int bottom, int right);

  // Sets the color of this border.
  void SetColor(SkColor color);
  // Reverts the color of this border to the system default.
  void UseDefaultColor();

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 protected:
  SkColor GetCurrentColor(const View& view) const;

 private:
  gfx::Insets insets_;

  // The color to paint the border when |use_default_color_| is false.
  SkColor override_color_;

  // Whether the system border color should be used. True unless SetColor has
  // been called.
  bool use_default_color_;

  DISALLOW_COPY_AND_ASSIGN(FocusableBorder);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_FOCUSABLE_BORDER_H_
