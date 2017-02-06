// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_switches.h"

namespace switches {

// Pass a set of GpuDriverBugWorkaroundType ids, seperated by ','.
const char kGpuDriverBugWorkarounds[] = "gpu-driver-bug-workarounds";

// Passes active gpu vendor id from browser process to GPU process.
const char kGpuActiveVendorID[] = "gpu-active-vendor-id";

// Passes active gpu device id from browser process to GPU process.
const char kGpuActiveDeviceID[] = "gpu-active-device-id";

// Passes secondary gpu vendor ids from browser process to GPU process.
const char kGpuSecondaryVendorIDs[] = "gpu-secondary-vendor-ids";

// Passes secondary gpu device ids from browser process to GPU process.
const char kGpuSecondaryDeviceIDs[] = "gpu-secondary-device-ids";

// Testing switch to not launch the gpu process for full gpu info collection.
const char kGpuTestingNoCompleteInfoCollection[] =
    "gpu-no-complete-info-collection";

// Override os version from GpuControlList::MakeDecision.
const char kGpuTestingOsVersion[] = "gpu-testing-os-version";

// Override gpu vendor id from the GpuInfoCollector.
const char kGpuTestingVendorId[] = "gpu-testing-vendor-id";

// Override gpu device id from the GpuInfoCollector.
const char kGpuTestingDeviceId[] = "gpu-testing-device-id";

// Override secondary gpu vendor ids from the GpuInfoCollector.
const char kGpuTestingSecondaryVendorIDs[] = "gpu-testing-secondary-vendor-ids";

// Override secondary gpu device ids from the GpuInfoCollector.
const char kGpuTestingSecondaryDeviceIDs[] = "gpu-testing-secondary-device-ids";

// Override gpu driver date from the GpuInfoCollector.
const char kGpuTestingDriverDate[] = "gpu-testing-driver-date";

// Override gl vendor from the GpuInfoCollector.
const char kGpuTestingGLVendor[] = "gpu-testing-gl-vendor";

// Override gl renderer from the GpuInfoCollector.
const char kGpuTestingGLRenderer[] = "gpu-testing-gl-renderer";

// Override gl version from the GpuInfoCollector.
const char kGpuTestingGLVersion[] = "gpu-testing-gl-version";

}  // namespace switches
