// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_surface_tracker.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include <android/native_window_jni.h>
#include "ui/gl/android/scoped_java_surface.h"
#endif  // defined(OS_ANDROID)

namespace gpu {

GpuSurfaceTracker::GpuSurfaceTracker()
    : next_surface_handle_(1) {
  gpu::GpuSurfaceLookup::InitInstance(this);
}

GpuSurfaceTracker::~GpuSurfaceTracker() {
  gpu::GpuSurfaceLookup::InitInstance(NULL);
}

GpuSurfaceTracker* GpuSurfaceTracker::GetInstance() {
  return base::Singleton<GpuSurfaceTracker>::get();
}

int GpuSurfaceTracker::AddSurfaceForNativeWidget(
    gfx::AcceleratedWidget widget) {
  base::AutoLock lock(surface_map_lock_);
  gpu::SurfaceHandle surface_handle = next_surface_handle_++;
  surface_map_[surface_handle] = widget;
  return surface_handle;
}

bool GpuSurfaceTracker::IsValidSurfaceHandle(
    gpu::SurfaceHandle surface_handle) const {
  base::AutoLock lock(surface_map_lock_);
  return surface_map_.find(surface_handle) != surface_map_.end();
}

void GpuSurfaceTracker::RemoveSurface(gpu::SurfaceHandle surface_handle) {
  base::AutoLock lock(surface_map_lock_);
  DCHECK(surface_map_.find(surface_handle) != surface_map_.end());
  surface_map_.erase(surface_handle);
}

gfx::AcceleratedWidget GpuSurfaceTracker::AcquireNativeWidget(
    gpu::SurfaceHandle surface_handle) {
  base::AutoLock lock(surface_map_lock_);
  SurfaceMap::iterator it = surface_map_.find(surface_handle);
  if (it == surface_map_.end())
    return gfx::kNullAcceleratedWidget;

#if defined(OS_ANDROID)
  if (it->second != gfx::kNullAcceleratedWidget)
    ANativeWindow_acquire(it->second);
#endif  // defined(OS_ANDROID)

  return it->second;
}

#if defined(OS_ANDROID)
void GpuSurfaceTracker::RegisterViewSurface(
    int surface_id, const base::android::JavaRef<jobject>& j_surface) {
  base::AutoLock lock(surface_view_map_lock_);
  DCHECK(surface_view_map_.find(surface_id) == surface_view_map_.end());

  surface_view_map_[surface_id] =
      gl::ScopedJavaSurface::AcquireExternalSurface(j_surface.obj());
  CHECK(surface_view_map_[surface_id].IsValid());
}

void GpuSurfaceTracker::UnregisterViewSurface(int surface_id)
{
  base::AutoLock lock(surface_view_map_lock_);
  DCHECK(surface_view_map_.find(surface_id) != surface_view_map_.end());
  surface_view_map_.erase(surface_id);
}

gl::ScopedJavaSurface GpuSurfaceTracker::AcquireJavaSurface(int surface_id) {
  base::AutoLock lock(surface_view_map_lock_);
  SurfaceViewMap::const_iterator iter = surface_view_map_.find(surface_id);
  if (iter == surface_view_map_.end())
    return gl::ScopedJavaSurface();

  const gl::ScopedJavaSurface& j_surface = iter->second;
  DCHECK(j_surface.IsValid());
  return gl::ScopedJavaSurface::AcquireExternalSurface(
      j_surface.j_surface().obj());
}
#endif

std::size_t GpuSurfaceTracker::GetSurfaceCount() {
  base::AutoLock lock(surface_map_lock_);
  return surface_map_.size();
}

}  // namespace gpu
