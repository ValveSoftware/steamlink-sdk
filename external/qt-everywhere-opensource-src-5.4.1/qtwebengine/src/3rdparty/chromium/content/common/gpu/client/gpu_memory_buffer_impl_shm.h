// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SHM_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SHM_H_

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

namespace content {

// Implementation of GPU memory buffer based on shared memory.
class GpuMemoryBufferImplShm : public GpuMemoryBufferImpl {
 public:
  GpuMemoryBufferImplShm(const gfx::Size& size, unsigned internalformat);
  virtual ~GpuMemoryBufferImplShm();

  // Allocates a shared memory backed GPU memory buffer with |size| and
  // |internalformat| for use by |child_process|.
  static void AllocateSharedMemoryForChildProcess(
      const gfx::Size& size,
      unsigned internalformat,
      base::ProcessHandle child_process,
      const AllocationCallback& callback);

  static bool IsLayoutSupported(const gfx::Size& size, unsigned internalformat);
  static bool IsUsageSupported(unsigned usage);
  static bool IsConfigurationSupported(const gfx::Size& size,
                                       unsigned internalformat,
                                       unsigned usage);

  bool Initialize();
  bool InitializeFromHandle(gfx::GpuMemoryBufferHandle handle);

  // Overridden from gfx::GpuMemoryBuffer:
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual uint32 GetStride() const OVERRIDE;
  virtual gfx::GpuMemoryBufferHandle GetHandle() const OVERRIDE;

 private:
  scoped_ptr<base::SharedMemory> shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplShm);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SHM_H_
