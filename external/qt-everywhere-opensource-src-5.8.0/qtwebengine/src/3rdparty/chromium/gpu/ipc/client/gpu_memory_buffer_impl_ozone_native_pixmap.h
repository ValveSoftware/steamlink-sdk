// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_GPU_MEMORY_BUFFER_IMPL_OZONE_NATIVE_PIXMAP_H_
#define GPU_IPC_CLIENT_GPU_MEMORY_BUFFER_IMPL_OZONE_NATIVE_PIXMAP_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"

namespace ui {
class ClientNativePixmap;
}

namespace gpu {

// Implementation of GPU memory buffer based on Ozone native pixmap.
class GPU_EXPORT GpuMemoryBufferImplOzoneNativePixmap
    : public GpuMemoryBufferImpl {
 public:
  ~GpuMemoryBufferImplOzoneNativePixmap() override;

  static std::unique_ptr<GpuMemoryBufferImplOzoneNativePixmap> CreateFromHandle(
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
  GpuMemoryBufferImplOzoneNativePixmap(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      const DestructionCallback& callback,
      std::unique_ptr<ui::ClientNativePixmap> native_pixmap,
      const std::vector<std::pair<int, int>>& strides_and_offsets,
      base::ScopedFD fd);

  std::unique_ptr<ui::ClientNativePixmap> pixmap_;
  std::vector<std::pair<int, int>> strides_and_offsets_;
  base::ScopedFD fd_;
  void* data_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplOzoneNativePixmap);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_GPU_MEMORY_BUFFER_IMPL_OZONE_NATIVE_PIXMAP_H_
