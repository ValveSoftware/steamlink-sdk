// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/ozone_platform_gbm.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <gbm.h>
#include <stdlib.h>
#include <xf86drm.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_platform_support.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_message_proxy.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_native_display_delegate.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"
#include "ui/ozone/platform/drm/mus_thread_proxy.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"

#if defined(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

namespace ui {

namespace {

class GlApiLoader {
 public:
  GlApiLoader()
      : glapi_lib_(dlopen("libglapi.so.0", RTLD_LAZY | RTLD_GLOBAL)) {}

  ~GlApiLoader() {
    if (glapi_lib_)
      dlclose(glapi_lib_);
  }

 private:
  // HACK: gbm drivers have broken linkage. The Mesa DRI driver references
  // symbols in the libglapi library however it does not explicitly link against
  // it. That caused linkage errors when running an application that does not
  // explicitly link against libglapi.
  void* glapi_lib_;

  DISALLOW_COPY_AND_ASSIGN(GlApiLoader);
};

// Returns true if we should operate in Mus mode.
// TODO(rjkroege): Create an explicit "single process ozone mode" that can be
// used for tests after mus+ash team finishes splitting mus.
bool RunningInsideMus() {
  bool has_channel_handle = base::CommandLine::ForCurrentProcess()->HasSwitch(
      "mojo-platform-channel-handle");
  return has_channel_handle;
}

class OzonePlatformGbm : public OzonePlatform {
 public:
  OzonePlatformGbm() {}
  ~OzonePlatformGbm() override {}

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
    return event_factory_ozone_->input_controller();
  }
  GpuPlatformSupport* GetGpuPlatformSupport() override {
    return gpu_platform_support_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return event_factory_ozone_->CreateSystemInputInjector();
  }
  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    GpuThreadAdapter* adapter = gpu_platform_support_host_.get();
    if (RunningInsideMus()) {
      DCHECK(drm_thread_) << "drm_thread_ should exist (and be running) here.";
      adapter = mus_thread_proxy_.get();
    }

    std::unique_ptr<DrmWindowHost> platform_window(new DrmWindowHost(
        delegate, bounds, adapter, event_factory_ozone_.get(), cursor_.get(),
        window_manager_.get(), display_manager_.get(), overlay_manager_.get()));
    platform_window->Initialize();
    return std::move(platform_window);
  }
  std::unique_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return base::WrapUnique(
        new DrmNativeDisplayDelegate(display_manager_.get()));
  }
  void InitializeUI() override {
    device_manager_ = CreateDeviceManager();
    window_manager_.reset(new DrmWindowHostManager());
    cursor_.reset(new DrmCursor(window_manager_.get()));
#if defined(USE_XKBCOMMON)
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(base::WrapUnique(
        new XkbKeyboardLayoutEngine(xkb_evdev_code_converter_)));
#else
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        base::WrapUnique(new StubKeyboardLayoutEngine()));
#endif
    event_factory_ozone_.reset(new EventFactoryEvdev(
        cursor_.get(), device_manager_.get(),
        KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()));

    GpuThreadAdapter* adapter;
    if (RunningInsideMus()) {
      gl_api_loader_.reset(new GlApiLoader());
      mus_thread_proxy_.reset(new MusThreadProxy());
      adapter = mus_thread_proxy_.get();
      cursor_->SetDrmCursorProxy(mus_thread_proxy_.get());
    } else {
      gpu_platform_support_host_.reset(
          new DrmGpuPlatformSupportHost(cursor_.get()));
      adapter = gpu_platform_support_host_.get();
    }

    display_manager_.reset(
        new DrmDisplayHostManager(adapter, device_manager_.get(),
                                  event_factory_ozone_->input_controller()));
    cursor_factory_ozone_.reset(new BitmapCursorFactoryOzone);
    overlay_manager_.reset(
        new DrmOverlayManager(adapter, window_manager_.get()));

    if (RunningInsideMus()) {
      mus_thread_proxy_->ProvideManagers(display_manager_.get(),
                                         overlay_manager_.get());
    }
  }

  void InitializeGPU() override {
    InterThreadMessagingProxy* itmp;
    if (RunningInsideMus()) {
      DCHECK(mus_thread_proxy_);
      itmp = mus_thread_proxy_.get();
    } else {
      gl_api_loader_.reset(new GlApiLoader());
      scoped_refptr<DrmThreadMessageProxy> message_proxy(
          new DrmThreadMessageProxy());
      itmp = message_proxy.get();
      gpu_platform_support_.reset(new DrmGpuPlatformSupport(message_proxy));
    }

    // NOTE: Can't start the thread here since this is called before sandbox
    // initialization in multi-process Chrome. In mus, we start the DRM thread.
    drm_thread_.reset(new DrmThreadProxy());
    drm_thread_->BindThreadIntoMessagingProxy(itmp);

    surface_factory_.reset(new GbmSurfaceFactory(drm_thread_.get()));
    if (RunningInsideMus()) {
      mus_thread_proxy_->StartDrmThread();
    }
  }

 private:
  // Objects in the GPU process.
  std::unique_ptr<DrmThreadProxy> drm_thread_;
  std::unique_ptr<GlApiLoader> gl_api_loader_;
  std::unique_ptr<GbmSurfaceFactory> surface_factory_;
  std::unique_ptr<DrmGpuPlatformSupport> gpu_platform_support_;

  // Objects in the Browser process.
  std::unique_ptr<DeviceManager> device_manager_;
  std::unique_ptr<BitmapCursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<DrmWindowHostManager> window_manager_;
  std::unique_ptr<DrmCursor> cursor_;
  std::unique_ptr<EventFactoryEvdev> event_factory_ozone_;
  std::unique_ptr<DrmGpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<DrmDisplayHostManager> display_manager_;
  std::unique_ptr<DrmOverlayManager> overlay_manager_;

  // Bridges the DRM, GPU and main threads in mus.
  std::unique_ptr<MusThreadProxy> mus_thread_proxy_;

#if defined(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformGbm);
};

}  // namespace

OzonePlatform* CreateOzonePlatformGbm() {
  return new OzonePlatformGbm;
}

}  // namespace ui
