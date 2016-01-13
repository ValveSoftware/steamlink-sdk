// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/screen_impl.h"

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect_conversions.h"

namespace mojo {
namespace view_manager {
namespace service {

// static
gfx::Screen* ScreenImpl::Create() {
  return new ScreenImpl(gfx::Rect(0, 0, 800, 600));
}

ScreenImpl::~ScreenImpl() {
}

bool ScreenImpl::IsDIPEnabled() {
  NOTIMPLEMENTED();
  return true;
}

gfx::Point ScreenImpl::GetCursorScreenPoint() {
  NOTIMPLEMENTED();
  return gfx::Point();
}

gfx::NativeWindow ScreenImpl::GetWindowUnderCursor() {
  return GetWindowAtScreenPoint(GetCursorScreenPoint());
}

gfx::NativeWindow ScreenImpl::GetWindowAtScreenPoint(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return NULL;
}

int ScreenImpl::GetNumDisplays() const {
  return 1;
}

std::vector<gfx::Display> ScreenImpl::GetAllDisplays() const {
  return std::vector<gfx::Display>(1, display_);
}

gfx::Display ScreenImpl::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return display_;
}

gfx::Display ScreenImpl::GetDisplayNearestPoint(const gfx::Point& point) const {
  return display_;
}

gfx::Display ScreenImpl::GetDisplayMatching(const gfx::Rect& match_rect) const {
  return display_;
}

gfx::Display ScreenImpl::GetPrimaryDisplay() const {
  return display_;
}

void ScreenImpl::AddObserver(gfx::DisplayObserver* observer) {
}

void ScreenImpl::RemoveObserver(gfx::DisplayObserver* observer) {
}

ScreenImpl::ScreenImpl(const gfx::Rect& screen_bounds) {
  static int64 synthesized_display_id = 2000;
  display_.set_id(synthesized_display_id++);
  display_.SetScaleAndBounds(1.0f, screen_bounds);
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
