// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_OZONE_GPU_MEMORY_BUFFER_
#define COMPONENTS_MUS_GLES2_OZONE_GPU_MEMORY_BUFFER_

#include <memory>

#include "components/mus/common/gpu_memory_buffer_impl.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class ClientNativePixmap;
class NativePixmap;
}  // namespace ui

namespace mus {

// A not-mojo GpuMemoryBuffer implementation solely for use internally to mus
// for scanout buffers. Note that OzoneGpuMemoryBuffer is for use on the client
// (aka CC thread).
class OzoneGpuMemoryBuffer : public mus::GpuMemoryBufferImpl {
 public:
  ~OzoneGpuMemoryBuffer() override;

  // gfx::GpuMemoryBuffer implementation
  bool Map() override;
  void Unmap() override;
  void* memory(size_t plane) override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;

  // Returns the type of this GpuMemoryBufferImpl.
  gfx::GpuMemoryBufferType GetBufferType() const override;
  scoped_refptr<ui::NativePixmap> GetNativePixmap() override;

  // Create a NativeBuffer. The implementation (mus-specific) will call directly
  // into ozone to allocate the buffer. See the version in the .cc file.
  static std::unique_ptr<gfx::GpuMemoryBuffer> CreateOzoneGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gfx::AcceleratedWidget widget);

  // Cribbed from content/common/gpu/client/gpu_memory_buffer_impl.h
  static OzoneGpuMemoryBuffer* FromClientBuffer(ClientBuffer buffer);

 private:
  // TODO(rjkroege): It is conceivable that we do not need an |id| here. It
  // would seem to be a legacy of content leaking into gfx. In a mojo context,
  // perhaps it should be a mojo handle instead.
  OzoneGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                       const gfx::Size& size,
                       gfx::BufferFormat format,
                       std::unique_ptr<ui::ClientNativePixmap> client_pixmap,
                       scoped_refptr<ui::NativePixmap> native_pixmap);

  // The real backing buffer.
  // From content/common/gpu/client/gpu_memory_buffer_impl_ozone_native_pixmap.h
  std::unique_ptr<ui::ClientNativePixmap> client_pixmap_;
  scoped_refptr<ui::NativePixmap> native_pixmap_;

  DISALLOW_COPY_AND_ASSIGN(OzoneGpuMemoryBuffer);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_OZONE_GPU_MEMORY_BUFFER_
