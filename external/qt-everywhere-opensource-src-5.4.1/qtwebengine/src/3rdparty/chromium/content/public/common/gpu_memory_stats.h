// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_GPU_MEMORY_STATS_H_
#define CONTENT_PUBLIC_COMMON_GPU_MEMORY_STATS_H_

// Provides access to the GPU information for the system
// on which chrome is currently running.

#include <map>

#include "base/basictypes.h"
#include "base/process/process.h"
#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT GPUVideoMemoryUsageStats {
  GPUVideoMemoryUsageStats();
  ~GPUVideoMemoryUsageStats();

  struct CONTENT_EXPORT ProcessStats {
    ProcessStats();
    ~ProcessStats();

    // The bytes of GPU resources accessible by this process
    size_t video_memory;

    // Set to true if this process' GPU resource count is inflated because
    // it is counting other processes' resources (e.g, the GPU process has
    // duplicate set to true because it is the aggregate of all processes)
    bool has_duplicates;
  };
  typedef std::map<base::ProcessId, ProcessStats> ProcessMap;

  // A map of processes to their GPU resource consumption
  ProcessMap process_map;

  // The total amount of GPU memory allocated at the time of the request.
  size_t bytes_allocated;

  // The maximum amount of GPU memory ever allocated at once.
  size_t bytes_allocated_historical_max;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_GPU_MEMORY_STATS_H_
