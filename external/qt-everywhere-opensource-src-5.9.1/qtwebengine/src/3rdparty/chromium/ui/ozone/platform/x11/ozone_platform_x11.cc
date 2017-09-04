// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/ozone_platform_x11.h"

#include <X11/Xlib.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/display/fake_display_delegate.h"
#include "ui/events/platform/x11/x11_event_source_libevent.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/x11/x11_cursor_factory_ozone.h"
#include "ui/ozone/platform/x11/x11_surface_factory.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/x11/x11_window_manager_ozone.h"
#include "ui/platform_window/x11/x11_window_ozone.h"

namespace ui {

namespace {

// Returns true if we should operate in Mus mode.
bool RunningInsideMus() {
  bool has_channel_handle = base::CommandLine::ForCurrentProcess()->HasSwitch(
      "mojo-platform-channel-handle");
  return has_channel_handle;
}

// Singleton OzonePlatform implementation for Linux X11 platform.
class OzonePlatformX11 : public OzonePlatform {
 public:
  OzonePlatformX11() {
    // If we're running in Mus mode both the UI and GPU components of Ozone will
    // be running in the same process. Enable X11 concurrent thread support.
    if (RunningInsideMus())
      XInitThreads();
  }

  ~OzonePlatformX11() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_ozone_.get();
  }

  ui::OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  InputController* GetInputController() override {
    return input_controller_.get();
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    std::unique_ptr<X11WindowOzone> window = base::MakeUnique<X11WindowOzone>(
        event_source_.get(), window_manager_.get(), delegate);
    window->SetBounds(bounds);
    window->Create();
    window->SetTitle(base::ASCIIToUTF16("Ozone X11"));
    return std::move(window);
  }

  std::unique_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return base::MakeUnique<display::FakeDisplayDelegate>();
  }

  void InitializeUI() override {
    window_manager_.reset(new X11WindowManagerOzone);
    if (!event_source_)
      event_source_.reset(new X11EventSourceLibevent(gfx::GetXDisplay()));
    overlay_manager_.reset(new StubOverlayManager());
    input_controller_ = CreateStubInputController();
    cursor_factory_ozone_.reset(new X11CursorFactoryOzone());
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU() override {
    if (!event_source_)
      event_source_.reset(new X11EventSourceLibevent(gfx::GetXDisplay()));
    surface_factory_ozone_.reset(new X11SurfaceFactory());
  }

 private:
  // Objects in the browser process.
  std::unique_ptr<X11WindowManagerOzone> window_manager_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<X11CursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;

  // Objects in the GPU process.
  std::unique_ptr<X11SurfaceFactory> surface_factory_ozone_;

  // Objects in both browser and GPU process.
  std::unique_ptr<X11EventSourceLibevent> event_source_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformX11);
};

}  // namespace

OzonePlatform* CreateOzonePlatformX11() {
  return new OzonePlatformX11;
}

}  // namespace ui
