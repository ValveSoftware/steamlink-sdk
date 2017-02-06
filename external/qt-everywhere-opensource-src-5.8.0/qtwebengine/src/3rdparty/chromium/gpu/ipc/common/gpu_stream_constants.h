// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_STREAM_CONSTANTS_H_
#define GPU_IPC_COMMON_GPU_STREAM_CONSTANTS_H_

namespace gpu {

enum class GpuStreamPriority { REAL_TIME, HIGH, NORMAL, LOW, LAST = LOW };

enum GpuStreamId { GPU_STREAM_DEFAULT = -1, GPU_STREAM_INVALID = 0 };

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_STREAM_CONSTANTS_H_
