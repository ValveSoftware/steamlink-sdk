// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/ozone_platform_caca.h"

#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/platform/caca/caca_connection.h"
#include "ui/ozone/platform/caca/caca_event_factory.h"
#include "ui/ozone/platform/caca/caca_surface_factory.h"
#include "ui/ozone/public/cursor_factory_ozone.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/common/chromeos/native_display_delegate_ozone.h"
#include "ui/ozone/common/chromeos/touchscreen_device_manager_ozone.h"
#endif

namespace ui {

namespace {

class OzonePlatformCaca : public OzonePlatform {
 public:
  OzonePlatformCaca() {}
  virtual ~OzonePlatformCaca() {}

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
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateOzone());
  }
  virtual scoped_ptr<TouchscreenDeviceManager>
      CreateTouchscreenDeviceManager() OVERRIDE {
    return scoped_ptr<TouchscreenDeviceManager>(
        new TouchscreenDeviceManagerOzone());
  }
#endif

  virtual void InitializeUI() OVERRIDE {
    surface_factory_ozone_.reset(new CacaSurfaceFactory(&connection_));
    event_factory_ozone_.reset(new CacaEventFactory(&connection_));
    cursor_factory_ozone_.reset(new CursorFactoryOzone());
  }

  virtual void InitializeGPU() OVERRIDE {}

 private:
  CacaConnection connection_;
  scoped_ptr<CacaSurfaceFactory> surface_factory_ozone_;
  scoped_ptr<CacaEventFactory> event_factory_ozone_;
  scoped_ptr<CursorFactoryOzone> cursor_factory_ozone_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformCaca);
};

}  // namespace

OzonePlatform* CreateOzonePlatformCaca() { return new OzonePlatformCaca; }

}  // namespace ui
