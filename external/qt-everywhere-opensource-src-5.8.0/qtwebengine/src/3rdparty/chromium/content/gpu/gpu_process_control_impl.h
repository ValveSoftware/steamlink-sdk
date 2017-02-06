// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_PROCESS_CONTROL_IMPL_H_
#define CONTENT_GPU_GPU_PROCESS_CONTROL_IMPL_H_

#include "base/macros.h"
#include "content/child/process_control_impl.h"

namespace content {

// Customization of ProcessControlImpl for the GPU process.
class GpuProcessControlImpl : public ProcessControlImpl {
 public:
  GpuProcessControlImpl();
  ~GpuProcessControlImpl() override;

  // ProcessControlImpl:
  void RegisterApplications(ApplicationMap* apps) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuProcessControlImpl);
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_PROCESS_CONTROL_IMPL_H_
