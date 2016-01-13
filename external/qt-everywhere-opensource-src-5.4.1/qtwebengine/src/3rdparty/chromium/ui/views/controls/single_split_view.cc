// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/single_split_view.h"

#include "skia/ext/skia_utils_win.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/single_split_view_listener.h"
#include "ui/views/native_cursor.h"

namespace views {

// static
const char SingleSplitView::kViewClassName[] = "SingleSplitView";

// Size of the divider in pixels.
static const int kDividerSize = 4;

SingleSplitView::SingleSplitView(View* leading,
                                 View* trailing,
                                 Orientation orientation,
                                 SingleSplitViewListener* listener)
    : is_horizontal_(orientation == HORIZONTAL_SPLIT),
      divider_offset_(-1),
      resize_leading_on_bounds_change_(true),
      resize_disabled_(false),
      listener_(listener) {
  AddChildView(leading);
  AddChildView(trailing);
#if defined(OS_WIN)
  set_background(
      views::Background::CreateSolidBackground(
          skia::COLORREFToSkColor(GetSysColor(COLOR_3DFACE))));
#endif
}

void SingleSplitView::Layout() {
  gfx::Rect leading_bounds;
  gfx::Rect trailing_bounds;
  CalculateChildrenBounds(bounds(), &leading_bounds, &trailing_bounds);

  if (has_children()) {
    if (child_at(0)->visible())
      child_at(0)->SetBoundsRect(leading_bounds);
    if (child_count() > 1) {
      if (child_at(1)->visible())
        child_at(1)->SetBoundsRect(trailing_bounds);
    }
  }

  // Invoke super's implementation so that the children are layed out.
  View::Layout();
}

const char* SingleSplitView::GetClassName() const {
  return kViewClassName;
}

void SingleSplitView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_GROUP;
  state->name = accessible_name_;
}

gfx::Size SingleSplitView::GetPreferredSize() const {
  int width = 0;
  int height = 0;
  for (int i = 0; i < 2 && i < child_count(); ++i) {
    const View* view = child_at(i);
    gfx::Size pref = view->GetPreferredSize();
    if (is_horizontal_) {
      width += pref.width();
      height = std::max(height, pref.height());
    } else {
      width = std::max(width, pref.width());
      height += pref.height();
    }
  }
  if (is_horizontal_)
    width += GetDividerSize();
  else
    height += GetDividerSize();
  return gfx::Size(width, height);
}

gfx::NativeCursor SingleSplitView::GetCursor(const ui::MouseEvent& event) {
  if (!IsPointInDivider(event.location()))
    return gfx::kNullCursor;
  return is_horizontal_ ? GetNativeEastWestResizeCursor()
                        : GetNativeNorthSouthResizeCursor();
}

int SingleSplitView::GetDividerSize() const {
  bool both_visible = child_count() > 1 && child_at(0)->visible() &&
      child_at(1)->visible();
  return both_visible && !resize_disabled_ ? kDividerSize : 0;
}

void SingleSplitView::CalculateChildrenBounds(
    const gfx::Rect& bounds,
    gfx::Rect* leading_bounds,
    gfx::Rect* trailing_bounds) const {
  bool is_leading_visible = has_children() && child_at(0)->visible();
  bool is_trailing_visible = child_count() > 1 && child_at(1)->visible();

  if (!is_leading_visible && !is_trailing_visible) {
    *leading_bounds = gfx::Rect();
    *trailing_bounds = gfx::Rect();
    return;
  }

  int divider_at;

  if (!is_trailing_visible) {
    divider_at = GetPrimaryAxisSize(bounds.width(), bounds.height());
  } else if (!is_leading_visible) {
    divider_at = 0;
  } else {
    divider_at =
        CalculateDividerOffset(divider_offset_, this->bounds(), bounds);
    divider_at = NormalizeDividerOffset(divider_at, bounds);
  }

  int divider_size = GetDividerSize();

  if (is_horizontal_) {
    *leading_bounds = gfx::Rect(0, 0, divider_at, bounds.height());
    *trailing_bounds =
        gfx::Rect(divider_at + divider_size, 0,
                  std::max(0, bounds.width() - divider_at - divider_size),
                  bounds.height());
  } else {
    *leading_bounds = gfx::Rect(0, 0, bounds.width(), divider_at);
    *trailing_bounds =
        gfx::Rect(0, divider_at + divider_size, bounds.width(),
                  std::max(0, bounds.height() - divider_at - divider_size));
  }
}

void SingleSplitView::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

bool SingleSplitView::OnMousePressed(const ui::MouseEvent& event) {
  if (!IsPointInDivider(event.location()))
    return false;
  drag_info_.initial_mouse_offset = GetPrimaryAxisSize(event.x(), event.y());
  drag_info_.initial_divider_offset =
      NormalizeDividerOffset(divider_offset_, bounds());
  return true;
}

bool SingleSplitView::OnMouseDragged(const ui::MouseEvent& event) {
  if (child_count() < 2)
    return false;

  int delta_offset = GetPrimaryAxisSize(event.x(), event.y()) -
      drag_info_.initial_mouse_offset;
  if (is_horizontal_ && base::i18n::IsRTL())
    delta_offset *= -1;
  // Honor the first child's minimum size when resizing.
  gfx::Size min = child_at(0)->GetMinimumSize();
  int new_size = std::max(GetPrimaryAxisSize(min.width(), min.height()),
                          drag_info_.initial_divider_offset + delta_offset);

  // Honor the second child's minimum size, and don't let the view
  // get bigger than our width.
  min = child_at(1)->GetMinimumSize();
  new_size = std::min(GetPrimaryAxisSize() - kDividerSize -
      GetPrimaryAxisSize(min.width(), min.height()), new_size);

  if (new_size != divider_offset_) {
    set_divider_offset(new_size);
    if (!listener_ || listener_->SplitHandleMoved(this))
      Layout();
  }
  return true;
}

void SingleSplitView::OnMouseCaptureLost() {
  if (child_count() < 2)
    return;

  if (drag_info_.initial_divider_offset != divider_offset_) {
    set_divider_offset(drag_info_.initial_divider_offset);
    if (!listener_ || listener_->SplitHandleMoved(this))
      Layout();
  }
}

void SingleSplitView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  divider_offset_ = CalculateDividerOffset(divider_offset_, previous_bounds,
                                           bounds());
}

bool SingleSplitView::IsPointInDivider(const gfx::Point& p) {
  if (resize_disabled_)
    return false;

  if (child_count() < 2)
    return false;

  if (!child_at(0)->visible() || !child_at(1)->visible())
    return false;

  int divider_relative_offset;
  if (is_horizontal_) {
    divider_relative_offset =
        p.x() - child_at(base::i18n::IsRTL() ? 1 : 0)->width();
  } else {
    divider_relative_offset = p.y() - child_at(0)->height();
  }
  return (divider_relative_offset >= 0 &&
      divider_relative_offset < GetDividerSize());
}

int SingleSplitView::CalculateDividerOffset(
    int divider_offset,
    const gfx::Rect& previous_bounds,
    const gfx::Rect& new_bounds) const {
  if (resize_leading_on_bounds_change_ && divider_offset != -1) {
    // We do not update divider_offset on minimize (to zero) and on restore
    // (to largest value). As a result we get back to the original value upon
    // window restore.
    bool is_minimize_or_restore =
        previous_bounds.height() == 0 || new_bounds.height() == 0;
    if (!is_minimize_or_restore) {
      if (is_horizontal_)
        divider_offset += new_bounds.width() - previous_bounds.width();
      else
        divider_offset += new_bounds.height() - previous_bounds.height();

      if (divider_offset < 0)
        divider_offset = GetDividerSize();
    }
  }
  return divider_offset;
}

int SingleSplitView::NormalizeDividerOffset(int divider_offset,
                                            const gfx::Rect& bounds) const {
  int primary_axis_size = GetPrimaryAxisSize(bounds.width(), bounds.height());
  if (divider_offset < 0)
    // primary_axis_size may < GetDividerSize during initial layout.
    return std::max(0, (primary_axis_size - GetDividerSize()) / 2);
  return std::min(divider_offset,
                  std::max(primary_axis_size - GetDividerSize(), 0));
}

}  // namespace views
