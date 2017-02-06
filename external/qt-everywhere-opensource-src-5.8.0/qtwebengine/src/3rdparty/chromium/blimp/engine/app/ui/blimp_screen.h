// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_UI_BLIMP_SCREEN_H_
#define BLIMP_ENGINE_APP_UI_BLIMP_SCREEN_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace aura {
class WindowTreeHost;
}

namespace blimp {
namespace engine {

// Presents the client's single screen.
class BlimpScreen : public display::Screen {
 public:
  BlimpScreen();
  ~BlimpScreen() override;

  void set_window_tree_host(aura::WindowTreeHost* window_tree_host) {
    window_tree_host_ = window_tree_host;
  }

  // Updates the size reported by the primary display.
  void UpdateDisplayScaleAndSize(float scale, const gfx::Size& size);

  // display::Screen implementation.
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
  aura::WindowTreeHost* window_tree_host_;
  display::Display display_;
  base::ObserverList<display::DisplayObserver> observers_;
  DISALLOW_COPY_AND_ASSIGN(BlimpScreen);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_UI_BLIMP_SCREEN_H_
