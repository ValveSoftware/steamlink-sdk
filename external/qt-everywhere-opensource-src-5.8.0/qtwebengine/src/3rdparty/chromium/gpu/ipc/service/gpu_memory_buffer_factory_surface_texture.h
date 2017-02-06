// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_SURFACE_TEXTURE_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_SURFACE_TEXTURE_H_

#include <utility>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gl {
class GLImage;
class SurfaceTexture;
}

namespace gpu {

class GPU_EXPORT GpuMemoryBufferFactorySurfaceTexture
    : public GpuMemoryBufferFactory,
      public ImageFactory {
 public:
  GpuMemoryBufferFactorySurfaceTexture();
  ~GpuMemoryBufferFactorySurfaceTexture() override;

  // Overridden from GpuMemoryBufferFactory:
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      SurfaceHandle surface_handle) override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id) override;
  ImageFactory* AsImageFactory() override;

  // Overridden from ImageFactory:
  scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      unsigned internalformat,
      int client_id,
      SurfaceHandle surface_handle) override;

 private:
  typedef std::pair<int, int> SurfaceTextureMapKey;
  typedef base::hash_map<SurfaceTextureMapKey,
                         scoped_refptr<gl::SurfaceTexture>>
      SurfaceTextureMap;
  SurfaceTextureMap surface_textures_;
  base::Lock surface_textures_lock_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_SURFACE_TEXTURE_H_
