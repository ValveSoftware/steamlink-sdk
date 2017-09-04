// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_ANDROID_IN_PROCESS_SURFACE_TEXTURE_MANAGER_H_
#define GPU_IPC_CLIENT_ANDROID_IN_PROCESS_SURFACE_TEXTURE_MANAGER_H_

#include <memory>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/android/surface_texture_manager.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace gpu {

class GPU_EXPORT InProcessSurfaceTextureManager : public SurfaceTextureManager {
 public:
  static GPU_EXPORT InProcessSurfaceTextureManager* GetInstance();

  // Overridden from SurfaceTextureManager:
  void RegisterSurfaceTexture(int surface_texture_id,
                              int client_id,
                              gl::SurfaceTexture* surface_texture) override;
  void UnregisterSurfaceTexture(int surface_texture_id, int client_id) override;
  gfx::AcceleratedWidget AcquireNativeWidgetForSurfaceTexture(
      int surface_texture_id) override;

 private:
  friend struct base::DefaultSingletonTraits<InProcessSurfaceTextureManager>;

  InProcessSurfaceTextureManager();
  ~InProcessSurfaceTextureManager() override;

  using SurfaceTextureMap =
      base::ScopedPtrHashMap<int, std::unique_ptr<gl::ScopedJavaSurface>>;
  SurfaceTextureMap surface_textures_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(InProcessSurfaceTextureManager);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_ANDROID_IN_PROCESS_SURFACE_TEXTURE_MANAGER_H_
