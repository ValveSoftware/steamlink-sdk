// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_UMA_STATS_H_
#define GPU_IPC_COMMON_GPU_MEMORY_UMA_STATS_H_

#include <stddef.h>
#include <stdint.h>

namespace gpu {

// Memory usage statistics send periodically to the browser process to report
// in UMA histograms if the GPU process crashes.
// Note: we use uint64_t instead of size_t for byte count because this struct
// is sent over IPC which could span 32 & 64 bit processes.
struct GPUMemoryUmaStats {
  GPUMemoryUmaStats()
      : bytes_allocated_current(0),
        bytes_allocated_max(0),
        context_group_count(0) {}

  // The number of bytes currently allocated.
  uint64_t bytes_allocated_current;

  // The maximum number of bytes ever allocated at once.
  uint64_t bytes_allocated_max;

  // The number of context groups.
  uint32_t context_group_count;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_UMA_STATS_H_
