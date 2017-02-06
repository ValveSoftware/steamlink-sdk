// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/ozone_platform_caca.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/no/no_keyboard_layout_engine.h"
#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/caca/caca_event_source.h"
#include "ui/ozone/platform/caca/caca_window.h"
#include "ui/ozone/platform/caca/caca_window_manager.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"

namespace ui {

namespace {

class OzonePlatformCaca : public OzonePlatform {
 public:
  OzonePlatformCaca() {}
  ~OzonePlatformCaca() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return window_manager_.get();
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
    std::unique_ptr<CacaWindow> caca_window(new CacaWindow(
        delegate, window_manager_.get(), event_source_.get(), bounds));
    if (!caca_window->Initialize())
      return nullptr;
    return std::move(caca_window);
  }
  std::unique_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return base::WrapUnique(new NativeDisplayDelegateOzone());
  }

  void InitializeUI() override {
    window_manager_.reset(new CacaWindowManager);
    overlay_manager_.reset(new StubOverlayManager());
    event_source_.reset(new CacaEventSource());
    cursor_factory_ozone_.reset(new CursorFactoryOzone());
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
    input_controller_ = CreateStubInputController();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        base::WrapUnique(new NoKeyboardLayoutEngine()));
  }

  void InitializeGPU() override {
    gpu_platform_support_.reset(CreateStubGpuPlatformSupport());
  }

 private:
  std::unique_ptr<CacaWindowManager> window_manager_;
  std::unique_ptr<CacaEventSource> event_source_;
  std::unique_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<GpuPlatformSupport> gpu_platform_support_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformCaca);
};

}  // namespace

OzonePlatform* CreateOzonePlatformCaca() {
  return new OzonePlatformCaca;
}

}  // namespace ui
