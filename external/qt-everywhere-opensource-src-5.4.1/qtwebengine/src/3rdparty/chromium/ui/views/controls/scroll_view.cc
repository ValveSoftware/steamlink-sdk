// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scroll_view.h"

#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/scrollbar/native_scroll_bar.h"
#include "ui/views/widget/root_view.h"

namespace views {

const char ScrollView::kViewClassName[] = "ScrollView";

namespace {

// Subclass of ScrollView that resets the border when the theme changes.
class ScrollViewWithBorder : public views::ScrollView {
 public:
  ScrollViewWithBorder() {}

  // View overrides;
  virtual void OnNativeThemeChanged(const ui::NativeTheme* theme) OVERRIDE {
    SetBorder(Border::CreateSolidBorder(
        1,
        theme->GetSystemColor(ui::NativeTheme::kColorId_UnfocusedBorderColor)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollViewWithBorder);
};

// Returns the position for the view so that it isn't scrolled off the visible
// region.
int CheckScrollBounds(int viewport_size, int content_size, int current_pos) {
  int max = std::max(content_size - viewport_size, 0);
  if (current_pos < 0)
    return 0;
  if (current_pos > max)
    return max;
  return current_pos;
}

// Make sure the content is not scrolled out of bounds
void CheckScrollBounds(View* viewport, View* view) {
  if (!view)
    return;

  int x = CheckScrollBounds(viewport->width(), view->width(), -view->x());
  int y = CheckScrollBounds(viewport->height(), view->height(), -view->y());

  // This is no op if bounds are the same
  view->SetBounds(-x, -y, view->width(), view->height());
}

// Used by ScrollToPosition() to make sure the new position fits within the
// allowed scroll range.
int AdjustPosition(int current_position,
                   int new_position,
                   int content_size,
                   int viewport_size) {
  if (-current_position == new_position)
    return new_position;
  if (new_position < 0)
    return 0;
  const int max_position = std::max(0, content_size - viewport_size);
  return (new_position > max_position) ? max_position : new_position;
}

}  // namespace

// Viewport contains the contents View of the ScrollView.
class ScrollView::Viewport : public View {
 public:
  Viewport() {}
  virtual ~Viewport() {}

  virtual const char* GetClassName() const OVERRIDE {
    return "ScrollView::Viewport";
  }

  virtual void ScrollRectToVisible(const gfx::Rect& rect) OVERRIDE {
    if (!has_children() || !parent())
      return;

    View* contents = child_at(0);
    gfx::Rect scroll_rect(rect);
    scroll_rect.Offset(-contents->x(), -contents->y());
    static_cast<ScrollView*>(parent())->ScrollContentsRegionToBeVisible(
        scroll_rect);
  }

  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE {
    if (parent())
      parent()->Layout();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Viewport);
};

ScrollView::ScrollView()
    : contents_(NULL),
      contents_viewport_(new Viewport()),
      header_(NULL),
      header_viewport_(new Viewport()),
      horiz_sb_(new NativeScrollBar(true)),
      vert_sb_(new NativeScrollBar(false)),
      resize_corner_(NULL),
      min_height_(-1),
      max_height_(-1),
      hide_horizontal_scrollbar_(false) {
  set_notify_enter_exit_on_child(true);

  AddChildView(contents_viewport_);
  AddChildView(header_viewport_);

  // Don't add the scrollbars as children until we discover we need them
  // (ShowOrHideScrollBar).
  horiz_sb_->SetVisible(false);
  horiz_sb_->set_controller(this);
  vert_sb_->SetVisible(false);
  vert_sb_->set_controller(this);
  if (resize_corner_)
    resize_corner_->SetVisible(false);
}

ScrollView::~ScrollView() {
  // The scrollbars may not have been added, delete them to ensure they get
  // deleted.
  delete horiz_sb_;
  delete vert_sb_;

  if (resize_corner_ && !resize_corner_->parent())
    delete resize_corner_;
}

// static
ScrollView* ScrollView::CreateScrollViewWithBorder() {
  return new ScrollViewWithBorder();
}

void ScrollView::SetContents(View* a_view) {
  SetHeaderOrContents(contents_viewport_, a_view, &contents_);
}

void ScrollView::SetHeader(View* header) {
  SetHeaderOrContents(header_viewport_, header, &header_);
}

gfx::Rect ScrollView::GetVisibleRect() const {
  if (!contents_)
    return gfx::Rect();
  return gfx::Rect(-contents_->x(), -contents_->y(),
                   contents_viewport_->width(), contents_viewport_->height());
}

void ScrollView::ClipHeightTo(int min_height, int max_height) {
  min_height_ = min_height;
  max_height_ = max_height;
}

int ScrollView::GetScrollBarWidth() const {
  return vert_sb_ ? vert_sb_->GetLayoutSize() : 0;
}

int ScrollView::GetScrollBarHeight() const {
  return horiz_sb_ ? horiz_sb_->GetLayoutSize() : 0;
}

void ScrollView::SetHorizontalScrollBar(ScrollBar* horiz_sb) {
  DCHECK(horiz_sb);
  horiz_sb->SetVisible(horiz_sb_->visible());
  delete horiz_sb_;
  horiz_sb->set_controller(this);
  horiz_sb_ = horiz_sb;
}

void ScrollView::SetVerticalScrollBar(ScrollBar* vert_sb) {
  DCHECK(vert_sb);
  vert_sb->SetVisible(vert_sb_->visible());
  delete vert_sb_;
  vert_sb->set_controller(this);
  vert_sb_ = vert_sb;
}

gfx::Size ScrollView::GetPreferredSize() const {
  if (!is_bounded())
    return View::GetPreferredSize();

  gfx::Size size = contents()->GetPreferredSize();
  size.SetToMax(gfx::Size(size.width(), min_height_));
  size.SetToMin(gfx::Size(size.width(), max_height_));
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

int ScrollView::GetHeightForWidth(int width) const {
  if (!is_bounded())
    return View::GetHeightForWidth(width);

  gfx::Insets insets = GetInsets();
  width = std::max(0, width - insets.width());
  int height = contents()->GetHeightForWidth(width) + insets.height();
  return std::min(std::max(height, min_height_), max_height_);
}

void ScrollView::Layout() {
  if (is_bounded()) {
    int content_width = width();
    int content_height = contents()->GetHeightForWidth(content_width);
    if (content_height > height()) {
      content_width = std::max(content_width - GetScrollBarWidth(), 0);
      content_height = contents()->GetHeightForWidth(content_width);
    }
    if (contents()->bounds().size() != gfx::Size(content_width, content_height))
      contents()->SetBounds(0, 0, content_width, content_height);
  }

  // Most views will want to auto-fit the available space. Most of them want to
  // use all available width (without overflowing) and only overflow in
  // height. Examples are HistoryView, MostVisitedView, DownloadTabView, etc.
  // Other views want to fit in both ways. An example is PrintView. To make both
  // happy, assume a vertical scrollbar but no horizontal scrollbar. To override
  // this default behavior, the inner view has to calculate the available space,
  // used ComputeScrollBarsVisibility() to use the same calculation that is done
  // here and sets its bound to fit within.
  gfx::Rect viewport_bounds = GetContentsBounds();
  const int contents_x = viewport_bounds.x();
  const int contents_y = viewport_bounds.y();
  if (viewport_bounds.IsEmpty()) {
    // There's nothing to layout.
    return;
  }

  const int header_height =
      std::min(viewport_bounds.height(),
               header_ ? header_->GetPreferredSize().height() : 0);
  viewport_bounds.set_height(
      std::max(0, viewport_bounds.height() - header_height));
  viewport_bounds.set_y(viewport_bounds.y() + header_height);
  // viewport_size is the total client space available.
  gfx::Size viewport_size = viewport_bounds.size();
  // Assumes a vertical scrollbar since most of the current views are designed
  // for this.
  int horiz_sb_height = GetScrollBarHeight();
  int vert_sb_width = GetScrollBarWidth();
  viewport_bounds.set_width(viewport_bounds.width() - vert_sb_width);
  // Update the bounds right now so the inner views can fit in it.
  contents_viewport_->SetBoundsRect(viewport_bounds);

  // Give |contents_| a chance to update its bounds if it depends on the
  // viewport.
  if (contents_)
    contents_->Layout();

  bool should_layout_contents = false;
  bool horiz_sb_required = false;
  bool vert_sb_required = false;
  if (contents_) {
    gfx::Size content_size = contents_->size();
    ComputeScrollBarsVisibility(viewport_size,
                                content_size,
                                &horiz_sb_required,
                                &vert_sb_required);
  }
  bool resize_corner_required = resize_corner_ && horiz_sb_required &&
                                vert_sb_required;
  // Take action.
  SetControlVisibility(horiz_sb_, horiz_sb_required);
  SetControlVisibility(vert_sb_, vert_sb_required);
  SetControlVisibility(resize_corner_, resize_corner_required);

  // Non-default.
  if (horiz_sb_required) {
    viewport_bounds.set_height(
        std::max(0, viewport_bounds.height() - horiz_sb_height));
    should_layout_contents = true;
  }
  // Default.
  if (!vert_sb_required) {
    viewport_bounds.set_width(viewport_bounds.width() + vert_sb_width);
    should_layout_contents = true;
  }

  if (horiz_sb_required) {
    int height_offset = horiz_sb_->GetContentOverlapSize();
    horiz_sb_->SetBounds(0,
                         viewport_bounds.bottom() - height_offset,
                         viewport_bounds.right(),
                         horiz_sb_height + height_offset);
  }
  if (vert_sb_required) {
    int width_offset = vert_sb_->GetContentOverlapSize();
    vert_sb_->SetBounds(viewport_bounds.right() - width_offset,
                        0,
                        vert_sb_width + width_offset,
                        viewport_bounds.bottom());
  }
  if (resize_corner_required) {
    // Show the resize corner.
    resize_corner_->SetBounds(viewport_bounds.right(),
                              viewport_bounds.bottom(),
                              vert_sb_width,
                              horiz_sb_height);
  }

  // Update to the real client size with the visible scrollbars.
  contents_viewport_->SetBoundsRect(viewport_bounds);
  if (should_layout_contents && contents_)
    contents_->Layout();

  header_viewport_->SetBounds(contents_x, contents_y,
                              viewport_bounds.width(), header_height);
  if (header_)
    header_->Layout();

  CheckScrollBounds(header_viewport_, header_);
  CheckScrollBounds(contents_viewport_, contents_);
  SchedulePaint();
  UpdateScrollBarPositions();
}

bool ScrollView::OnKeyPressed(const ui::KeyEvent& event) {
  bool processed = false;

  // Give vertical scrollbar priority
  if (vert_sb_->visible())
    processed = vert_sb_->OnKeyPressed(event);

  if (!processed && horiz_sb_->visible())
    processed = horiz_sb_->OnKeyPressed(event);

  return processed;
}

bool ScrollView::OnMouseWheel(const ui::MouseWheelEvent& e) {
  bool processed = false;
  // Give vertical scrollbar priority
  if (vert_sb_->visible())
    processed = vert_sb_->OnMouseWheel(e);

  if (!processed && horiz_sb_->visible())
    processed = horiz_sb_->OnMouseWheel(e);

  return processed;
}

void ScrollView::OnMouseEntered(const ui::MouseEvent& event) {
  if (horiz_sb_)
    horiz_sb_->OnMouseEnteredScrollView(event);
  if (vert_sb_)
    vert_sb_->OnMouseEnteredScrollView(event);
}

void ScrollView::OnMouseExited(const ui::MouseEvent& event) {
  if (horiz_sb_)
    horiz_sb_->OnMouseExitedScrollView(event);
  if (vert_sb_)
    vert_sb_->OnMouseExitedScrollView(event);
}

void ScrollView::OnGestureEvent(ui::GestureEvent* event) {
  // If the event happened on one of the scrollbars, then those events are
  // sent directly to the scrollbars. Otherwise, only scroll events are sent to
  // the scrollbars.
  bool scroll_event = event->type() == ui::ET_GESTURE_SCROLL_UPDATE ||
                      event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
                      event->type() == ui::ET_GESTURE_SCROLL_END ||
                      event->type() == ui::ET_SCROLL_FLING_START;

  if (vert_sb_->visible()) {
    if (vert_sb_->bounds().Contains(event->location()) || scroll_event)
      vert_sb_->OnGestureEvent(event);
  }
  if (!event->handled() && horiz_sb_->visible()) {
    if (horiz_sb_->bounds().Contains(event->location()) || scroll_event)
      horiz_sb_->OnGestureEvent(event);
  }
}

const char* ScrollView::GetClassName() const {
  return kViewClassName;
}

void ScrollView::ScrollToPosition(ScrollBar* source, int position) {
  if (!contents_)
    return;

  if (source == horiz_sb_ && horiz_sb_->visible()) {
    position = AdjustPosition(contents_->x(), position, contents_->width(),
                              contents_viewport_->width());
    if (-contents_->x() == position)
      return;
    contents_->SetX(-position);
    if (header_) {
      header_->SetX(-position);
      header_->SchedulePaintInRect(header_->GetVisibleBounds());
    }
  } else if (source == vert_sb_ && vert_sb_->visible()) {
    position = AdjustPosition(contents_->y(), position, contents_->height(),
                              contents_viewport_->height());
    if (-contents_->y() == position)
      return;
    contents_->SetY(-position);
  }
  contents_->SchedulePaintInRect(contents_->GetVisibleBounds());
}

int ScrollView::GetScrollIncrement(ScrollBar* source, bool is_page,
                                   bool is_positive) {
  bool is_horizontal = source->IsHorizontal();
  int amount = 0;
  if (contents_) {
    if (is_page) {
      amount = contents_->GetPageScrollIncrement(
          this, is_horizontal, is_positive);
    } else {
      amount = contents_->GetLineScrollIncrement(
          this, is_horizontal, is_positive);
    }
    if (amount > 0)
      return amount;
  }
  // No view, or the view didn't return a valid amount.
  if (is_page) {
    return is_horizontal ? contents_viewport_->width() :
                           contents_viewport_->height();
  }
  return is_horizontal ? contents_viewport_->width() / 5 :
                         contents_viewport_->height() / 5;
}

void ScrollView::SetHeaderOrContents(View* parent,
                                     View* new_view,
                                     View** member) {
  if (*member == new_view)
    return;

  delete *member;
  *member = new_view;
  if (*member)
    parent->AddChildView(*member);
  Layout();
}

void ScrollView::ScrollContentsRegionToBeVisible(const gfx::Rect& rect) {
  if (!contents_ || (!horiz_sb_->visible() && !vert_sb_->visible()))
    return;

  // Figure out the maximums for this scroll view.
  const int contents_max_x =
      std::max(contents_viewport_->width(), contents_->width());
  const int contents_max_y =
      std::max(contents_viewport_->height(), contents_->height());

  // Make sure x and y are within the bounds of [0,contents_max_*].
  int x = std::max(0, std::min(contents_max_x, rect.x()));
  int y = std::max(0, std::min(contents_max_y, rect.y()));

  // Figure out how far and down the rectangle will go taking width
  // and height into account.  This will be "clipped" by the viewport.
  const int max_x = std::min(contents_max_x,
      x + std::min(rect.width(), contents_viewport_->width()));
  const int max_y = std::min(contents_max_y,
      y + std::min(rect.height(), contents_viewport_->height()));

  // See if the rect is already visible. Note the width is (max_x - x)
  // and the height is (max_y - y) to take into account the clipping of
  // either viewport or the content size.
  const gfx::Rect vis_rect = GetVisibleRect();
  if (vis_rect.Contains(gfx::Rect(x, y, max_x - x, max_y - y)))
    return;

  // Shift contents_'s X and Y so that the region is visible. If we
  // need to shift up or left from where we currently are then we need
  // to get it so that the content appears in the upper/left
  // corner. This is done by setting the offset to -X or -Y.  For down
  // or right shifts we need to make sure it appears in the
  // lower/right corner. This is calculated by taking max_x or max_y
  // and scaling it back by the size of the viewport.
  const int new_x =
      (vis_rect.x() > x) ? x : std::max(0, max_x - contents_viewport_->width());
  const int new_y =
      (vis_rect.y() > y) ? y : std::max(0, max_y -
                                        contents_viewport_->height());

  contents_->SetX(-new_x);
  if (header_)
    header_->SetX(-new_x);
  contents_->SetY(-new_y);
  UpdateScrollBarPositions();
}

void ScrollView::ComputeScrollBarsVisibility(const gfx::Size& vp_size,
                                             const gfx::Size& content_size,
                                             bool* horiz_is_shown,
                                             bool* vert_is_shown) const {
  // Try to fit both ways first, then try vertical bar only, then horizontal
  // bar only, then defaults to both shown.
  if (content_size.width() <= vp_size.width() &&
      content_size.height() <= vp_size.height()) {
    *horiz_is_shown = false;
    *vert_is_shown = false;
  } else if (content_size.width() <= vp_size.width() - GetScrollBarWidth()) {
    *horiz_is_shown = false;
    *vert_is_shown = true;
  } else if (content_size.height() <= vp_size.height() - GetScrollBarHeight()) {
    *horiz_is_shown = true;
    *vert_is_shown = false;
  } else {
    *horiz_is_shown = true;
    *vert_is_shown = true;
  }

  if (hide_horizontal_scrollbar_)
    *horiz_is_shown = false;
}

// Make sure that a single scrollbar is created and visible as needed
void ScrollView::SetControlVisibility(View* control, bool should_show) {
  if (!control)
    return;
  if (should_show) {
    if (!control->visible()) {
      AddChildView(control);
      control->SetVisible(true);
    }
  } else {
    RemoveChildView(control);
    control->SetVisible(false);
  }
}

void ScrollView::UpdateScrollBarPositions() {
  if (!contents_)
    return;

  if (horiz_sb_->visible()) {
    int vw = contents_viewport_->width();
    int cw = contents_->width();
    int origin = contents_->x();
    horiz_sb_->Update(vw, cw, -origin);
  }
  if (vert_sb_->visible()) {
    int vh = contents_viewport_->height();
    int ch = contents_->height();
    int origin = contents_->y();
    vert_sb_->Update(vh, ch, -origin);
  }
}

// VariableRowHeightScrollHelper ----------------------------------------------

VariableRowHeightScrollHelper::VariableRowHeightScrollHelper(
    Controller* controller) : controller_(controller) {
}

VariableRowHeightScrollHelper::~VariableRowHeightScrollHelper() {
}

int VariableRowHeightScrollHelper::GetPageScrollIncrement(
    ScrollView* scroll_view, bool is_horizontal, bool is_positive) {
  if (is_horizontal)
    return 0;
  // y coordinate is most likely negative.
  int y = abs(scroll_view->contents()->y());
  int vis_height = scroll_view->contents()->parent()->height();
  if (is_positive) {
    // Align the bottom most row to the top of the view.
    int bottom = std::min(scroll_view->contents()->height() - 1,
                          y + vis_height);
    RowInfo bottom_row_info = GetRowInfo(bottom);
    // If 0, ScrollView will provide a default value.
    return std::max(0, bottom_row_info.origin - y);
  } else {
    // Align the row on the previous page to to the top of the view.
    int last_page_y = y - vis_height;
    RowInfo last_page_info = GetRowInfo(std::max(0, last_page_y));
    if (last_page_y != last_page_info.origin)
      return std::max(0, y - last_page_info.origin - last_page_info.height);
    return std::max(0, y - last_page_info.origin);
  }
}

int VariableRowHeightScrollHelper::GetLineScrollIncrement(
    ScrollView* scroll_view, bool is_horizontal, bool is_positive) {
  if (is_horizontal)
    return 0;
  // y coordinate is most likely negative.
  int y = abs(scroll_view->contents()->y());
  RowInfo row = GetRowInfo(y);
  if (is_positive) {
    return row.height - (y - row.origin);
  } else if (y == row.origin) {
    row = GetRowInfo(std::max(0, row.origin - 1));
    return y - row.origin;
  } else {
    return y - row.origin;
  }
}

VariableRowHeightScrollHelper::RowInfo
    VariableRowHeightScrollHelper::GetRowInfo(int y) {
  return controller_->GetRowInfo(y);
}

// FixedRowHeightScrollHelper -----------------------------------------------

FixedRowHeightScrollHelper::FixedRowHeightScrollHelper(int top_margin,
                                                       int row_height)
    : VariableRowHeightScrollHelper(NULL),
      top_margin_(top_margin),
      row_height_(row_height) {
  DCHECK_GT(row_height, 0);
}

VariableRowHeightScrollHelper::RowInfo
    FixedRowHeightScrollHelper::GetRowInfo(int y) {
  if (y < top_margin_)
    return RowInfo(0, top_margin_);
  return RowInfo((y - top_margin_) / row_height_ * row_height_ + top_margin_,
                 row_height_);
}

}  // namespace views
