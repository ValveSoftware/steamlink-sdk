// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/aura/screen_mojo.h"

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect_conversions.h"

namespace mojo {

// static
ScreenMojo* ScreenMojo::Create() {
  return new ScreenMojo(gfx::Rect(0, 0, 800, 600));
}

ScreenMojo::~ScreenMojo() {
}

bool ScreenMojo::IsDIPEnabled() {
  NOTIMPLEMENTED();
  return true;
}

gfx::Point ScreenMojo::GetCursorScreenPoint() {
  NOTIMPLEMENTED();
  return gfx::Point();
}

gfx::NativeWindow ScreenMojo::GetWindowUnderCursor() {
  return GetWindowAtScreenPoint(GetCursorScreenPoint());
}

gfx::NativeWindow ScreenMojo::GetWindowAtScreenPoint(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return NULL;
}

int ScreenMojo::GetNumDisplays() const {
  return 1;
}

std::vector<gfx::Display> ScreenMojo::GetAllDisplays() const {
  return std::vector<gfx::Display>(1, display_);
}

gfx::Display ScreenMojo::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return display_;
}

gfx::Display ScreenMojo::GetDisplayNearestPoint(const gfx::Point& point) const {
  return display_;
}

gfx::Display ScreenMojo::GetDisplayMatching(const gfx::Rect& match_rect) const {
  return display_;
}

gfx::Display ScreenMojo::GetPrimaryDisplay() const {
  return display_;
}

void ScreenMojo::AddObserver(gfx::DisplayObserver* observer) {
}

void ScreenMojo::RemoveObserver(gfx::DisplayObserver* observer) {
}

ScreenMojo::ScreenMojo(const gfx::Rect& screen_bounds) {
  static int64 synthesized_display_id = 2000;
  display_.set_id(synthesized_display_id++);
  display_.SetScaleAndBounds(1.0f, screen_bounds);
}

}  // namespace mojo
