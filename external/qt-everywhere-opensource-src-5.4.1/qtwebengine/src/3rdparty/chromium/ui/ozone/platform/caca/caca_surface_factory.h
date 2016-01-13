// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CACA_CACA_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_CACA_CACA_SURFACE_FACTORY_H_

#include <caca.h>

#include "base/memory/scoped_ptr.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gfx {
class SurfaceOzone;
}

namespace ui {

class CacaConnection;

class CacaSurfaceFactory : public ui::SurfaceFactoryOzone {
 public:
  CacaSurfaceFactory(CacaConnection* connection);
  virtual ~CacaSurfaceFactory();

  // ui::SurfaceFactoryOzone overrides:
  virtual HardwareState InitializeHardware() OVERRIDE;
  virtual void ShutdownHardware() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE;
  virtual scoped_ptr<ui::SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) OVERRIDE;

 private:
  HardwareState state_;

  CacaConnection* connection_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(CacaSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CACA_CACA_SURFACE_FACTORY_H_
