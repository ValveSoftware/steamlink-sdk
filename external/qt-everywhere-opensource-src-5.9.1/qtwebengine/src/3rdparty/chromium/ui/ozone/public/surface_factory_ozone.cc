// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/surface_factory_ozone.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

SurfaceFactoryOzone::SurfaceFactoryOzone() {}

SurfaceFactoryOzone::~SurfaceFactoryOzone() {}

std::vector<gl::GLImplementation>
SurfaceFactoryOzone::GetAllowedGLImplementations() {
  std::vector<gl::GLImplementation> impls;
  // TODO(kylechar): Remove EGL from this list once every Ozone platform that
  // uses EGL overrides this method.
  impls.push_back(gl::kGLImplementationEGLGLES2);
  impls.push_back(gl::kGLImplementationOSMesaGL);
  return impls;
}

GLOzone* SurfaceFactoryOzone::GetGLOzone(gl::GLImplementation implementation) {
  return nullptr;
}

intptr_t SurfaceFactoryOzone::GetNativeDisplay() {
  return 0;
}

scoped_refptr<gl::GLSurface> SurfaceFactoryOzone::CreateViewGLSurface(
    gl::GLImplementation implementation,
    gfx::AcceleratedWidget widget) {
  return nullptr;
}

scoped_refptr<gl::GLSurface>
SurfaceFactoryOzone::CreateSurfacelessViewGLSurface(
    gl::GLImplementation implementation,
    gfx::AcceleratedWidget widget) {
  return nullptr;
}

scoped_refptr<gl::GLSurface> SurfaceFactoryOzone::CreateOffscreenGLSurface(
    gl::GLImplementation implementation,
    const gfx::Size& size) {
  return nullptr;
}

std::unique_ptr<SurfaceOzoneCanvas> SurfaceFactoryOzone::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  return nullptr;
}

bool SurfaceFactoryOzone::LoadEGLGLES2Bindings() {
  return false;
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
