// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/public/surface_factory_ozone.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace gfx {
class SurfaceOzoneCanvas;
}

namespace ui {

class DriSurface;
class DriWrapper;
class HardwareDisplayController;
class ScreenManager;

// SurfaceFactoryOzone implementation on top of DRM/KMS using dumb buffers.
// This implementation is used in conjunction with the software rendering
// path.
class OZONE_EXPORT DriSurfaceFactory : public ui::SurfaceFactoryOzone {
 public:
  static const gfx::AcceleratedWidget kDefaultWidgetHandle;

  DriSurfaceFactory(DriWrapper* drm, ScreenManager* screen_manager);
  virtual ~DriSurfaceFactory();

  // SurfaceFactoryOzone overrides:
  virtual HardwareState InitializeHardware() OVERRIDE;
  virtual void ShutdownHardware() OVERRIDE;

  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;

  virtual scoped_ptr<ui::SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget w) OVERRIDE;

  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE;

  gfx::Size GetWidgetSize(gfx::AcceleratedWidget w);

  void SetHardwareCursor(gfx::AcceleratedWidget window,
                         const SkBitmap& image,
                         const gfx::Point& location);

  void MoveHardwareCursor(gfx::AcceleratedWidget window,
                          const gfx::Point& location);

  void UnsetHardwareCursor(gfx::AcceleratedWidget window);

 protected:
  // Draw the last set cursor & update the cursor plane.
  void ResetCursor(gfx::AcceleratedWidget w);

  virtual DriSurface* CreateSurface(const gfx::Size& size);

  DriWrapper* drm_;  // Not owned.
  ScreenManager* screen_manager_;  // Not owned.
  HardwareState state_;

  // Active outputs.
  int allocated_widgets_;

  scoped_ptr<DriSurface> cursor_surface_;

  SkBitmap cursor_bitmap_;
  gfx::Point cursor_location_;

  DISALLOW_COPY_AND_ASSIGN(DriSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_SURFACE_FACTORY_H_
