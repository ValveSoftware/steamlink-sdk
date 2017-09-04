// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/ozone_platform_wayland.h"

#include "base/memory/ptr_util.h"
#include "ui/display/fake_display_delegate.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"

namespace ui {

namespace {

class OzonePlatformWayland : public OzonePlatform {
 public:
  OzonePlatformWayland() {}
  ~OzonePlatformWayland() override {}

  // OzonePlatform
  SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }

  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_.get();
  }

  InputController* GetInputController() override {
    return input_controller_.get();
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    auto window =
        base::MakeUnique<WaylandWindow>(delegate, connection_.get(), bounds);
    if (!window->Initialize())
      return nullptr;
    return std::move(window);
  }

  std::unique_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return base::MakeUnique<display::FakeDisplayDelegate>();
  }

  void InitializeUI() override {
    InitParams default_params;
    InitializeUI(default_params);
  }

  void InitializeUI(const InitParams& args) override {
    connection_.reset(new WaylandConnection);
    if (!connection_->Initialize())
      LOG(FATAL) << "Failed to initialize Wayland platform";

    cursor_factory_.reset(new CursorFactoryOzone);
    overlay_manager_.reset(new StubOverlayManager);
    input_controller_ = CreateStubInputController();
    surface_factory_.reset(new WaylandSurfaceFactory(connection_.get()));
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU() override {
    InitParams default_params;
    InitializeGPU(default_params);
  }

  void InitializeGPU(const InitParams& args) override {
    // TODO(fwang): args.single_process parameter should be checked here; make
    // sure callers pass in the proper value. Once it happens, the check whether
    // surface factory was set in the same process by a previous InitializeUI
    // call becomes unneeded.
    if (!surface_factory_) {
      // TODO(fwang): Separate processes can not share a Wayland connection
      // and so the current implementations of GLOzoneEGLWayland and
      // WaylandCanvasSurface may only work when UI and GPU live in the same
      // process. GetSurfaceFactoryOzone() must be non-null so a dummy instance
      // of WaylandSurfaceFactory is needed to make the GPU initialization
      // gracefully fail.
      surface_factory_.reset(new WaylandSurfaceFactory(nullptr));
    }
  }

 private:
  std::unique_ptr<WaylandConnection> connection_;
  std::unique_ptr<WaylandSurfaceFactory> surface_factory_;
  std::unique_ptr<CursorFactoryOzone> cursor_factory_;
  std::unique_ptr<StubOverlayManager> overlay_manager_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformWayland);
};

}  // namespace

OzonePlatform* CreateOzonePlatformWayland() {
  return new OzonePlatformWayland;
}

}  // namespace ui
