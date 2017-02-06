// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_
#define GPU_IPC_CLIENT_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_

#include <IOSurface/IOSurface.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"

namespace gpu {

// Implementation of GPU memory buffer based on IO surfaces.
class GPU_EXPORT GpuMemoryBufferImplIOSurface : public GpuMemoryBufferImpl {
 public:
  ~GpuMemoryBufferImplIOSurface() override;

  static std::unique_ptr<GpuMemoryBufferImplIOSurface> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const DestructionCallback& callback);

  static bool IsConfigurationSupported(gfx::BufferFormat format,
                                       gfx::BufferUsage usage);

  static base::Closure AllocateForTesting(const gfx::Size& size,
                                          gfx::BufferFormat format,
                                          gfx::BufferUsage usage,
                                          gfx::GpuMemoryBufferHandle* handle);

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;

 private:
  GpuMemoryBufferImplIOSurface(gfx::GpuMemoryBufferId id,
                               const gfx::Size& size,
                               gfx::BufferFormat format,
                               const DestructionCallback& callback,
                               IOSurfaceRef io_surface,
                               uint32_t lock_flags);

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  uint32_t lock_flags_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplIOSurface);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_
