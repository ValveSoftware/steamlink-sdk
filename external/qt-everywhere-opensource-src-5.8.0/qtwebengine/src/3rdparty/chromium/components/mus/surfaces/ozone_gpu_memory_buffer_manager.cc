// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/ozone_gpu_memory_buffer_manager.h"

#include "components/mus/gles2/ozone_gpu_memory_buffer.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_types.h"

namespace mus {

OzoneGpuMemoryBufferManager::OzoneGpuMemoryBufferManager() {}

OzoneGpuMemoryBufferManager::~OzoneGpuMemoryBufferManager() {}

std::unique_ptr<gfx::GpuMemoryBuffer>
OzoneGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  return OzoneGpuMemoryBuffer::CreateOzoneGpuMemoryBuffer(
      size, format, gfx::BufferUsage::SCANOUT, surface_handle);
}

std::unique_ptr<gfx::GpuMemoryBuffer>
OzoneGpuMemoryBufferManager::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format) {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::GpuMemoryBuffer*
OzoneGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  NOTIMPLEMENTED();
  return nullptr;
}

void OzoneGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  NOTIMPLEMENTED();
}

}  // namespace mus
