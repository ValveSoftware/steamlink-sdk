// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/ozone_platform_cast.h"

#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromecast/chromecast_features.h"
#include "chromecast/public/cast_egl_platform.h"
#include "chromecast/public/cast_egl_platform_shlib.h"
#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/platform/cast/gpu_platform_support_cast.h"
#include "ui/ozone/platform/cast/overlay_manager_cast.h"
#include "ui/ozone/platform/cast/platform_window_cast.h"
#include "ui/ozone/platform/cast/surface_factory_cast.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"

using chromecast::CastEglPlatform;

namespace ui {
namespace {

base::LazyInstance<std::unique_ptr<GpuPlatformSupport>> g_gpu_platform_support =
    LAZY_INSTANCE_INITIALIZER;

// Ozone platform implementation for Cast.  Implements functionality
// common to all Cast implementations:
//  - Always one window with window size equal to display size
//  - No input, cursor support
//  - Relinquish GPU resources flow for switching to external applications
// Meanwhile, platform-specific implementation details are abstracted out
// to the CastEglPlatform interface.
class OzonePlatformCast : public OzonePlatform {
 public:
  explicit OzonePlatformCast(std::unique_ptr<CastEglPlatform> egl_platform)
      : egl_platform_(std::move(egl_platform)) {}
  ~OzonePlatformCast() override {}

  // OzonePlatform implementation:
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
  GpuPlatformSupport* GetGpuPlatformSupport() override {
    return g_gpu_platform_support.Get().get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;  // no input injection support
  }
  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    return base::WrapUnique<PlatformWindow>(
        new PlatformWindowCast(delegate, bounds));
  }
  std::unique_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return base::WrapUnique(new NativeDisplayDelegateOzone());
  }

  void InitializeUI() override {
    overlay_manager_.reset(new OverlayManagerCast());
    cursor_factory_.reset(new CursorFactoryOzone());
    input_controller_ = CreateStubInputController();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

    // Enable dummy software rendering support if GPU process disabled
    // or if we're an audio-only build.
    // Note: switch is kDisableGpu from content/public/common/content_switches.h
    bool enable_dummy_software_rendering = true;
#if !BUILDFLAG(DISABLE_DISPLAY)
    enable_dummy_software_rendering =
        base::CommandLine::ForCurrentProcess()->HasSwitch("disable-gpu");
#endif  // BUILDFLAG(DISABLE_DISPLAY)

    if (enable_dummy_software_rendering)
      surface_factory_.reset(new SurfaceFactoryCast());
  }
  void InitializeGPU() override {
    surface_factory_.reset(new SurfaceFactoryCast(std::move(egl_platform_)));
    g_gpu_platform_support.Get() =
        base::WrapUnique(new GpuPlatformSupportCast(surface_factory_.get()));
  }

 private:
  std::unique_ptr<CastEglPlatform> egl_platform_;
  std::unique_ptr<SurfaceFactoryCast> surface_factory_;
  std::unique_ptr<CursorFactoryOzone> cursor_factory_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformCast);
};

}  // namespace

OzonePlatform* CreateOzonePlatformCast() {
  const std::vector<std::string>& argv =
      base::CommandLine::ForCurrentProcess()->argv();
  std::unique_ptr<chromecast::CastEglPlatform> platform(
      chromecast::CastEglPlatformShlib::Create(argv));
  return new OzonePlatformCast(std::move(platform));
}

}  // namespace ui
