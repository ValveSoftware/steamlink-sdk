// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_UMA_STATS_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_UMA_STATS_H_

#include "base/basictypes.h"

namespace content {

// Memory usage statistics send periodically to the browser process to report
// in UMA histograms if the GPU process crashes.
struct GPUMemoryUmaStats {
  GPUMemoryUmaStats()
      : bytes_allocated_current(0),
        bytes_allocated_max(0),
        bytes_limit(0),
        client_count(0),
        context_group_count(0) {
  }

  // The number of bytes currently allocated.
  size_t bytes_allocated_current;

  // The maximum number of bytes ever allocated at once.
  size_t bytes_allocated_max;

  // The memory limit being imposed by the memory manager.
  size_t bytes_limit;

  // The number of managed memory clients.
  size_t client_count;

  // The number of context groups.
  size_t context_group_count;

  // The number of visible windows.
  uint32 window_count;
};

}  // namespace content

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_UMA_STATS_H_
