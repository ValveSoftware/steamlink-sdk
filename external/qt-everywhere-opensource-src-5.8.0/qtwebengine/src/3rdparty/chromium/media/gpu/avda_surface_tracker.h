// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AVDA_SURFACE_TRACKER_H_
#define MEDIA_GPU_AVDA_SURFACE_TRACKER_H_

#include <vector>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

using OnDestroyingSurfaceCallback = base::Callback<void(int)>;

// AVDASurfaceTracker provides AndroidVideoDecodeAccelerators a way to register
// callbacks that will be called when surfaces are destroyed. This is important
// because Android SurfaceView Surfaces can be destroyed by the framework at any
// time and we must stop rendering to them before returning from
// onSurfaceDestroyed().
// This is not thread safe. All access should be on the gpu main thread.
// Callbacks will be run on the same thread.
class MEDIA_GPU_EXPORT AVDASurfaceTracker {
 public:
  AVDASurfaceTracker();
  ~AVDASurfaceTracker();

  static AVDASurfaceTracker* GetInstance();

  void RegisterOnDestroyingSurfaceCallback(
      const OnDestroyingSurfaceCallback& cb);

  // Unregister a callback that was previously registered.
  void UnregisterOnDestroyingSurfaceCallback(
      const OnDestroyingSurfaceCallback& cb);

  // Called in response to a message from the Browser indicating that the given
  // surface is in the process of being destroyed.
  void NotifyDestroyingSurface(int surface_id);

 private:
  std::vector<OnDestroyingSurfaceCallback> callbacks_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AVDASurfaceTracker);
};

}  // namespace media

#endif  // MEDIA_GPU_AVDA_SURFACE_TRACKER_H_
