// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/resize_area.h"

#include "base/logging.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/native_cursor.h"

namespace views {

const char ResizeArea::kViewClassName[] = "ResizeArea";

////////////////////////////////////////////////////////////////////////////////
// ResizeArea

ResizeArea::ResizeArea(ResizeAreaDelegate* delegate)
    : delegate_(delegate),
      initial_position_(0) {
}

ResizeArea::~ResizeArea() {
}

const char* ResizeArea::GetClassName() const {
  return kViewClassName;
}

gfx::NativeCursor ResizeArea::GetCursor(const ui::MouseEvent& event) {
  return enabled() ? GetNativeEastWestResizeCursor()
                   : gfx::kNullCursor;
}

bool ResizeArea::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;

  // The resize area obviously will move once you start dragging so we need to
  // convert coordinates to screen coordinates so that we don't lose our
  // bearings.
  gfx::Point point(event.x(), 0);
  View::ConvertPointToScreen(this, &point);
  initial_position_ = point.x();

  return true;
}

bool ResizeArea::OnMouseDragged(const ui::MouseEvent& event) {
  if (!event.IsLeftMouseButton())
    return false;

  ReportResizeAmount(event.x(), false);
  return true;
}

void ResizeArea::OnMouseReleased(const ui::MouseEvent& event) {
  ReportResizeAmount(event.x(), true);
}

void ResizeArea::OnMouseCaptureLost() {
  ReportResizeAmount(initial_position_, true);
}

void ResizeArea::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_SPLITTER;
}

void ResizeArea::ReportResizeAmount(int resize_amount, bool last_update) {
  gfx::Point point(resize_amount, 0);
  View::ConvertPointToScreen(this, &point);
  resize_amount = point.x() - initial_position_;
  delegate_->OnResize(base::i18n::IsRTL() ? -resize_amount : resize_amount,
                      last_update);
}

}  // namespace views
