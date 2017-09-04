// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_FOCUS_RING_H_
#define UI_VIEWS_CONTROLS_FOCUS_RING_H_

#include "base/optional.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/view.h"

namespace views {

// FocusRing is a View that is designed to act as an indicator of focus for its
// parent. It is a stand-alone view that paints to a layer which extends beyond
// the bounds of its parent view.
class FocusRing : public View {
 public:
  static const char kViewClassName[];

  // Create a FocusRing and adds it to |parent|, or updates the one that already
  // exists. |override_color_id| will be used in place of the default coloration
  // when provided.
  static View* Install(views::View* parent,
                      ui::NativeTheme::ColorId override_color_id =
                          ui::NativeTheme::kColorId_NumColors);

  // Removes the FocusRing from |parent|.
  static void Uninstall(views::View* parent);

  // View:
  const char* GetClassName() const override;
  bool CanProcessEventsWithinSubtree() const override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  FocusRing();
  ~FocusRing() override;

  ui::NativeTheme::ColorId override_color_id_;

  DISALLOW_COPY_AND_ASSIGN(FocusRing);
};

}  // views

#endif  // UI_VIEWS_CONTROLS_FOCUS_RING_H_
