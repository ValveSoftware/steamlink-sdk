// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/gpu_memory_buffer_impl_surface_texture.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/common/android/surface_texture_manager.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
namespace {

int WindowFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
      return WINDOW_FORMAT_RGBA_8888;
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

void FreeSurfaceTextureForTesting(
    scoped_refptr<gl::SurfaceTexture> surface_texture,
    gfx::GpuMemoryBufferId id) {
  gpu::SurfaceTextureManager::GetInstance()->UnregisterSurfaceTexture(id.id, 0);
}

}  // namespace

GpuMemoryBufferImplSurfaceTexture::GpuMemoryBufferImplSurfaceTexture(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    const DestructionCallback& callback,
    ANativeWindow* native_window)
    : GpuMemoryBufferImpl(id, size, format, callback),
      native_window_(native_window) {}

GpuMemoryBufferImplSurfaceTexture::~GpuMemoryBufferImplSurfaceTexture() {
  ANativeWindow_release(native_window_);
}

// static
std::unique_ptr<GpuMemoryBufferImplSurfaceTexture>
GpuMemoryBufferImplSurfaceTexture::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  ANativeWindow* native_window =
      gpu::SurfaceTextureManager::GetInstance()
          ->AcquireNativeWidgetForSurfaceTexture(handle.id.id);
  if (!native_window)
    return nullptr;

  ANativeWindow_setBuffersGeometry(native_window, size.width(), size.height(),
                                   WindowFormat(format));

  return base::WrapUnique(new GpuMemoryBufferImplSurfaceTexture(
      handle.id, size, format, callback, native_window));
}

// static
bool GpuMemoryBufferImplSurfaceTexture::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return gpu::IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
}

// static
base::Closure GpuMemoryBufferImplSurfaceTexture::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  scoped_refptr<gl::SurfaceTexture> surface_texture =
      gl::SurfaceTexture::Create(0);
  DCHECK(surface_texture);
  const gfx::GpuMemoryBufferId kBufferId(1);
  gpu::SurfaceTextureManager::GetInstance()->RegisterSurfaceTexture(
      kBufferId.id, 0, surface_texture.get());
  handle->type = gfx::SURFACE_TEXTURE_BUFFER;
  handle->id = kBufferId;
  return base::Bind(&FreeSurfaceTextureForTesting, surface_texture, kBufferId);
}

bool GpuMemoryBufferImplSurfaceTexture::Map() {
  TRACE_EVENT0("gpu", "GpuMemoryBufferImplSurfaceTexture::Map");
  DCHECK(!mapped_);
  DCHECK(native_window_);
  if (ANativeWindow_lock(native_window_, &buffer_, NULL)) {
    DPLOG(ERROR) << "ANativeWindow_lock failed";
    return false;
  }
  DCHECK_LE(size_.width(), buffer_.stride);
  mapped_ = true;
  return true;
}

void* GpuMemoryBufferImplSurfaceTexture::memory(size_t plane) {
  TRACE_EVENT0("gpu", "GpuMemoryBufferImplSurfaceTexture::memory");
  DCHECK(mapped_);
  DCHECK_EQ(0u, plane);
  return buffer_.bits;
}

void GpuMemoryBufferImplSurfaceTexture::Unmap() {
  TRACE_EVENT0("gpu", "GpuMemoryBufferImplSurfaceTexture::Unmap");
  DCHECK(mapped_);
  ANativeWindow_unlockAndPost(native_window_);
  mapped_ = false;
}

int GpuMemoryBufferImplSurfaceTexture::stride(size_t plane) const {
  DCHECK(mapped_);
  DCHECK_EQ(0u, plane);
  return gfx::RowSizeForBufferFormat(buffer_.stride, format_, 0);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplSurfaceTexture::GetHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SURFACE_TEXTURE_BUFFER;
  handle.id = id_;
  return handle;
}

}  // namespace gpu
