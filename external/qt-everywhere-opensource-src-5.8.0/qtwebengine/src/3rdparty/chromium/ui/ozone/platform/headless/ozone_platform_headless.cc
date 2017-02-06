// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/ozone_platform_headless.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/headless/headless_surface_factory.h"
#include "ui/ozone/platform/headless/headless_window.h"
#include "ui/ozone/platform/headless/headless_window_manager.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/system_input_injector.h"

namespace ui {

namespace {

// A headless implementation of PlatformEventSource that we can instantiate to
// make
// sure that the PlatformEventSource has an instance while in unit tests.
class HeadlessPlatformEventSource : public ui::PlatformEventSource {
 public:
  HeadlessPlatformEventSource() {}
  ~HeadlessPlatformEventSource() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessPlatformEventSource);
};

// OzonePlatform for headless mode
class OzonePlatformHeadless : public OzonePlatform {
 public:
  OzonePlatformHeadless(const base::FilePath& dump_file)
      : file_path_(dump_file) {}
  ~OzonePlatformHeadless() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }
  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }
  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }
  InputController* GetInputController() override {
    return input_controller_.get();
  }
  GpuPlatformSupport* GetGpuPlatformSupport() override {
    return gpu_platform_support_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;  // no input injection support.
  }
  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    return base::WrapUnique<PlatformWindow>(
        new HeadlessWindow(delegate, window_manager_.get(), bounds));
  }
  std::unique_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return base::WrapUnique(new NativeDisplayDelegateOzone());
  }

  void InitializeUI() override {
    window_manager_.reset(new HeadlessWindowManager(file_path_));
    window_manager_->Initialize();
    surface_factory_.reset(new HeadlessSurfaceFactory(window_manager_.get()));
    // This unbreaks tests that create their own.
    if (!PlatformEventSource::GetInstance())
      platform_event_source_.reset(new HeadlessPlatformEventSource);
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        base::WrapUnique(new StubKeyboardLayoutEngine()));

    overlay_manager_.reset(new StubOverlayManager());
    input_controller_ = CreateStubInputController();
    cursor_factory_ozone_.reset(new BitmapCursorFactoryOzone);
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU() override {
    if (!surface_factory_)
      surface_factory_.reset(new HeadlessSurfaceFactory());
    gpu_platform_support_.reset(CreateStubGpuPlatformSupport());
  }

 private:
  std::unique_ptr<HeadlessWindowManager> window_manager_;
  std::unique_ptr<HeadlessSurfaceFactory> surface_factory_;
  std::unique_ptr<PlatformEventSource> platform_event_source_;
  std::unique_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupport> gpu_platform_support_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformHeadless);
};

}  // namespace

OzonePlatform* CreateOzonePlatformHeadless() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  base::FilePath location;
  if (cmd->HasSwitch(switches::kOzoneDumpFile))
    location = cmd->GetSwitchValuePath(switches::kOzoneDumpFile);
  return new OzonePlatformHeadless(location);
}

}  // namespace ui
