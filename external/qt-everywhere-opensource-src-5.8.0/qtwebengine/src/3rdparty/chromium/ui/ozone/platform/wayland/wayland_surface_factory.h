// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SURFACE_FACTORY_H_

#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class WaylandDisplay;

class WaylandSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit WaylandSurfaceFactory(WaylandDisplay* display);
  ~WaylandSurfaceFactory() override;

  intptr_t GetNativeDisplay() override;
  bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) override;
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  std::unique_ptr<SurfaceOzoneEGL> CreateEGLSurfaceForWidget(
      gfx::AcceleratedWidget w) override;
  scoped_refptr<NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
  scoped_refptr<NativePixmap> CreateNativePixmapFromHandle(
      gfx::Size size,
      gfx::BufferFormat format,
      const gfx::NativePixmapHandle& handle) override;

 private:
  WaylandDisplay* display_;

  DISALLOW_COPY_AND_ASSIGN(WaylandSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SURFACE_FACTORY_H_
