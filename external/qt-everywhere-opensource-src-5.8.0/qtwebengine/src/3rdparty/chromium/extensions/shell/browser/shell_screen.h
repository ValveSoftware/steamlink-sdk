// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_SCREEN_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_SCREEN_H_

#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace aura {
class WindowTreeHost;
}

namespace gfx {
class Size;
}

namespace extensions {

// A minimal Aura implementation of a screen. Scale factor is locked at 1.0.
// When running on a Linux desktop resizing the main window resizes the screen.
class ShellScreen : public display::Screen, public aura::WindowObserver {
 public:
  // Creates a screen occupying |size| physical pixels.
  explicit ShellScreen(const gfx::Size& size);
  ~ShellScreen() override;

  // Caller owns the returned object.
  aura::WindowTreeHost* CreateHostForPrimaryDisplay();

  // WindowObserver overrides:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(aura::Window* window) override;

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
  aura::WindowTreeHost* host_;  // Not owned.
  display::Display display_;

  DISALLOW_COPY_AND_ASSIGN(ShellScreen);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_SCREEN_H_
