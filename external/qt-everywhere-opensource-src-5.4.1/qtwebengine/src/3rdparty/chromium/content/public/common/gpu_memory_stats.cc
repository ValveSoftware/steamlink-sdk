// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/gpu_memory_stats.h"

namespace content {

GPUVideoMemoryUsageStats::GPUVideoMemoryUsageStats()
    : bytes_allocated(0),
      bytes_allocated_historical_max(0) {
}

GPUVideoMemoryUsageStats::~GPUVideoMemoryUsageStats() {
}

GPUVideoMemoryUsageStats::ProcessStats::ProcessStats()
    : video_memory(0),
      has_duplicates(false) {
}

GPUVideoMemoryUsageStats::ProcessStats::~ProcessStats() {
}

}  // namespace content
