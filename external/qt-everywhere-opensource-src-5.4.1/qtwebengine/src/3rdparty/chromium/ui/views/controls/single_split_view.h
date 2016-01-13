// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_H_
#define UI_VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_H_

#include "base/gtest_prod_util.h"
#include "ui/views/view.h"

namespace views {

class SingleSplitViewListener;

// SingleSplitView lays out two views next to each other, either horizontally
// or vertically. A splitter exists between the two views that the user can
// drag around to resize the views.
// SingleSplitViewListener's SplitHandleMoved notification helps to monitor user
// initiated layout changes.
class VIEWS_EXPORT SingleSplitView : public View {
 public:
  enum Orientation {
    HORIZONTAL_SPLIT,
    VERTICAL_SPLIT
  };

  static const char kViewClassName[];

  SingleSplitView(View* leading,
                  View* trailing,
                  Orientation orientation,
                  SingleSplitViewListener* listener);

  virtual void Layout() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;

  // SingleSplitView's preferred size is the sum of the preferred widths
  // and the max of the heights.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  // Overriden to return a resize cursor when over the divider.
  virtual gfx::NativeCursor GetCursor(const ui::MouseEvent& event) OVERRIDE;

  Orientation orientation() const {
    return is_horizontal_ ? HORIZONTAL_SPLIT : VERTICAL_SPLIT;
  }

  void set_orientation(Orientation orientation) {
    is_horizontal_ = orientation == HORIZONTAL_SPLIT;
  }

  void set_divider_offset(int divider_offset) {
    divider_offset_ = divider_offset;
  }
  int divider_offset() const { return divider_offset_; }

  int GetDividerSize() const;

  void set_resize_disabled(bool resize_disabled) {
    resize_disabled_ = resize_disabled;
  }
  bool is_resize_disabled() const { return resize_disabled_; }

  // Sets whether the leading component is resized when the split views size
  // changes. The default is true. A value of false results in the trailing
  // component resizing on a bounds change.
  void set_resize_leading_on_bounds_change(bool resize) {
    resize_leading_on_bounds_change_ = resize;
  }

  // Calculates ideal leading and trailing view bounds according to the given
  // split view |bounds|, current divider offset and children visiblity.
  // Does not change children view bounds.
  void CalculateChildrenBounds(const gfx::Rect& bounds,
                               gfx::Rect* leading_bounds,
                               gfx::Rect* trailing_bounds) const;

  void SetAccessibleName(const base::string16& name);

 protected:
  // View overrides.
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  // This test calls OnMouse* functions.
  FRIEND_TEST_ALL_PREFIXES(SingleSplitViewTest, MouseDrag);

  // Returns true if |x| or |y| is over the divider.
  bool IsPointInDivider(const gfx::Point& p);

  // Calculates the new |divider_offset| based on the changes of split view
  // bounds.
  int CalculateDividerOffset(int divider_offset,
                             const gfx::Rect& previous_bounds,
                             const gfx::Rect& new_bounds) const;

  // Returns divider offset within primary axis size range for given split
  // view |bounds|.
  int NormalizeDividerOffset(int divider_offset, const gfx::Rect& bounds) const;

  // Returns width in case of horizontal split and height otherwise.
  int GetPrimaryAxisSize() const {
    return GetPrimaryAxisSize(width(), height());
  }

  int GetPrimaryAxisSize(int h, int v) const {
    return is_horizontal_ ? h : v;
  }

  // Used to track drag info.
  struct DragInfo {
    // The initial coordinate of the mouse when the user started the drag.
    int initial_mouse_offset;
    // The initial position of the divider when the user started the drag.
    int initial_divider_offset;
  };

  DragInfo drag_info_;

  // Orientation of the split view.
  bool is_horizontal_;

  // Position of the divider.
  int divider_offset_;

  bool resize_leading_on_bounds_change_;

  // Whether resizing is disabled.
  bool resize_disabled_;

  // Listener to notify about user initiated handle movements. Not owned.
  SingleSplitViewListener* listener_;

  // The accessible name of this view.
  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(SingleSplitView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SINGLE_SPLIT_VIEW_H_
