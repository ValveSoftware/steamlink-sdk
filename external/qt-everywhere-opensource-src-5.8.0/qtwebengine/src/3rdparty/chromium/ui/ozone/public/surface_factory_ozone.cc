// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/surface_factory_ozone.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

SurfaceFactoryOzone::SurfaceFactoryOzone() {
}

SurfaceFactoryOzone::~SurfaceFactoryOzone() {
}

intptr_t SurfaceFactoryOzone::GetNativeDisplay() {
  return 0;
}

std::unique_ptr<SurfaceOzoneEGL> SurfaceFactoryOzone::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  return nullptr;
}

std::unique_ptr<SurfaceOzoneEGL>
SurfaceFactoryOzone::CreateSurfacelessEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  return nullptr;
}

std::unique_ptr<SurfaceOzoneCanvas> SurfaceFactoryOzone::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  return nullptr;
}

std::vector<gfx::BufferFormat> SurfaceFactoryOzone::GetScanoutFormats(
    gfx::AcceleratedWidget widget) {
  return std::vector<gfx::BufferFormat>();
}

scoped_refptr<ui::NativePixmap> SurfaceFactoryOzone::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return nullptr;
}

scoped_refptr<ui::NativePixmap>
SurfaceFactoryOzone::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    const gfx::NativePixmapHandle& handle) {
  return nullptr;
}

}  // namespace ui
