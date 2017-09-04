// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_support.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/client_native_pixmap_factory.h"
#endif

namespace gpu {

gfx::GpuMemoryBufferType GetNativeGpuMemoryBufferType() {
#if defined(OS_MACOSX)
  return gfx::IO_SURFACE_BUFFER;
#endif
#if defined(USE_OZONE)
  return gfx::OZONE_NATIVE_PIXMAP;
#endif
  return gfx::EMPTY_BUFFER;
}

bool IsNativeGpuMemoryBufferConfigurationSupported(gfx::BufferFormat format,
                                                   gfx::BufferUsage usage) {
  DCHECK_NE(gfx::SHARED_MEMORY_BUFFER, GetNativeGpuMemoryBufferType());
  DCHECK_NE(gfx::EMPTY_BUFFER, GetNativeGpuMemoryBufferType());
#if defined(OS_MACOSX)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::BGRX_8888;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      return format == gfx::BufferFormat::R_8 ||
             format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::UYVY_422 ||
             format == gfx::BufferFormat::YUV_420_BIPLANAR;
  }
  NOTREACHED();
  return false;
#endif

#if defined(USE_OZONE)
  if (!ui::ClientNativePixmapFactory::GetInstance()) {
    // unittests don't have to set ClientNativePixmapFactory.
    return false;
  }
  return ui::ClientNativePixmapFactory::GetInstance()->IsConfigurationSupported(
      format, usage);
#endif

  NOTREACHED();
  return false;
}

}  // namespace gpu
