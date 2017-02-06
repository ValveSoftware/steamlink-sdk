// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gpu/mus_gpu_memory_buffer_manager.h"

#include "base/logging.h"
#include "components/mus/common/generic_shared_memory_id_generator.h"
#include "components/mus/gpu/gpu_service_mus.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"

namespace mus {

namespace {

MusGpuMemoryBufferManager* g_gpu_memory_buffer_manager = nullptr;

bool IsNativeGpuMemoryBufferFactoryConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  switch (gpu::GetNativeGpuMemoryBufferType()) {
    case gfx::SHARED_MEMORY_BUFFER:
      return false;
    case gfx::IO_SURFACE_BUFFER:
    case gfx::SURFACE_TEXTURE_BUFFER:
    case gfx::OZONE_NATIVE_PIXMAP:
      return gpu::IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
    default:
      NOTREACHED();
      return false;
  }
}
}

MusGpuMemoryBufferManager* MusGpuMemoryBufferManager::current() {
  return g_gpu_memory_buffer_manager;
}

MusGpuMemoryBufferManager::MusGpuMemoryBufferManager(GpuServiceMus* gpu_service,
                                                     int client_id)
    : gpu_service_(gpu_service), client_id_(client_id), weak_factory_(this) {
  DCHECK(!g_gpu_memory_buffer_manager);
  g_gpu_memory_buffer_manager = this;
}

MusGpuMemoryBufferManager::~MusGpuMemoryBufferManager() {
  g_gpu_memory_buffer_manager = nullptr;
}

std::unique_ptr<gfx::GpuMemoryBuffer>
MusGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  gfx::GpuMemoryBufferId id = GetNextGenericSharedMemoryId();
  const bool is_native =
      IsNativeGpuMemoryBufferFactoryConfigurationSupported(format, usage);
  if (is_native) {
    gfx::GpuMemoryBufferHandle handle =
        gpu_service_->gpu_memory_buffer_factory()->CreateGpuMemoryBuffer(
            id, size, format, usage, client_id_, surface_handle);
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

gfx::GpuMemoryBuffer*
MusGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return gpu::GpuMemoryBufferImpl::FromClientBuffer(buffer);
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
    gpu_service_->gpu_channel_manager()->DestroyGpuMemoryBuffer(id, client_id,
                                                                sync_token);
  }
}

}  // namespace mus
