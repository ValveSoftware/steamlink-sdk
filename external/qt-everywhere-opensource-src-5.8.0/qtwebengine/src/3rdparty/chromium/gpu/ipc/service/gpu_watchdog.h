// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_WATCHDOG_H_
#define GPU_IPC_SERVICE_GPU_WATCHDOG_H_

#include "base/macros.h"

namespace gpu {

// Interface for objects that monitor the a GPUProcessor's progress. The
// GPUProcessor will regularly invoke CheckArmed.
class GpuWatchdog {
 public:
  virtual void CheckArmed() = 0;

 protected:
  GpuWatchdog() {}
  virtual ~GpuWatchdog() {};

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuWatchdog);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_WATCHDOG_H_
