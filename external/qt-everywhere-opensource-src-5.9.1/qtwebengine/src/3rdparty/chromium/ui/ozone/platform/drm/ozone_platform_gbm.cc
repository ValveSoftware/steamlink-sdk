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
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/cursor_proxy_mojo.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
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

class OzonePlatformGbm
    : public OzonePlatform,
      public service_manager::InterfaceFactory<ozone::mojom::DeviceCursor> {
 public:
  OzonePlatformGbm() : using_mojo_(false), single_process_(false) {}
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
  IPC::MessageFilter* GetGpuMessageFilter() override {
    return gpu_message_filter_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return event_factory_ozone_->CreateSystemInputInjector();
  }
  void AddInterfaces(service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<ozone::mojom::DeviceCursor>(this);
  }
  // service_manager::InterfaceFactory<ozone::mojom::DeviceCursor>:
  void Create(const service_manager::Identity& remote_identity,
              ozone::mojom::DeviceCursorRequest request) override {
    if (drm_thread_proxy_)
      drm_thread_proxy_->AddBinding(std::move(request));
    else
      pending_cursor_requests_.push_back(std::move(request));
  }
  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    GpuThreadAdapter* adapter = gpu_platform_support_host_.get();
    if (using_mojo_ || single_process_) {
      DCHECK(drm_thread_proxy_)
          << "drm_thread_proxy_ should exist (and be running) here.";
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
    return base::MakeUnique<DrmNativeDisplayDelegate>(display_manager_.get());
  }
  void InitializeUI() override {
    InitParams default_params;
    InitializeUI(default_params);
  }
  void InitializeUI(const InitParams& args) override {
    // Ozone drm can operate in three modes configured at runtime:
    //   1. legacy mode where browser and gpu components communicate
    //      via param traits IPC.
    //   2. single-process mode where browser and gpu components
    //      communicate via PostTask.
    //   3. mojo mode where browser and gpu components communicate
    //      via mojo IPC.
    // Currently, mojo mode uses mojo in a single process but this is
    // an interim implementation detail that will be eliminated in a
    // future CL.
    single_process_ = args.single_process;
    using_mojo_ = args.connector != nullptr;
    DCHECK(!(using_mojo_ && single_process_));

    device_manager_ = CreateDeviceManager();
    window_manager_.reset(new DrmWindowHostManager());
    cursor_.reset(new DrmCursor(window_manager_.get()));
#if defined(USE_XKBCOMMON)
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        base::MakeUnique<XkbKeyboardLayoutEngine>(xkb_evdev_code_converter_));
#else
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        base::MakeUnique<StubKeyboardLayoutEngine>());
#endif

    event_factory_ozone_.reset(new EventFactoryEvdev(
        cursor_.get(), device_manager_.get(),
        KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()));

    GpuThreadAdapter* adapter;

    // TODO(rjkroege): Once mus is split, only do this for single_process.
    if (single_process_ || using_mojo_)
      gl_api_loader_.reset(new GlApiLoader());

    if (using_mojo_) {
      DCHECK(args.connector);
      mus_thread_proxy_.reset(new MusThreadProxy());
      adapter = mus_thread_proxy_.get();
      cursor_->SetDrmCursorProxy(new CursorProxyMojo(args.connector));
    } else if (single_process_) {
      mus_thread_proxy_.reset(new MusThreadProxy());
      adapter = mus_thread_proxy_.get();
      cursor_->SetDrmCursorProxy(
          new CursorProxyThread(mus_thread_proxy_.get()));
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

    if (using_mojo_ || single_process_) {
      mus_thread_proxy_->ProvideManagers(display_manager_.get(),
                                         overlay_manager_.get());
    }
  }
  void InitializeGPU() override {
    InitParams default_params;
    InitializeGPU(default_params);
  }
  void InitializeGPU(const InitParams& args) override {
    // TODO(rjkroege): services/ui should initialize this with a connector.
    // However, in-progress refactorings in services/ui make it difficult to
    // require this at present. Set using_mojo_ like below once this is
    // complete.
    // using_mojo_ = args.connector != nullptr;

    InterThreadMessagingProxy* itmp;
    if (using_mojo_ || single_process_) {
      itmp = mus_thread_proxy_.get();
    } else {
      gl_api_loader_.reset(new GlApiLoader());
      scoped_refptr<DrmThreadMessageProxy> message_proxy(
          new DrmThreadMessageProxy());
      itmp = message_proxy.get();
      gpu_message_filter_ = std::move(message_proxy);
    }

    // NOTE: Can't start the thread here since this is called before sandbox
    // initialization in multi-process Chrome. In mus, we start the DRM thread.
    drm_thread_proxy_.reset(new DrmThreadProxy());
    drm_thread_proxy_->BindThreadIntoMessagingProxy(itmp);

    surface_factory_.reset(new GbmSurfaceFactory(drm_thread_proxy_.get()));
    if (using_mojo_ || single_process_) {
      mus_thread_proxy_->StartDrmThread();
    }
    for (auto& request : pending_cursor_requests_)
      drm_thread_proxy_->AddBinding(std::move(request));
    pending_cursor_requests_.clear();
  }

 private:
  bool using_mojo_;
  bool single_process_;

  // Objects in the GPU process.
  std::unique_ptr<DrmThreadProxy> drm_thread_proxy_;
  std::unique_ptr<GlApiLoader> gl_api_loader_;
  std::unique_ptr<GbmSurfaceFactory> surface_factory_;
  scoped_refptr<IPC::MessageFilter> gpu_message_filter_;
  // TODO(sad): Once the mus gpu process split happens, this can go away.
  std::vector<ozone::mojom::DeviceCursorRequest> pending_cursor_requests_;

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
