// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_GPU_MEMORY_TRACKER_H_
#define COMPONENTS_MUS_GLES2_GPU_MEMORY_TRACKER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "gpu/command_buffer/service/memory_tracking.h"

namespace mus {

// TODO(fsamuel, rjkroege): This is a stub implementation that needs to be
// completed for proper memory tracking.
class GpuMemoryTracker : public gpu::gles2::MemoryTracker {
 public:
  GpuMemoryTracker();

  // gpu::MemoryTracker implementation:
  void TrackMemoryAllocatedChange(
      size_t old_size,
      size_t new_size) override;
  bool EnsureGPUMemoryAvailable(size_t size_needed) override;
  uint64_t ClientTracingId() const override;
  int ClientId() const override;
  uint64_t ShareGroupTracingGUID() const override;

 private:
  ~GpuMemoryTracker() override;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryTracker);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_GPU_MEMORY_TRACKER_H_
