// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/non_client_view.h"

#include "ui/accessibility/ax_view_state.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"

namespace views {

// static
const char NonClientFrameView::kViewClassName[] =
    "ui/views/window/NonClientFrameView";

const char NonClientView::kViewClassName[] =
    "ui/views/window/NonClientView";

// The frame view and the client view are always at these specific indices,
// because the RootView message dispatch sends messages to items higher in the
// z-order first and we always want the client view to have first crack at
// handling mouse messages.
static const int kFrameViewIndex = 0;
static const int kClientViewIndex = 1;
// The overlay view is always on top (index == child_count() - 1).

////////////////////////////////////////////////////////////////////////////////
// NonClientView, public:

NonClientView::NonClientView()
    : client_view_(NULL),
      overlay_view_(NULL) {
}

NonClientView::~NonClientView() {
  // This value may have been reset before the window hierarchy shuts down,
  // so we need to manually remove it.
  RemoveChildView(frame_view_.get());
}

void NonClientView::SetFrameView(NonClientFrameView* frame_view) {
  // See comment in header about ownership.
  frame_view->set_owned_by_client();
  if (frame_view_.get())
    RemoveChildView(frame_view_.get());
  frame_view_.reset(frame_view);
  if (parent())
    AddChildViewAt(frame_view_.get(), kFrameViewIndex);
}

void NonClientView::SetOverlayView(View* view) {
  if (overlay_view_)
    RemoveChildView(overlay_view_);

  if (!view)
    return;

  overlay_view_ = view;
  if (parent())
    AddChildView(overlay_view_);
}

bool NonClientView::CanClose() {
  return client_view_->CanClose();
}

void NonClientView::WindowClosing() {
  client_view_->WidgetClosing();
}

void NonClientView::UpdateFrame() {
  Widget* widget = GetWidget();
  SetFrameView(widget->CreateNonClientFrameView());
  widget->ThemeChanged();
  Layout();
  SchedulePaint();
}

void NonClientView::SetInactiveRenderingDisabled(bool disable) {
  frame_view_->SetInactiveRenderingDisabled(disable);
}

gfx::Rect NonClientView::GetWindowBoundsForClientBounds(
    const gfx::Rect client_bounds) const {
  return frame_view_->GetWindowBoundsForClientBounds(client_bounds);
}

int NonClientView::NonClientHitTest(const gfx::Point& point) {
  // The NonClientFrameView is responsible for also asking the ClientView.
  return frame_view_->NonClientHitTest(point);
}

void NonClientView::GetWindowMask(const gfx::Size& size,
                                  gfx::Path* window_mask) {
  frame_view_->GetWindowMask(size, window_mask);
}

void NonClientView::ResetWindowControls() {
  frame_view_->ResetWindowControls();
}

void NonClientView::UpdateWindowIcon() {
  frame_view_->UpdateWindowIcon();
}

void NonClientView::UpdateWindowTitle() {
  frame_view_->UpdateWindowTitle();
}

void NonClientView::LayoutFrameView() {
  // First layout the NonClientFrameView, which determines the size of the
  // ClientView...
  frame_view_->SetBounds(0, 0, width(), height());

  // We need to manually call Layout here because layout for the frame view can
  // change independently of the bounds changing - e.g. after the initial
  // display of the window the metrics of the native window controls can change,
  // which does not change the bounds of the window but requires a re-layout to
  // trigger a repaint. We override OnBoundsChanged() for the NonClientFrameView
  // to do nothing so that SetBounds above doesn't cause Layout to be called
  // twice.
  frame_view_->Layout();
}

void NonClientView::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

////////////////////////////////////////////////////////////////////////////////
// NonClientView, View overrides:

gfx::Size NonClientView::GetPreferredSize() const {
  // TODO(pkasting): This should probably be made to look similar to
  // GetMinimumSize() below.  This will require implementing GetPreferredSize()
  // better in the various frame views.
  gfx::Rect client_bounds(gfx::Point(), client_view_->GetPreferredSize());
  return GetWindowBoundsForClientBounds(client_bounds).size();
}

gfx::Size NonClientView::GetMinimumSize() const {
  return frame_view_->GetMinimumSize();
}

gfx::Size NonClientView::GetMaximumSize() const {
  return frame_view_->GetMaximumSize();
}

void NonClientView::Layout() {
  LayoutFrameView();

  // Then layout the ClientView, using those bounds.
  client_view_->SetBoundsRect(frame_view_->GetBoundsForClientView());

  // We need to manually call Layout on the ClientView as well for the same
  // reason as above.
  client_view_->Layout();

  if (overlay_view_ && overlay_view_->visible())
    overlay_view_->SetBoundsRect(GetLocalBounds());
}

void NonClientView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  // Add our two child views here as we are added to the Widget so that if we
  // are subsequently resized all the parent-child relationships are
  // established.
  if (details.is_add && GetWidget() && details.child == this) {
    AddChildViewAt(frame_view_.get(), kFrameViewIndex);
    AddChildViewAt(client_view_, kClientViewIndex);
    if (overlay_view_)
      AddChildView(overlay_view_);
  }
}

void NonClientView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_CLIENT;
  state->name = accessible_name_;
}

const char* NonClientView::GetClassName() const {
  return kViewClassName;
}

View* NonClientView::GetEventHandlerForRect(const gfx::Rect& rect) {
  if (!UsePointBasedTargeting(rect))
    return View::GetEventHandlerForRect(rect);

  // Because of the z-ordering of our child views (the client view is positioned
  // over the non-client frame view, if the client view ever overlaps the frame
  // view visually (as it does for the browser window), then it will eat
  // events for the window controls. We override this method here so that we can
  // detect this condition and re-route the events to the non-client frame view.
  // The assumption is that the frame view's implementation of HitTest will only
  // return true for area not occupied by the client view.
  if (frame_view_->parent() == this) {
    // During the reset of the frame_view_ it's possible to be in this code
    // after it's been removed from the view hierarchy but before it's been
    // removed from the NonClientView.
    gfx::RectF rect_in_child_coords_f(rect);
    View::ConvertRectToTarget(this, frame_view_.get(), &rect_in_child_coords_f);
    gfx::Rect rect_in_child_coords = gfx::ToEnclosingRect(
        rect_in_child_coords_f);
    if (frame_view_->HitTestRect(rect_in_child_coords))
      return frame_view_->GetEventHandlerForRect(rect_in_child_coords);
  }

  return View::GetEventHandlerForRect(rect);
}

View* NonClientView::GetTooltipHandlerForPoint(const gfx::Point& point) {
  // The same logic as for |GetEventHandlerForRect()| applies here.
  if (frame_view_->parent() == this) {
    // During the reset of the frame_view_ it's possible to be in this code
    // after it's been removed from the view hierarchy but before it's been
    // removed from the NonClientView.
    gfx::Point point_in_child_coords(point);
    View::ConvertPointToTarget(this, frame_view_.get(), &point_in_child_coords);
    View* handler =
        frame_view_->GetTooltipHandlerForPoint(point_in_child_coords);
    if (handler)
      return handler;
  }

  return View::GetTooltipHandlerForPoint(point);
}

////////////////////////////////////////////////////////////////////////////////
// NonClientFrameView, public:

NonClientFrameView::~NonClientFrameView() {
}

void NonClientFrameView::SetInactiveRenderingDisabled(bool disable) {
  if (inactive_rendering_disabled_ == disable)
    return;

  bool should_paint_as_active_old = ShouldPaintAsActive();
  inactive_rendering_disabled_ = disable;

  // The widget schedules a paint when the activation changes.
  if (should_paint_as_active_old != ShouldPaintAsActive())
    SchedulePaint();
}

bool NonClientFrameView::ShouldPaintAsActive() const {
  return inactive_rendering_disabled_ || GetWidget()->IsActive();
}

int NonClientFrameView::GetHTComponentForFrame(const gfx::Point& point,
                                               int top_resize_border_height,
                                               int resize_border_thickness,
                                               int top_resize_corner_height,
                                               int resize_corner_width,
                                               bool can_resize) {
  // Tricky: In XP, native behavior is to return HTTOPLEFT and HTTOPRIGHT for
  // a |resize_corner_size|-length strip of both the side and top borders, but
  // only to return HTBOTTOMLEFT/HTBOTTOMRIGHT along the bottom border + corner
  // (not the side border).  Vista goes further and doesn't return these on any
  // of the side borders.  We allow callers to match either behavior.
  int component;
  if (point.x() < resize_border_thickness) {
    if (point.y() < top_resize_corner_height)
      component = HTTOPLEFT;
    else if (point.y() >= (height() - resize_border_thickness))
      component = HTBOTTOMLEFT;
    else
      component = HTLEFT;
  } else if (point.x() >= (width() - resize_border_thickness)) {
    if (point.y() < top_resize_corner_height)
      component = HTTOPRIGHT;
    else if (point.y() >= (height() - resize_border_thickness))
      component = HTBOTTOMRIGHT;
    else
      component = HTRIGHT;
  } else if (point.y() < top_resize_border_height) {
    if (point.x() < resize_corner_width)
      component = HTTOPLEFT;
    else if (point.x() >= (width() - resize_corner_width))
      component = HTTOPRIGHT;
    else
      component = HTTOP;
  } else if (point.y() >= (height() - resize_border_thickness)) {
    if (point.x() < resize_corner_width)
      component = HTBOTTOMLEFT;
    else if (point.x() >= (width() - resize_corner_width))
      component = HTBOTTOMRIGHT;
    else
      component = HTBOTTOM;
  } else {
    return HTNOWHERE;
  }

  // If the window can't be resized, there are no resize boundaries, just
  // window borders.
  return can_resize ? component : HTBORDER;
}

////////////////////////////////////////////////////////////////////////////////
// NonClientFrameView, View overrides:

bool NonClientFrameView::HitTestRect(const gfx::Rect& rect) const {
  // For the default case, we assume the non-client frame view never overlaps
  // the client view.
  return !GetWidget()->client_view()->bounds().Intersects(rect);
}

////////////////////////////////////////////////////////////////////////////////
// NonClientFrameView, protected:

void NonClientFrameView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_CLIENT;
}

const char* NonClientFrameView::GetClassName() const {
  return kViewClassName;
}

void NonClientFrameView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Overridden to do nothing. The NonClientView manually calls Layout on the
  // FrameView when it is itself laid out, see comment in NonClientView::Layout.
}

NonClientFrameView::NonClientFrameView() : inactive_rendering_disabled_(false) {
}

}  // namespace views
