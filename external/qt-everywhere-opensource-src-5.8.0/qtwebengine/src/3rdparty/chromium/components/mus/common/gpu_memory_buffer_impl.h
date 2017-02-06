// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_COMMON_GPU_MEMORY_BUFFER_IMPL_H_
#define COMPONENTS_MUS_COMMON_GPU_MEMORY_BUFFER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "components/mus/common/mus_common_export.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/native_pixmap.h"
#endif

namespace mus {

// Provides common implementation of a GPU memory buffer.
class MUS_COMMON_EXPORT GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  ~GpuMemoryBufferImpl() override;

  // Type-checking upcast routine. Returns an NULL on failure.
  static GpuMemoryBufferImpl* FromClientBuffer(ClientBuffer buffer);

  // Overridden from gfx::GpuMemoryBuffer:
  gfx::Size GetSize() const override;
  gfx::BufferFormat GetFormat() const override;
  gfx::GpuMemoryBufferId GetId() const override;
  ClientBuffer AsClientBuffer() override;

  // Returns the type of this GpuMemoryBufferImpl.
  virtual gfx::GpuMemoryBufferType GetBufferType() const = 0;

#if defined(USE_OZONE)
  // Returns a ui::NativePixmap when one is available.
  virtual scoped_refptr<ui::NativePixmap> GetNativePixmap();
#endif

 protected:
  GpuMemoryBufferImpl(gfx::GpuMemoryBufferId id,
                      const gfx::Size& size,
                      gfx::BufferFormat format);

  const gfx::GpuMemoryBufferId id_;
  const gfx::Size size_;
  const gfx::BufferFormat format_;
  bool mapped_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_COMMON_GPU_MEMORY_BUFFER_IMPL_H_
