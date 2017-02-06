// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/gpu_service_registry.h"

#include "content/browser/gpu/gpu_process_host.h"

namespace content {

shell::InterfaceProvider* GetGpuRemoteInterfaces() {
  GpuProcessHost* host =
      GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                          CAUSE_FOR_GPU_LAUNCH_GET_GPU_SERVICE_REGISTRY);
  return host->GetRemoteInterfaces();
}

}  // namespace content
