// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_MAC_H_
#define UI_AURA_WINDOW_TREE_HOST_MAC_H_

#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class MouseEvent;
}

namespace aura {

class AURA_EXPORT WindowTreeHostMac : public WindowTreeHost {
 public:
  explicit WindowTreeHostMac(const gfx::Rect& bounds);
  ~WindowTreeHostMac() override;

 private:
  // WindowTreeHost Overrides.
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void Show() override;
  void Hide() override;
  void ToggleFullScreen() override;
  gfx::Rect GetBounds() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Insets GetInsets() const override;
  void SetInsets(const gfx::Insets& insets) override;
  gfx::Point GetLocationOnNativeScreen() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool ConfineCursorToRootWindow() override;
  void UnConfineCursor() override;
  void SetCursorNative(gfx::NativeCursor cursor_type) override;
  void MoveCursorToNative(const gfx::Point& location) override;
  void OnCursorVisibilityChangedNative(bool show) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;

 private:
  base::scoped_nsobject<NSWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMac);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_MAC_H_
