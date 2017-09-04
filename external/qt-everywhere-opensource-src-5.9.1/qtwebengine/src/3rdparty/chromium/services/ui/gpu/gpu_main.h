// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_GPU_GPU_MAIN_H_
#define SERVICES_UI_GPU_GPU_MAIN_H_

#include "base/threading/thread.h"
#include "gpu/ipc/service/gpu_init.h"
#include "services/ui/gpu/interfaces/gpu_service_internal.mojom.h"

namespace gpu {
class GpuMemoryBufferFactory;
}

namespace ui {

class GpuServiceInternal;

class GpuMain : public gpu::GpuSandboxHelper {
 public:
  GpuMain();
  ~GpuMain() override;

  void OnStart();
  void Create(mojom::GpuServiceInternalRequest request);

  GpuServiceInternal* gpu_service() { return gpu_service_internal_.get(); }

 private:
  void InitOnGpuThread();
  void TearDownOnGpuThread();
  void CreateOnGpuThread(mojom::GpuServiceInternalRequest request);

  // gpu::GpuSandboxHelper:
  void PreSandboxStartup() override;
  bool EnsureSandboxInitialized(
      gpu::GpuWatchdogThread* watchdog_thread) override;

  std::unique_ptr<gpu::GpuInit> gpu_init_;
  std::unique_ptr<GpuServiceInternal> gpu_service_internal_;
  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

  // The main thread for GpuService.
  base::Thread gpu_thread_;

  // The thread that handles IO events for GpuService.
  base::Thread io_thread_;

  base::WeakPtrFactory<GpuMain> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuMain);
};

}  // namespace ui

#endif  // SERVICES_UI_GPU_GPU_MAIN_H_
