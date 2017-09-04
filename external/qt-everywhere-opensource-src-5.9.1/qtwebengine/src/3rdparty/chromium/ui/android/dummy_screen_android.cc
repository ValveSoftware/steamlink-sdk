// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/dummy_screen_android.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ui {

using display::Display;
using display::DisplayObserver;

// A Screen for Android unit tests that do not talk to Java. The class contains
// one primary display with default Display configuration and 256x512 dip size.
class DummyScreenAndroid : public display::Screen {
 public:
  DummyScreenAndroid() {}
  ~DummyScreenAndroid() override {}

  // Screen interface.

  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }

  bool IsWindowUnderCursor(gfx::NativeWindow window) override { return false; }

  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    return NULL;
  }

  int GetNumDisplays() const override { return 1; }

  std::vector<Display> GetAllDisplays() const override {
    return std::vector<Display>(1, GetPrimaryDisplay());
  }

  Display GetDisplayNearestWindow(gfx::NativeView view) const override {
    return GetPrimaryDisplay();
  }

  Display GetDisplayNearestPoint(const gfx::Point& point) const override {
    return GetPrimaryDisplay();
  }

  Display GetDisplayMatching(const gfx::Rect& match_rect) const override {
    return GetPrimaryDisplay();
  }

  Display GetPrimaryDisplay() const override {
    const int display_id = 0;
    const gfx::Rect bounds_in_dip(256, 512);
    return display::Display(display_id, bounds_in_dip);
  }

  void AddObserver(DisplayObserver* observer) override {}
  void RemoveObserver(DisplayObserver* observer) override {}
};

display::Screen* CreateDummyScreenAndroid() {
  return new DummyScreenAndroid;
}

}  // namespace ui
