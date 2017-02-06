// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_SWITCHES_H_
#define GPU_CONFIG_GPU_SWITCHES_H_

#include "gpu/gpu_export.h"

namespace switches {

GPU_EXPORT extern const char kGpuDriverBugWorkarounds[];
GPU_EXPORT extern const char kGpuActiveVendorID[];
GPU_EXPORT extern const char kGpuActiveDeviceID[];
GPU_EXPORT extern const char kGpuSecondaryVendorIDs[];
GPU_EXPORT extern const char kGpuSecondaryDeviceIDs[];
GPU_EXPORT extern const char kGpuTestingNoCompleteInfoCollection[];
GPU_EXPORT extern const char kGpuTestingOsVersion[];
GPU_EXPORT extern const char kGpuTestingVendorId[];
GPU_EXPORT extern const char kGpuTestingDeviceId[];
GPU_EXPORT extern const char kGpuTestingSecondaryVendorIDs[];
GPU_EXPORT extern const char kGpuTestingSecondaryDeviceIDs[];
GPU_EXPORT extern const char kGpuTestingDriverDate[];
GPU_EXPORT extern const char kGpuTestingGLVendor[];
GPU_EXPORT extern const char kGpuTestingGLRenderer[];
GPU_EXPORT extern const char kGpuTestingGLVersion[];

}  // namespace switches

#endif  // GPU_CONFIG_GPU_SWITCHES_H_
