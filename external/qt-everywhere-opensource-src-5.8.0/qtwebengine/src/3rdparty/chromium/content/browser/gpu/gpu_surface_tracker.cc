// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_surface_tracker.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include <android/native_window_jni.h>
#include "content/browser/android/child_process_launcher_android.h"
#include "ui/gl/android/scoped_java_surface.h"
#endif  // defined(OS_ANDROID)

namespace content {

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
  base::AutoLock lock(lock_);
  gpu::SurfaceHandle surface_handle = next_surface_handle_++;
  surface_map_[surface_handle] = widget;
  return surface_handle;
}

void GpuSurfaceTracker::RemoveSurface(gpu::SurfaceHandle surface_handle) {
  base::AutoLock lock(lock_);
  DCHECK(surface_map_.find(surface_handle) != surface_map_.end());
  surface_map_.erase(surface_handle);
}

gfx::AcceleratedWidget GpuSurfaceTracker::AcquireNativeWidget(
    gpu::SurfaceHandle surface_handle) {
  base::AutoLock lock(lock_);
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
gl::ScopedJavaSurface GpuSurfaceTracker::AcquireJavaSurface(int surface_id) {
  return GetViewSurface(surface_id);
}
#endif

std::size_t GpuSurfaceTracker::GetSurfaceCount() {
  base::AutoLock lock(lock_);
  return surface_map_.size();
}

}  // namespace content
