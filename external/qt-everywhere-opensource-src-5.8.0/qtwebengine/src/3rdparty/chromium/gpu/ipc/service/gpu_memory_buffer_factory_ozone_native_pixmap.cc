// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_ozone_native_pixmap.h"

#include "ui/gl/gl_image_ozone_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {

GpuMemoryBufferFactoryOzoneNativePixmap::
    GpuMemoryBufferFactoryOzoneNativePixmap() {}

GpuMemoryBufferFactoryOzoneNativePixmap::
    ~GpuMemoryBufferFactoryOzoneNativePixmap() {}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryOzoneNativePixmap::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
  scoped_refptr<ui::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(surface_handle, size, format, usage);
  if (!pixmap.get()) {
    DLOG(ERROR) << "Failed to create pixmap " << size.width() << "x"
                << size.height() << " format " << static_cast<int>(format)
                << ", usage " << static_cast<int>(usage);
    return gfx::GpuMemoryBufferHandle();
  }

  gfx::GpuMemoryBufferHandle new_handle;
  new_handle.type = gfx::OZONE_NATIVE_PIXMAP;
  new_handle.id = id;
  new_handle.native_pixmap_handle = pixmap->ExportHandle();

  // TODO(reveman): Remove this once crbug.com/628334 has been fixed.
  {
    base::AutoLock lock(native_pixmaps_lock_);
    NativePixmapMapKey key(id.id, client_id);
    DCHECK(native_pixmaps_.find(key) == native_pixmaps_.end());
    native_pixmaps_[key] = pixmap;
  }

  return new_handle;
}

void GpuMemoryBufferFactoryOzoneNativePixmap::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  base::AutoLock lock(native_pixmaps_lock_);
  NativePixmapMapKey key(id.id, client_id);
  native_pixmaps_.erase(key);
}

ImageFactory* GpuMemoryBufferFactoryOzoneNativePixmap::AsImageFactory() {
  return this;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryOzoneNativePixmap::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    unsigned internalformat,
    int client_id,
    SurfaceHandle surface_handle) {
  DCHECK_EQ(handle.type, gfx::OZONE_NATIVE_PIXMAP);

  scoped_refptr<ui::NativePixmap> pixmap;

  // If CreateGpuMemoryBuffer was used to allocate this buffer then avoid
  // creating a new native pixmap for it.
  {
    base::AutoLock lock(native_pixmaps_lock_);
    NativePixmapMapKey key(handle.id.id, client_id);
    auto it = native_pixmaps_.find(key);
    if (it != native_pixmaps_.end())
      pixmap = it->second;
  }

  // Create new pixmap from handle if one doesn't already exist.
  if (!pixmap) {
    pixmap = ui::OzonePlatform::GetInstance()
                 ->GetSurfaceFactoryOzone()
                 ->CreateNativePixmapFromHandle(surface_handle, size, format,
                                                handle.native_pixmap_handle);
    if (!pixmap.get()) {
      DLOG(ERROR) << "Failed to create pixmap from handle";
      return nullptr;
    }
  } else {
    for (const auto& fd : handle.native_pixmap_handle.fds) {
      // Close the fd by wrapping it in a ScopedFD and letting it fall
      // out of scope.
      base::ScopedFD scoped_fd(fd.fd);
    }
  }

  scoped_refptr<gl::GLImageOzoneNativePixmap> image(
      new gl::GLImageOzoneNativePixmap(size, internalformat));
  if (!image->Initialize(pixmap.get(), format)) {
    LOG(ERROR) << "Failed to create GLImage";
    return nullptr;
  }
  return image;
}

}  // namespace gpu
