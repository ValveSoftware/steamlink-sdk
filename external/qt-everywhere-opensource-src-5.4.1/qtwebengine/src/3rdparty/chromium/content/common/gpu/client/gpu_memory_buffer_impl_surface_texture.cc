// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_surface_texture.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/common/android/surface_texture_lookup.h"
#include "ui/gl/gl_bindings.h"

namespace content {

GpuMemoryBufferImplSurfaceTexture::GpuMemoryBufferImplSurfaceTexture(
    const gfx::Size& size,
    unsigned internalformat)
    : GpuMemoryBufferImpl(size, internalformat),
      native_window_(NULL),
      stride_(0u) {}

GpuMemoryBufferImplSurfaceTexture::~GpuMemoryBufferImplSurfaceTexture() {
  if (native_window_)
    ANativeWindow_release(native_window_);
}

// static
bool GpuMemoryBufferImplSurfaceTexture::IsFormatSupported(
    unsigned internalformat) {
  switch (internalformat) {
    case GL_RGBA8_OES:
      return true;
    default:
      return false;
  }
}

// static
bool GpuMemoryBufferImplSurfaceTexture::IsUsageSupported(unsigned usage) {
  switch (usage) {
    case GL_IMAGE_MAP_CHROMIUM:
      return true;
    default:
      return false;
  }
}

// static
bool GpuMemoryBufferImplSurfaceTexture::IsConfigurationSupported(
    unsigned internalformat,
    unsigned usage) {
  return IsFormatSupported(internalformat) && IsUsageSupported(usage);
}

// static
int GpuMemoryBufferImplSurfaceTexture::WindowFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_RGBA8_OES:
      return WINDOW_FORMAT_RGBA_8888;
    default:
      NOTREACHED();
      return 0;
  }
}

bool GpuMemoryBufferImplSurfaceTexture::InitializeFromHandle(
    gfx::GpuMemoryBufferHandle handle) {
  TRACE_EVENT0("gpu",
               "GpuMemoryBufferImplSurfaceTexture::InitializeFromHandle");

  DCHECK(IsFormatSupported(internalformat_));
  DCHECK(!native_window_);
  native_window_ = SurfaceTextureLookup::GetInstance()->AcquireNativeWidget(
      handle.surface_texture_id.primary_id,
      handle.surface_texture_id.secondary_id);
  if (!native_window_)
    return false;

  ANativeWindow_setBuffersGeometry(native_window_,
                                   size_.width(),
                                   size_.height(),
                                   WindowFormat(internalformat_));

  surface_texture_id_ = handle.surface_texture_id;
  return true;
}

void* GpuMemoryBufferImplSurfaceTexture::Map() {
  TRACE_EVENT0("gpu", "GpuMemoryBufferImplSurfaceTexture::Map");

  DCHECK(!mapped_);
  DCHECK(native_window_);
  ANativeWindow_Buffer buffer;
  int status = ANativeWindow_lock(native_window_, &buffer, NULL);
  if (status) {
    VLOG(1) << "ANativeWindow_lock failed with error code: " << status;
    return NULL;
  }

  DCHECK_LE(size_.width(), buffer.stride);
  stride_ = buffer.stride * BytesPerPixel(internalformat_);
  mapped_ = true;
  return buffer.bits;
}

void GpuMemoryBufferImplSurfaceTexture::Unmap() {
  TRACE_EVENT0("gpu", "GpuMemoryBufferImplSurfaceTexture::Unmap");

  DCHECK(mapped_);
  ANativeWindow_unlockAndPost(native_window_);
  mapped_ = false;
}

uint32 GpuMemoryBufferImplSurfaceTexture::GetStride() const { return stride_; }

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplSurfaceTexture::GetHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SURFACE_TEXTURE_BUFFER;
  handle.surface_texture_id = surface_texture_id_;
  return handle;
}

}  // namespace content
