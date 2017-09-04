// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_INIT_H_
#define GPU_IPC_SERVICE_GPU_INIT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"

namespace base {
class CommandLine;
}

namespace gpu {

class GPU_EXPORT GpuSandboxHelper {
 public:
  virtual ~GpuSandboxHelper() {}

  virtual void PreSandboxStartup() = 0;

  virtual bool EnsureSandboxInitialized(GpuWatchdogThread* watchdog_thread) = 0;
};

class GPU_EXPORT GpuInit {
 public:
  GpuInit();
  ~GpuInit();

  void set_sandbox_helper(GpuSandboxHelper* helper) {
    sandbox_helper_ = helper;
  }

  bool InitializeAndStartSandbox(const base::CommandLine& command_line);

  const GPUInfo& gpu_info() const { return gpu_info_; }
  std::unique_ptr<GpuWatchdogThread> TakeWatchdogThread() {
    return std::move(watchdog_thread_);
  }

 private:
  GpuSandboxHelper* sandbox_helper_ = nullptr;
  std::unique_ptr<GpuWatchdogThread> watchdog_thread_;
  GPUInfo gpu_info_;

  DISALLOW_COPY_AND_ASSIGN(GpuInit);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_INIT_H_
