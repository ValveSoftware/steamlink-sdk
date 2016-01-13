// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/ozone_platform_dri.h"

#include "base/at_exit.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/platform/dri/cursor_factory_evdev_dri.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/scanout_surface.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/platform/dri/virtual_terminal_manager.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/common/chromeos/touchscreen_device_manager_ozone.h"
#include "ui/ozone/platform/dri/chromeos/native_display_delegate_dri.h"
#endif

namespace ui {

namespace {

const char kDefaultGraphicsCardPath[] = "/dev/dri/card0";

class DriSurfaceGenerator : public ScanoutSurfaceGenerator {
 public:
  DriSurfaceGenerator(DriWrapper* dri) : dri_(dri) {}
  virtual ~DriSurfaceGenerator() {}

  virtual ScanoutSurface* Create(const gfx::Size& size) OVERRIDE {
    return new DriSurface(dri_, size);
  }

 private:
  DriWrapper* dri_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(DriSurfaceGenerator);
};

// OzonePlatform for Linux DRI (Direct Rendering Infrastructure)
//
// This platform is Linux without any display server (no X, wayland, or
// anything). This means chrome alone owns the display and input devices.
class OzonePlatformDri : public OzonePlatform {
 public:
  OzonePlatformDri()
      : vt_manager_(new VirtualTerminalManager()),
        dri_(new DriWrapper(kDefaultGraphicsCardPath)),
        surface_generator_(new DriSurfaceGenerator(dri_.get())),
        screen_manager_(new ScreenManager(dri_.get(),
                                          surface_generator_.get())),
        device_manager_(CreateDeviceManager()) {
    base::AtExitManager::RegisterTask(
        base::Bind(&base::DeletePointer<OzonePlatformDri>, this));
  }
  virtual ~OzonePlatformDri() {}

  // OzonePlatform:
  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE {
    return surface_factory_ozone_.get();
  }
  virtual EventFactoryOzone* GetEventFactoryOzone() OVERRIDE {
    return event_factory_ozone_.get();
  }
  virtual CursorFactoryOzone* GetCursorFactoryOzone() OVERRIDE {
    return cursor_factory_ozone_.get();
  }
  virtual GpuPlatformSupport* GetGpuPlatformSupport() OVERRIDE {
    return NULL;  // no GPU support
  }
  virtual GpuPlatformSupportHost* GetGpuPlatformSupportHost() OVERRIDE {
    return NULL;  // no GPU support
  }
#if defined(OS_CHROMEOS)
  virtual scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      OVERRIDE {
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateDri(
        dri_.get(), screen_manager_.get(), device_manager_.get()));
  }
  virtual scoped_ptr<TouchscreenDeviceManager>
      CreateTouchscreenDeviceManager() OVERRIDE {
    return scoped_ptr<TouchscreenDeviceManager>(
        new TouchscreenDeviceManagerOzone());
  }
#endif
  virtual void InitializeUI() OVERRIDE {
    surface_factory_ozone_.reset(
        new DriSurfaceFactory(dri_.get(), screen_manager_.get()));
    cursor_factory_ozone_.reset(
        new CursorFactoryEvdevDri(surface_factory_ozone_.get()));
    event_factory_ozone_.reset(new EventFactoryEvdev(
        cursor_factory_ozone_.get(), device_manager_.get()));
  }

  virtual void InitializeGPU() OVERRIDE {}

 private:
  scoped_ptr<VirtualTerminalManager> vt_manager_;
  scoped_ptr<DriWrapper> dri_;
  scoped_ptr<DriSurfaceGenerator> surface_generator_;
  scoped_ptr<ScreenManager> screen_manager_;
  scoped_ptr<DeviceManager> device_manager_;

  scoped_ptr<DriSurfaceFactory> surface_factory_ozone_;
  scoped_ptr<CursorFactoryEvdevDri> cursor_factory_ozone_;
  scoped_ptr<EventFactoryEvdev> event_factory_ozone_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformDri);
};

}  // namespace

OzonePlatform* CreateOzonePlatformDri() { return new OzonePlatformDri; }

}  // namespace ui
