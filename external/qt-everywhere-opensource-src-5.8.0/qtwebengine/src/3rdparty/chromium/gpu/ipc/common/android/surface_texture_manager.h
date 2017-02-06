// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_ANDROID_SURFACE_TEXTURE_MANAGER_H_
#define GPU_IPC_COMMON_ANDROID_SURFACE_TEXTURE_MANAGER_H_

#include "gpu/gpu_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gl {
class SurfaceTexture;
}

namespace gpu {

class GPU_EXPORT SurfaceTextureManager {
 public:
  static SurfaceTextureManager* GetInstance();
  static void SetInstance(SurfaceTextureManager* instance);

  // Register a surface texture for use in another process.
  virtual void RegisterSurfaceTexture(int surface_texture_id,
                                      int client_id,
                                      gl::SurfaceTexture* surface_texture) = 0;

  // Unregister a surface texture previously registered for use in another
  // process.
  virtual void UnregisterSurfaceTexture(int surface_texture_id,
                                        int client_id) = 0;

  // Acquire native widget for a registered surface texture.
  virtual gfx::AcceleratedWidget AcquireNativeWidgetForSurfaceTexture(
      int surface_texture_id) = 0;

 protected:
  virtual ~SurfaceTextureManager() {}
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_ANDROID_SURFACE_TEXTURE_MANAGER_H_
