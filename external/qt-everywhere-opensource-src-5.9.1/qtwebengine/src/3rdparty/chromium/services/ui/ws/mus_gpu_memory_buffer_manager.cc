// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/mus_gpu_memory_buffer_manager.h"

#include "base/logging.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "services/ui/common/generic_shared_memory_id_generator.h"
#include "services/ui/gpu/interfaces/gpu_service_internal.mojom.h"

namespace ui {

namespace ws {

MusGpuMemoryBufferManager::MusGpuMemoryBufferManager(
    mojom::GpuServiceInternal* gpu_service,
    int client_id)
    : gpu_service_(gpu_service), client_id_(client_id), weak_factory_(this) {}

MusGpuMemoryBufferManager::~MusGpuMemoryBufferManager() {}

std::unique_ptr<gfx::GpuMemoryBuffer>
MusGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  gfx::GpuMemoryBufferId id = GetNextGenericSharedMemoryId();
  const bool is_native =
      gpu::IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
  if (is_native) {
    gfx::GpuMemoryBufferHandle handle;
    gpu_service_->CreateGpuMemoryBuffer(id, size, format, usage, client_id_,
                                        surface_handle, &handle);
    if (handle.is_null())
      return nullptr;
    return gpu::GpuMemoryBufferImpl::CreateFromHandle(
        handle, size, format, usage,
        base::Bind(&MusGpuMemoryBufferManager::DestroyGpuMemoryBuffer,
                   weak_factory_.GetWeakPtr(), id, client_id_, is_native));
  }

  DCHECK(gpu::GpuMemoryBufferImplSharedMemory::IsUsageSupported(usage))
      << static_cast<int>(usage);
  return gpu::GpuMemoryBufferImplSharedMemory::Create(
      id, size, format,
      base::Bind(&MusGpuMemoryBufferManager::DestroyGpuMemoryBuffer,
                 weak_factory_.GetWeakPtr(), id, client_id_, is_native));
}

std::unique_ptr<gfx::GpuMemoryBuffer>
MusGpuMemoryBufferManager::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format) {
  NOTIMPLEMENTED();
  return nullptr;
}

void MusGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  static_cast<gpu::GpuMemoryBufferImpl*>(buffer)->set_destruction_sync_token(
      sync_token);
}

void MusGpuMemoryBufferManager::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    bool is_native,
    const gpu::SyncToken& sync_token) {
  if (is_native) {
    gpu_service_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
  }
}

}  // namespace ws
}  // namespace ui
