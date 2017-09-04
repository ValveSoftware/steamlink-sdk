// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_MOJO_GPU_MEMORY_BUFFER_MANAGER_H_
#define SERVICES_UI_PUBLIC_CPP_MOJO_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "services/ui/public/interfaces/gpu_service.mojom.h"

namespace base {
class WaitableEvent;
}

namespace service_manager {
class Connector;
}

namespace ui {

namespace mojom {
class GpuService;
}

class MojoGpuMemoryBufferManager : public gpu::GpuMemoryBufferManager {
 public:
  explicit MojoGpuMemoryBufferManager(service_manager::Connector* connector);
  ~MojoGpuMemoryBufferManager() override;

 private:
  void InitThread();
  void TearDownThread();
  void AllocateGpuMemoryBufferOnThread(const gfx::Size& size,
                                       gfx::BufferFormat format,
                                       gfx::BufferUsage usage,
                                       gfx::GpuMemoryBufferHandle* handle,
                                       base::WaitableEvent* wait);
  void DeletedGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              const gpu::SyncToken& sync_token);

  // Overridden from gpu::GpuMemoryBufferManager:
  std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle) override;
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBufferFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format) override;
  void SetDestructionSyncToken(gfx::GpuMemoryBuffer* buffer,
                               const gpu::SyncToken& sync_token) override;
  int counter_ = 0;
  // TODO(sad): Explore the option of doing this from an existing thread.
  base::Thread thread_;
  mojom::GpuServicePtr gpu_service_;
  std::unique_ptr<service_manager::Connector> connector_;
  base::WeakPtrFactory<MojoGpuMemoryBufferManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoGpuMemoryBufferManager);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_MOJO_GPU_MEMORY_BUFFER_MANAGER_H_
