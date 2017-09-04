// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_CPP_MOJO_GPU_MEMORY_BUFFER_H_
#define UI_AURA_MUS_CPP_MOJO_GPU_MEMORY_BUFFER_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "ui/aura/mus/gpu_memory_buffer_impl.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace aura {

class MojoGpuMemoryBufferImpl : public GpuMemoryBufferImpl {
 public:
  ~MojoGpuMemoryBufferImpl() override;

  static std::unique_ptr<gfx::GpuMemoryBuffer> Create(const gfx::Size& size,
                                                      gfx::BufferFormat format,
                                                      gfx::BufferUsage usage);
  static std::unique_ptr<gfx::GpuMemoryBuffer> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage);
  static MojoGpuMemoryBufferImpl* FromClientBuffer(ClientBuffer buffer);

  const unsigned char* GetMemory() const;

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;

  // Overridden from gfx::GpuMemoryBufferImpl
  gfx::GpuMemoryBufferType GetBufferType() const override;

 private:
  MojoGpuMemoryBufferImpl(const gfx::Size& size,
                          gfx::BufferFormat format,
                          std::unique_ptr<base::SharedMemory> shared_memory,
                          uint32_t offset,
                          int32_t stride);

  std::unique_ptr<base::SharedMemory> shared_memory_;
  const uint32_t offset_;
  const int32_t stride_;

  DISALLOW_COPY_AND_ASSIGN(MojoGpuMemoryBufferImpl);
};

}  // namespace aura

#endif  // UI_AURA_MUS_CPP_MOJO_GPU_MEMORY_BUFFER_H_
