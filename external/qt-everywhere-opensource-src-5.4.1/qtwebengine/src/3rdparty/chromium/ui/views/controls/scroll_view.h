// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLL_VIEW_H_
#define UI_VIEWS_CONTROLS_SCROLL_VIEW_H_

#include <string>

#include "base/compiler_specific.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// ScrollView class
//
// A ScrollView is used to make any View scrollable. The view is added to
// a viewport which takes care of clipping.
//
// In this current implementation both horizontal and vertical scrollbars are
// added as needed.
//
// The scrollview supports keyboard UI and mousewheel.
//
/////////////////////////////////////////////////////////////////////////////

class VIEWS_EXPORT ScrollView : public View, public ScrollBarController {
 public:
  static const char kViewClassName[];

  ScrollView();

  virtual ~ScrollView();

  // Creates a ScrollView with a theme specific border.
  static ScrollView* CreateScrollViewWithBorder();

  // Set the contents. Any previous contents will be deleted. The contents
  // is the view that needs to scroll.
  void SetContents(View* a_view);
  const View* contents() const { return contents_; }
  View* contents() { return contents_; }

  // Sets the header, deleting the previous header.
  void SetHeader(View* header);

  // Returns the visible region of the content View.
  gfx::Rect GetVisibleRect() const;

  void set_hide_horizontal_scrollbar(bool visible) {
    hide_horizontal_scrollbar_ = visible;
  }

  // Turns this scroll view into a bounded scroll view, with a fixed height.
  // By default, a ScrollView will stretch to fill its outer container.
  void ClipHeightTo(int min_height, int max_height);

  // Returns whether or not the ScrollView is bounded (as set by ClipHeightTo).
  bool is_bounded() const { return max_height_ >= 0 && min_height_ >= 0; }

  // Retrieves the width/height of scrollbars. These return 0 if the scrollbar
  // has not yet been created.
  int GetScrollBarWidth() const;
  int GetScrollBarHeight() const;

  // Returns the horizontal/vertical scrollbar. This may return NULL.
  const ScrollBar* horizontal_scroll_bar() const { return horiz_sb_; }
  const ScrollBar* vertical_scroll_bar() const { return vert_sb_; }

  // Customize the scrollbar design. ScrollView takes the ownership of the
  // specified ScrollBar. |horiz_sb| and |vert_sb| cannot be NULL.
  void SetHorizontalScrollBar(ScrollBar* horiz_sb);
  void SetVerticalScrollBar(ScrollBar* vert_sb);

  // View overrides:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual int GetHeightForWidth(int width) const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& e) OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

  // ScrollBarController overrides:
  virtual void ScrollToPosition(ScrollBar* source, int position) OVERRIDE;
  virtual int GetScrollIncrement(ScrollBar* source,
                                 bool is_page,
                                 bool is_positive) OVERRIDE;

 private:
  class Viewport;

  // Used internally by SetHeader() and SetContents() to reset the view.  Sets
  // |member| to |new_view|. If |new_view| is non-null it is added to |parent|.
  void SetHeaderOrContents(View* parent, View* new_view, View** member);

  // Scrolls the minimum amount necessary to make the specified rectangle
  // visible, in the coordinates of the contents view. The specified rectangle
  // is constrained by the bounds of the contents view. This has no effect if
  // the contents have not been set.
  void ScrollContentsRegionToBeVisible(const gfx::Rect& rect);

  // Computes the visibility of both scrollbars, taking in account the view port
  // and content sizes.
  void ComputeScrollBarsVisibility(const gfx::Size& viewport_size,
                                   const gfx::Size& content_size,
                                   bool* horiz_is_shown,
                                   bool* vert_is_shown) const;

  // Shows or hides the scrollbar/resize_corner based on the value of
  // |should_show|.
  void SetControlVisibility(View* control, bool should_show);

  // Update the scrollbars positions given viewport and content sizes.
  void UpdateScrollBarPositions();

  // The current contents and its viewport. |contents_| is contained in
  // |contents_viewport_|.
  View* contents_;
  View* contents_viewport_;

  // The current header and its viewport. |header_| is contained in
  // |header_viewport_|.
  View* header_;
  View* header_viewport_;

  // Horizontal scrollbar.
  ScrollBar* horiz_sb_;

  // Vertical scrollbar.
  ScrollBar* vert_sb_;

  // Resize corner.
  View* resize_corner_;

  // The min and max height for the bounded scroll view. These are negative
  // values if the view is not bounded.
  int min_height_;
  int max_height_;

  // If true, never show the horizontal scrollbar (even if the contents is wider
  // than the viewport).
  bool hide_horizontal_scrollbar_;

  DISALLOW_COPY_AND_ASSIGN(ScrollView);
};

// VariableRowHeightScrollHelper is intended for views that contain rows of
// varying height. To use a VariableRowHeightScrollHelper create one supplying
// a Controller and delegate GetPageScrollIncrement and GetLineScrollIncrement
// to the helper. VariableRowHeightScrollHelper calls back to the
// Controller to determine row boundaries.
class VariableRowHeightScrollHelper {
 public:
  // The origin and height of a row.
  struct RowInfo {
    RowInfo(int origin, int height) : origin(origin), height(height) {}

    // Origin of the row.
    int origin;

    // Height of the row.
    int height;
  };

  // Used to determine row boundaries.
  class Controller {
   public:
    // Returns the origin and size of the row at the specified location.
    virtual VariableRowHeightScrollHelper::RowInfo GetRowInfo(int y) = 0;
  };

  // Creates a new VariableRowHeightScrollHelper. Controller is
  // NOT deleted by this VariableRowHeightScrollHelper.
  explicit VariableRowHeightScrollHelper(Controller* controller);
  virtual ~VariableRowHeightScrollHelper();

  // Delegate the View methods of the same name to these. The scroll amount is
  // determined by querying the Controller for the appropriate row to scroll
  // to.
  int GetPageScrollIncrement(ScrollView* scroll_view,
                             bool is_horizontal, bool is_positive);
  int GetLineScrollIncrement(ScrollView* scroll_view,
                             bool is_horizontal, bool is_positive);

 protected:
  // Returns the row information for the row at the specified location. This
  // calls through to the method of the same name on the controller.
  virtual RowInfo GetRowInfo(int y);

 private:
  Controller* controller_;

  DISALLOW_COPY_AND_ASSIGN(VariableRowHeightScrollHelper);
};

// FixedRowHeightScrollHelper is intended for views that contain fixed height
// height rows. To use a FixedRowHeightScrollHelper delegate
// GetPageScrollIncrement and GetLineScrollIncrement to it.
class FixedRowHeightScrollHelper : public VariableRowHeightScrollHelper {
 public:
  // Creates a FixedRowHeightScrollHelper. top_margin gives the distance from
  // the top of the view to the first row, and may be 0. row_height gives the
  // height of each row.
  FixedRowHeightScrollHelper(int top_margin, int row_height);

 protected:
  // Calculates the bounds of the row from the top margin and row height.
  virtual RowInfo GetRowInfo(int y) OVERRIDE;

 private:
  int top_margin_;
  int row_height_;

  DISALLOW_COPY_AND_ASSIGN(FixedRowHeightScrollHelper);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLL_VIEW_H_
