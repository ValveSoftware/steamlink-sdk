// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_SCREEN_H_
#define CHROMECAST_GRAPHICS_CAST_SCREEN_H_

#include "base/macros.h"
#include "chromecast/public/graphics_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromecast {
namespace shell {
class CastBrowserMainParts;
}  // namespace shell

// CastScreen is Chromecast's own minimal implementation of display::Screen.
// Right now, it almost exactly duplicates the behavior of aura's TestScreen
// class for necessary methods. The instantiation of CastScreen occurs in
// CastBrowserMainParts, where its ownership is assigned to CastBrowserProcess.
// To then subsequently access CastScreen, see CastBrowerProcess.
class CastScreen : public display::Screen {
 public:
  ~CastScreen() override;

  // display::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  std::vector<display::Display> GetAllDisplays() const override;
  display::Display GetDisplayNearestWindow(gfx::NativeView view) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  display::Display GetPrimaryDisplay() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

 private:
  CastScreen();

  display::Display display_;

  friend class shell::CastBrowserMainParts;

  DISALLOW_COPY_AND_ASSIGN(CastScreen);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_SCREEN_H_
