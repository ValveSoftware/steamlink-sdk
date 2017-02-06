// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_PLATFORM_H_
#define UI_AURA_WINDOW_TREE_HOST_PLATFORM_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace aura {

// The unified WindowTreeHost implementation for platforms
// that implement PlatformWindow.
class AURA_EXPORT WindowTreeHostPlatform
    : public WindowTreeHost,
      public NON_EXPORTED_BASE(ui::PlatformWindowDelegate) {
 public:
  explicit WindowTreeHostPlatform(const gfx::Rect& bounds);
  ~WindowTreeHostPlatform() override;

  // WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBounds() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Point GetLocationOnNativeScreen() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void MoveCursorToNative(const gfx::Point& location) override;
  void OnCursorVisibilityChangedNative(bool show) override;

 protected:
  WindowTreeHostPlatform();
  void SetPlatformWindow(std::unique_ptr<ui::PlatformWindow> window);
  ui::PlatformWindow* platform_window() { return window_.get(); }

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override;
  void OnDamageRect(const gfx::Rect& damaged_region) override;
  void DispatchEvent(ui::Event* event) override;
  void OnCloseRequest() override;
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override;
  void OnLostCapture() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                    float device_pixel_ratio) override;
  void OnAcceleratedWidgetDestroyed() override;
  void OnActivationChanged(bool active) override;

 private:
  gfx::AcceleratedWidget widget_;
  std::unique_ptr<ui::PlatformWindow> window_;
  gfx::NativeCursor current_cursor_;
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostPlatform);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_PLATFORM_H_
