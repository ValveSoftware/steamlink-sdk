// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/gpu_memory_tracker.h"

namespace mus {

GpuMemoryTracker::GpuMemoryTracker() {}

void GpuMemoryTracker::TrackMemoryAllocatedChange(
    size_t old_size,
    size_t new_size) {}

bool GpuMemoryTracker::EnsureGPUMemoryAvailable(size_t size_needed) {
  return true;
}

uint64_t GpuMemoryTracker::ClientTracingId() const {
  return 0u;
}

int GpuMemoryTracker::ClientId() const {
  return 0;
}

uint64_t GpuMemoryTracker::ShareGroupTracingGUID() const {
  // TODO(rjkroege): This should return a GUID which identifies the share group
  // we are tracking memory for. This GUID should correspond to the GUID of the
  // first command buffer in the share group.
  return 0;
}

GpuMemoryTracker::~GpuMemoryTracker() {}

}  // namespace mus
