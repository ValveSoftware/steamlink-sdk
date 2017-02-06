// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_BROWSER_SURFACE_TEXTURE_MANAGER_H_
#define CONTENT_BROWSER_ANDROID_BROWSER_SURFACE_TEXTURE_MANAGER_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "gpu/ipc/common/android/surface_texture_manager.h"

namespace content {

class CONTENT_EXPORT BrowserSurfaceTextureManager
    : public gpu::SurfaceTextureManager {
 public:
  static BrowserSurfaceTextureManager* GetInstance();

  // Overridden from SurfaceTextureManager:
  void RegisterSurfaceTexture(int surface_texture_id,
                              int client_id,
                              gl::SurfaceTexture* surface_texture) override;
  void UnregisterSurfaceTexture(int surface_texture_id, int client_id) override;
  gfx::AcceleratedWidget AcquireNativeWidgetForSurfaceTexture(
      int surface_texture_id) override;

 private:
  friend struct base::DefaultSingletonTraits<BrowserSurfaceTextureManager>;

  BrowserSurfaceTextureManager();
  ~BrowserSurfaceTextureManager() override;

  DISALLOW_COPY_AND_ASSIGN(BrowserSurfaceTextureManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_BROWSER_SURFACE_TEXTURE_MANAGER_H_
