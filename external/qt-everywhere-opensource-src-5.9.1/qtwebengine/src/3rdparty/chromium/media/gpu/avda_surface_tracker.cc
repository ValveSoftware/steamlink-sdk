// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/avda_surface_tracker.h"

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_checker.h"

namespace media {

namespace {
static base::LazyInstance<AVDASurfaceTracker> g_lazy_instance =
    LAZY_INSTANCE_INITIALIZER;
}

AVDASurfaceTracker::AVDASurfaceTracker() {}

AVDASurfaceTracker::~AVDASurfaceTracker() {}

void AVDASurfaceTracker::RegisterOnDestroyingSurfaceCallback(
    const OnDestroyingSurfaceCallback& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callbacks_.push_back(cb);
}

void AVDASurfaceTracker::UnregisterOnDestroyingSurfaceCallback(
    const OnDestroyingSurfaceCallback& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
    if (it->Equals(cb)) {
      callbacks_.erase(it);
      return;
    }
  }
}

void AVDASurfaceTracker::NotifyDestroyingSurface(int surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (const auto& cb : callbacks_)
    cb.Run(surface_id);
}

AVDASurfaceTracker* AVDASurfaceTracker::GetInstance() {
  return g_lazy_instance.Pointer();
}

}  // namespace media
