// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This has to be before any other includes, else default is picked up.
// See base/logging for details on this.
#define NOTIMPLEMENTED_POLICY 5

#include "ui/display/screen_base.h"

#include "ui/display/display_finder.h"

namespace display {

ScreenBase::ScreenBase() {}

ScreenBase::~ScreenBase() {}

gfx::Point ScreenBase::GetCursorScreenPoint() {
  NOTIMPLEMENTED();
  return gfx::Point();
}

bool ScreenBase::IsWindowUnderCursor(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow ScreenBase::GetWindowAtScreenPoint(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return nullptr;
}

Display ScreenBase::GetPrimaryDisplay() const {
  auto iter = display_list_.GetPrimaryDisplayIterator();
  if (iter == display_list_.displays().end())
    return Display();  // Invalid display since we have no primary display.
  return *iter;
}

Display ScreenBase::GetDisplayNearestWindow(gfx::NativeView view) const {
  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

Display ScreenBase::GetDisplayNearestPoint(const gfx::Point& point) const {
  return *FindDisplayNearestPoint(display_list_.displays(), point);
}

int ScreenBase::GetNumDisplays() const {
  return static_cast<int>(display_list_.displays().size());
}

std::vector<Display> ScreenBase::GetAllDisplays() const {
  return display_list_.displays();
}

Display ScreenBase::GetDisplayMatching(const gfx::Rect& match_rect) const {
  const Display* match =
      FindDisplayWithBiggestIntersection(display_list_.displays(), match_rect);
  return match ? *match : GetPrimaryDisplay();
}

void ScreenBase::AddObserver(DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void ScreenBase::RemoveObserver(DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

void ScreenBase::ProcessDisplayChanged(const Display& changed_display,
                                       bool is_primary) {
  if (display_list_.FindDisplayById(changed_display.id()) ==
      display_list_.displays().end()) {
    display_list_.AddDisplay(changed_display,
                             is_primary ? DisplayList::Type::PRIMARY
                                        : DisplayList::Type::NOT_PRIMARY);
    return;
  }
  display_list_.UpdateDisplay(
      changed_display,
      is_primary ? DisplayList::Type::PRIMARY : DisplayList::Type::NOT_PRIMARY);
}

}  // namespace display
