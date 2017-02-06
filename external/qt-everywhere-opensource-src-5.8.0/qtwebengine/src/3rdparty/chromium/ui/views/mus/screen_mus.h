// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_SCREEN_MUS_H_
#define UI_VIEWS_MUS_SCREEN_MUS_H_

#include <vector>

#include "base/observer_list.h"
#include "base/run_loop.h"
#include "components/mus/public/interfaces/display.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/mus/display_list.h"
#include "ui/views/mus/mus_export.h"

namespace shell {
class Connector;
}

namespace views {

class ScreenMusDelegate;

// Screen implementation backed by mus::mojom::DisplayManager.
class VIEWS_MUS_EXPORT ScreenMus
    : public display::Screen,
      public NON_EXPORTED_BASE(mus::mojom::DisplayManagerObserver) {
 public:
  // |delegate| can be nullptr.
  explicit ScreenMus(ScreenMusDelegate* delegate);
  ~ScreenMus() override;

  void Init(shell::Connector* connector);

 private:
  // Invoked when a display changed in some weay, including being added.
  // If |is_primary| is true, |changed_display| is the primary display.
  void ProcessDisplayChanged(const display::Display& changed_display,
                             bool is_primary);

  // display::Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayNearestWindow(gfx::NativeView view) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  int GetNumDisplays() const override;
  std::vector<display::Display> GetAllDisplays() const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

  // mus::mojom::DisplayManager:
  void OnDisplays(
      mojo::Array<mus::mojom::DisplayPtr> transport_displays) override;
  void OnDisplaysChanged(
      mojo::Array<mus::mojom::DisplayPtr> transport_displays) override;
  void OnDisplayRemoved(int64_t id) override;

  ScreenMusDelegate* delegate_;  // Can be nullptr.
  mus::mojom::DisplayManagerPtr display_manager_;
  mojo::Binding<mus::mojom::DisplayManagerObserver>
      display_manager_observer_binding_;
  DisplayList display_list_;

  DISALLOW_COPY_AND_ASSIGN(ScreenMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_SCREEN_MUS_H_
