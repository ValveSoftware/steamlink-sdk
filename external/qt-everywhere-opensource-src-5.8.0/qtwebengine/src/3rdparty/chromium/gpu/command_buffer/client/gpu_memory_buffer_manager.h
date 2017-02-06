// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>

#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

struct SyncToken;

class GPU_EXPORT GpuMemoryBufferManager {
 public:
  GpuMemoryBufferManager();

  // Allocates a GpuMemoryBuffer that can be shared with another process.
  virtual std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle) = 0;

  // Creates a GpuMemoryBuffer from existing handle.
  virtual std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBufferFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format) = 0;

  // Returns a GpuMemoryBuffer instance given a ClientBuffer. Returns NULL on
  // failure.
  virtual gfx::GpuMemoryBuffer* GpuMemoryBufferFromClientBuffer(
      ClientBuffer buffer) = 0;

  // Associates destruction sync point with |buffer|.
  virtual void SetDestructionSyncToken(gfx::GpuMemoryBuffer* buffer,
                                       const gpu::SyncToken& sync_token) = 0;

 protected:
  virtual ~GpuMemoryBufferManager();
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MANAGER_H_
