// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_surface_texture.h"

#include "gpu/ipc/common/android/surface_texture_manager.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_image_surface_texture.h"

namespace gpu {

GpuMemoryBufferFactorySurfaceTexture::GpuMemoryBufferFactorySurfaceTexture() {
}

GpuMemoryBufferFactorySurfaceTexture::~GpuMemoryBufferFactorySurfaceTexture() {
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactorySurfaceTexture::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
  // Note: this needs to be 0 as the surface texture implemenation will take
  // ownership of the texture and call glDeleteTextures when the GPU service
  // attaches the surface texture to a real texture id. glDeleteTextures
  // silently ignores 0.
  const int kDummyTextureId = 0;
  scoped_refptr<gl::SurfaceTexture> surface_texture =
      gl::SurfaceTexture::Create(kDummyTextureId);
  if (!surface_texture.get())
    return gfx::GpuMemoryBufferHandle();

  SurfaceTextureManager::GetInstance()->RegisterSurfaceTexture(
      id.id, client_id, surface_texture.get());

  {
    base::AutoLock lock(surface_textures_lock_);

    SurfaceTextureMapKey key(id.id, client_id);
    DCHECK(surface_textures_.find(key) == surface_textures_.end());
    surface_textures_[key] = surface_texture;
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SURFACE_TEXTURE_BUFFER;
  handle.id = id;
  return handle;
}

void GpuMemoryBufferFactorySurfaceTexture::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  {
    base::AutoLock lock(surface_textures_lock_);

    SurfaceTextureMapKey key(id.id, client_id);
    DCHECK(surface_textures_.find(key) != surface_textures_.end());
    surface_textures_.erase(key);
  }

  SurfaceTextureManager::GetInstance()->UnregisterSurfaceTexture(
      id.id, client_id);
}

ImageFactory* GpuMemoryBufferFactorySurfaceTexture::AsImageFactory() {
  return this;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactorySurfaceTexture::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    unsigned internalformat,
    int client_id,
    SurfaceHandle surface_handle) {
  base::AutoLock lock(surface_textures_lock_);

  DCHECK_EQ(handle.type, gfx::SURFACE_TEXTURE_BUFFER);

  SurfaceTextureMapKey key(handle.id.id, client_id);
  SurfaceTextureMap::iterator it = surface_textures_.find(key);
  if (it == surface_textures_.end())
    return scoped_refptr<gl::GLImage>();

  scoped_refptr<gl::GLImageSurfaceTexture> image(
      new gl::GLImageSurfaceTexture(size));
  if (!image->Initialize(it->second.get()))
    return scoped_refptr<gl::GLImage>();

  return image;
}

}  // namespace gpu
