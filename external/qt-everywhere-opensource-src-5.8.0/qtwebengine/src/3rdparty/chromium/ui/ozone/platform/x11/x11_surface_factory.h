// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_X11_X11_SURFACE_FACTORY_H_

#include <memory>

#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

// Handles creation of EGL and software surfaces for drawing in XWindow.
class X11SurfaceFactory : public SurfaceFactoryOzone {
 public:
  X11SurfaceFactory();
  ~X11SurfaceFactory() override;

  // SurfaceFactoryOzone:

  std::unique_ptr<SurfaceOzoneEGL> CreateEGLSurfaceForWidget(
      gfx::AcceleratedWidget widget) override;
  bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) override;
  intptr_t GetNativeDisplay() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(X11SurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_SURFACE_FACTORY_H_
