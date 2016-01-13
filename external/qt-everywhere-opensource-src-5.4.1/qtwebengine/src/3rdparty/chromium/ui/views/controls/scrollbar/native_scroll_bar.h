// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLL_BAR_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLL_BAR_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/view.h"

namespace ui {
class NativeTheme;
}

namespace views {

class NativeScrollBarWrapper;

// The NativeScrollBar class is a scrollbar that uses platform's
// native control.
class VIEWS_EXPORT NativeScrollBar : public ScrollBar {
 public:
  // The scroll-bar's class name.
  static const char kViewClassName[];

  // Create new scrollbar, either horizontal or vertical.
  explicit NativeScrollBar(bool is_horiz);
  virtual ~NativeScrollBar();

  // Return the system sizes.
  static int GetHorizontalScrollBarHeight(const ui::NativeTheme* theme);
  static int GetVerticalScrollBarWidth(const ui::NativeTheme* theme);

 private:
  friend class NativeScrollBarTest;
  FRIEND_TEST_ALL_PREFIXES(NativeScrollBarTest, Scrolling);

  // Overridden from View.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

  // Overrideen from View for keyboard UI purpose.
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& e) OVERRIDE;

  // Overridden from ui::EventHandler.
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Overridden from ScrollBar.
  virtual void Update(int viewport_size,
                      int content_size,
                      int current_pos) OVERRIDE;
  virtual int GetPosition() const OVERRIDE;
  virtual int GetLayoutSize() const OVERRIDE;

  // init border
  NativeScrollBarWrapper* native_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(NativeScrollBar);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLL_BAR_H_
