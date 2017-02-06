// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_COMMON_MOJO_GPU_MEMORY_BUFFER_H_
#define COMPONENTS_MUS_COMMON_MOJO_GPU_MEMORY_BUFFER_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "components/mus/common/gpu_memory_buffer_impl.h"
#include "components/mus/common/mus_common_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace mus {

class MUS_COMMON_EXPORT MojoGpuMemoryBufferImpl
    : public mus::GpuMemoryBufferImpl {
 public:
  MojoGpuMemoryBufferImpl(const gfx::Size& size,
                          gfx::BufferFormat format,
                          std::unique_ptr<base::SharedMemory> shared_memory);
  ~MojoGpuMemoryBufferImpl() override;

  static std::unique_ptr<gfx::GpuMemoryBuffer> Create(const gfx::Size& size,
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
  std::unique_ptr<base::SharedMemory> shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(MojoGpuMemoryBufferImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_COMMON_MOJO_GPU_MEMORY_BUFFER_H_
