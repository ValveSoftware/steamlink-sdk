// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SEPARATOR_H_
#define UI_VIEWS_CONTROLS_SEPARATOR_H_

#include <string>

#include "base/macros.h"
#include "ui/views/view.h"

namespace views {

// The Separator class is a view that shows a line used to visually separate
// other views.

class VIEWS_EXPORT Separator : public View {
 public:
  enum Orientation {
    HORIZONTAL,
    VERTICAL
  };

  // The separator's class name.
  static const char kViewClassName[];

  explicit Separator(Orientation orientation);
  ~Separator() override;

  SkColor color() const { return color_; }
  void SetColor(SkColor color);

  int size() const { return size_; }
  // Preferred size of one axis: height for horizontal separator
  // and width for vertical separator
  void SetPreferredSize(int size);

  // Overridden from View:
  gfx::Size GetPreferredSize() const override;
  void GetAccessibleState(ui::AXViewState* state) override;
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

 private:
  const Orientation orientation_;
  SkColor color_;
  int size_;

  DISALLOW_COPY_AND_ASSIGN(Separator);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SEPARATOR_H_
