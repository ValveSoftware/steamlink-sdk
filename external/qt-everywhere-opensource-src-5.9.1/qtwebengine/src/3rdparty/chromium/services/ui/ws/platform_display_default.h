// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_PLATFORM_DISPLAY_DEFAULT_H_
#define SERVICES_UI_WS_PLATFORM_DISPLAY_DEFAULT_H_

#include <memory>

#include "base/macros.h"
#include "services/ui/display/viewport_metrics.h"
#include "services/ui/ws/frame_generator.h"
#include "services/ui/ws/frame_generator_delegate.h"
#include "services/ui/ws/platform_display.h"
#include "services/ui/ws/platform_display_delegate.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

class CursorLoader;
class PlatformWindow;

namespace ws {

// PlatformDisplay implementation that connects to a PlatformWindow and
// FrameGenerator for Chrome OS.
class PlatformDisplayDefault : public PlatformDisplay,
                               public ui::PlatformWindowDelegate,
                               private FrameGeneratorDelegate {
 public:
  explicit PlatformDisplayDefault(const PlatformDisplayInitParams& init_params);
  ~PlatformDisplayDefault() override;

  // PlatformDisplay:
  void Init(PlatformDisplayDelegate* delegate) override;
  int64_t GetId() const override;
  void SetViewportSize(const gfx::Size& size) override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetCursorById(mojom::Cursor cursor) override;
  void UpdateTextInputState(const ui::TextInputState& state) override;
  void SetImeVisibility(bool visible) override;
  gfx::Rect GetBounds() const override;
  bool UpdateViewportMetrics(const display::ViewportMetrics& metrics) override;
  const display::ViewportMetrics& GetViewportMetrics() const override;
  bool IsPrimaryDisplay() const override;
  void OnGpuChannelEstablished(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel) override;

 private:
  // Update the root_location of located events to be relative to the origin
  // of this display. For example, if the origin of this display is (1800, 0)
  // and the location of the event is (100, 200) then the root_location will be
  // updated to be (1900, 200).
  // TODO(riajiang): This is totally broken with HDPI.
  void UpdateEventRootLocation(ui::LocatedEvent* event);

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override;
  void OnDamageRect(const gfx::Rect& damaged_region) override;
  void DispatchEvent(ui::Event* event) override;
  void OnCloseRequest() override;
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override;
  void OnLostCapture() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                    float device_scale_factor) override;
  void OnAcceleratedWidgetDestroyed() override;
  void OnActivationChanged(bool active) override;

  // FrameGeneratorDelegate:
  bool IsInHighContrastMode() override;

  int64_t display_id_;

#if !defined(OS_ANDROID)
  std::unique_ptr<ui::CursorLoader> cursor_loader_;
#endif

  PlatformDisplayDelegate* delegate_ = nullptr;
  std::unique_ptr<FrameGenerator> frame_generator_;

  display::ViewportMetrics metrics_;
  std::unique_ptr<ui::PlatformWindow> platform_window_;

  DISALLOW_COPY_AND_ASSIGN(PlatformDisplayDefault);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_PLATFORM_DISPLAY_DEFAULT_H_
