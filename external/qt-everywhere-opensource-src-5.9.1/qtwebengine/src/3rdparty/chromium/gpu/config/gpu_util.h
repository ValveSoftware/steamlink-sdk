// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_UTIL_H_
#define GPU_CONFIG_GPU_UTIL_H_

#include <set>
#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "gpu/gpu_export.h"

namespace base {
class CommandLine;
}

namespace gpu {

struct GPUInfo;

// With provided GPUInfo, compute the driver bug workarounds and disabled
// extensions for the current system, and append the |command_line|.
GPU_EXPORT void ApplyGpuDriverBugWorkarounds(
    const GPUInfo& gpu_info,
    base::CommandLine* command_line);

// |str| is in the format of "feature1,feature2,...,featureN".
GPU_EXPORT void StringToFeatureSet(
    const std::string& str, std::set<int>* feature_set);

// With provided command line, fill gpu_info->secondary_gpus with parsed
// secondary vendor and device ids.
GPU_EXPORT void ParseSecondaryGpuDevicesFromCommandLine(
    const base::CommandLine& command_line,
    GPUInfo* gpu_info);

GPU_EXPORT void InitializeDualGpusIfSupported(
    const std::set<int>& driver_bug_workarounds);

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_UTIL_H_

