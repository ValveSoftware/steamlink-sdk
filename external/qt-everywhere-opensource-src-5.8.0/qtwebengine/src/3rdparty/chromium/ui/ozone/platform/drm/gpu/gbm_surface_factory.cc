// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"

#include <gbm.h>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

GbmSurfaceFactory::GbmSurfaceFactory(DrmThreadProxy* drm_thread)
    : drm_thread_(drm_thread) {}

GbmSurfaceFactory::~GbmSurfaceFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void GbmSurfaceFactory::RegisterSurface(gfx::AcceleratedWidget widget,
                                        GbmSurfaceless* surface) {
  DCHECK(thread_checker_.CalledOnValidThread());
  widget_to_surface_map_.insert(std::make_pair(widget, surface));
}

void GbmSurfaceFactory::UnregisterSurface(gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  widget_to_surface_map_.erase(widget);
}

GbmSurfaceless* GbmSurfaceFactory::GetSurface(
    gfx::AcceleratedWidget widget) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = widget_to_surface_map_.find(widget);
  DCHECK(it != widget_to_surface_map_.end());
  return it->second;
}

intptr_t GbmSurfaceFactory::GetNativeDisplay() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return EGL_DEFAULT_DISPLAY;
}

bool GbmSurfaceFactory::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return LoadDefaultEGLGLES2Bindings(add_gl_library, set_gl_get_proc_address);
}

std::unique_ptr<SurfaceOzoneCanvas> GbmSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(ERROR) << "Software rendering mode is not supported with GBM platform";
  return nullptr;
}

std::unique_ptr<SurfaceOzoneEGL> GbmSurfaceFactory::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<SurfaceOzoneEGL>
GbmSurfaceFactory::CreateSurfacelessEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::WrapUnique(
      new GbmSurfaceless(drm_thread_->CreateDrmWindowProxy(widget), this));
}

std::vector<gfx::BufferFormat> GbmSurfaceFactory::GetScanoutFormats(
    gfx::AcceleratedWidget widget) {
  std::vector<gfx::BufferFormat> scanout_formats;
  drm_thread_->GetScanoutFormats(widget, &scanout_formats);
  return scanout_formats;
}

scoped_refptr<ui::NativePixmap> GbmSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
#if !defined(OS_CHROMEOS)
  // Support for memory mapping accelerated buffers requires some
  // CrOS-specific patches (using dma-buf mmap API).
  DCHECK(gfx::BufferUsage::SCANOUT == usage);
#endif

  scoped_refptr<GbmBuffer> buffer =
      drm_thread_->CreateBuffer(widget, size, format, usage);
  if (!buffer.get())
    return nullptr;

  return make_scoped_refptr(new GbmPixmap(this, buffer));
}

scoped_refptr<ui::NativePixmap> GbmSurfaceFactory::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    const gfx::NativePixmapHandle& handle) {
  size_t planes = gfx::NumberOfPlanesForBufferFormat(format);
  if (handle.strides_and_offsets.size() != planes ||
      (handle.fds.size() != 1 && handle.fds.size() != planes)) {
    return nullptr;
  }
  std::vector<base::ScopedFD> scoped_fds;
  for (auto& fd : handle.fds) {
    scoped_fds.emplace_back(fd.fd);
  }

  std::vector<int> strides;
  std::vector<int> offsets;

  for (const auto& stride_and_offset : handle.strides_and_offsets) {
    strides.push_back(stride_and_offset.first);
    offsets.push_back(stride_and_offset.second);
  }

  scoped_refptr<GbmBuffer> buffer = drm_thread_->CreateBufferFromFds(
      widget, size, format, std::move(scoped_fds), strides, offsets);
  if (!buffer)
    return nullptr;
  return make_scoped_refptr(new GbmPixmap(this, buffer));
}

}  // namespace ui
