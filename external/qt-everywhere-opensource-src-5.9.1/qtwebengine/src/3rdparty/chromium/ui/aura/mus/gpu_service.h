// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_GPU_SERVICE_H_
#define UI_AURA_MUS_GPU_SERVICE_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/public/interfaces/gpu_service.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/mus/mojo_gpu_memory_buffer_manager.h"

namespace service_manager {
class Connector;
}

namespace aura {

class AURA_EXPORT GpuService : public gpu::GpuChannelHostFactory,
                               public gpu::GpuChannelEstablishFactory {
 public:
  ~GpuService() override;

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_.get();
  }

  // The GpuService has to be initialized in the main thread before establishing
  // the gpu channel. If no |task_runner| is provided, then a new thread is
  // created and used.
  static std::unique_ptr<GpuService> Create(
      service_manager::Connector* connector,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner = nullptr);

  // gpu::GpuChannelEstablishFactory:
  void EstablishGpuChannel(
      const gpu::GpuChannelEstablishedCallback& callback) override;
  scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannelSync() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;

 private:
  friend struct base::DefaultSingletonTraits<GpuService>;

  GpuService(service_manager::Connector* connector,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  scoped_refptr<gpu::GpuChannelHost> GetGpuChannel();
  void EstablishGpuChannelOnMainThreadSyncLocked();
  void OnEstablishedGpuChannel(int client_id,
                               mojo::ScopedMessagePipeHandle channel_handle,
                               const gpu::GPUInfo& gpu_info);

  // gpu::GpuChannelHostFactory overrides:
  bool IsMainThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() override;
  std::unique_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t size) override;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  service_manager::Connector* connector_;
  base::WaitableEvent shutdown_event_;
  std::unique_ptr<base::Thread> io_thread_;
  std::unique_ptr<MojoGpuMemoryBufferManager> gpu_memory_buffer_manager_;

  ui::mojom::GpuServicePtr gpu_service_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_;
  std::vector<gpu::GpuChannelEstablishedCallback> establish_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(GpuService);
};

}  // namespace aura

#endif  // UI_AURA_MUS_GPU_SERVICE_H_
