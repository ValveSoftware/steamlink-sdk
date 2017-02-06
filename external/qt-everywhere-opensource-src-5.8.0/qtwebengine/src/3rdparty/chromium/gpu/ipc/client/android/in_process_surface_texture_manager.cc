// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/android/in_process_surface_texture_manager.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "base/android/jni_android.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace gpu {

// static
InProcessSurfaceTextureManager* InProcessSurfaceTextureManager::GetInstance() {
  return base::Singleton<
      InProcessSurfaceTextureManager,
      base::LeakySingletonTraits<InProcessSurfaceTextureManager>>::get();
}

void InProcessSurfaceTextureManager::RegisterSurfaceTexture(
    int surface_texture_id,
    int client_id,
    gl::SurfaceTexture* surface_texture) {
  base::AutoLock lock(lock_);

  DCHECK(surface_textures_.find(surface_texture_id) == surface_textures_.end());
  surface_textures_.add(
      surface_texture_id,
      base::WrapUnique(new gl::ScopedJavaSurface(surface_texture)));
}

void InProcessSurfaceTextureManager::UnregisterSurfaceTexture(
    int surface_texture_id,
    int client_id) {
  base::AutoLock lock(lock_);

  DCHECK(surface_textures_.find(surface_texture_id) != surface_textures_.end());
  surface_textures_.erase(surface_texture_id);
}

gfx::AcceleratedWidget
InProcessSurfaceTextureManager::AcquireNativeWidgetForSurfaceTexture(
    int surface_texture_id) {
  base::AutoLock lock(lock_);

  DCHECK(surface_textures_.find(surface_texture_id) != surface_textures_.end());
  JNIEnv* env = base::android::AttachCurrentThread();
  return ANativeWindow_fromSurface(
      env, surface_textures_.get(surface_texture_id)->j_surface().obj());
}

InProcessSurfaceTextureManager::InProcessSurfaceTextureManager() {}

InProcessSurfaceTextureManager::~InProcessSurfaceTextureManager() {}

}  // namespace gpu
