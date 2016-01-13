// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/size.h"

namespace content {

// Provides common implementation of a GPU memory buffer.
class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  typedef base::Callback<void(const gfx::GpuMemoryBufferHandle& handle)>
      AllocationCallback;

  virtual ~GpuMemoryBufferImpl();

  // Creates a GPU memory buffer instance with |size| and |internalformat| for
  // |usage|.
  static scoped_ptr<GpuMemoryBufferImpl> Create(const gfx::Size& size,
                                                unsigned internalformat,
                                                unsigned usage);

  // Allocates a GPU memory buffer with |size| and |internalformat| for |usage|
  // by |child_process|. The |handle| returned can be used by the
  // |child_process| to create an instance of this class.
  static void AllocateForChildProcess(const gfx::Size& size,
                                      unsigned internalformat,
                                      unsigned usage,
                                      base::ProcessHandle child_process,
                                      const AllocationCallback& callback);

  // Creates an instance from the given |handle|. |size| and |internalformat|
  // should match what was used to allocate the |handle|.
  static scoped_ptr<GpuMemoryBufferImpl> CreateFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      unsigned internalformat);

  // Returns true if |internalformat| is a format recognized by this base class.
  static bool IsFormatValid(unsigned internalformat);

  // Returns true if |usage| is recognized by this base class.
  static bool IsUsageValid(unsigned usage);

  // Returns the number of bytes per pixel that must be used by an
  // implementation when using |internalformat|.
  static size_t BytesPerPixel(unsigned internalformat);

  // Overridden from gfx::GpuMemoryBuffer:
  virtual bool IsMapped() const OVERRIDE;

 protected:
  GpuMemoryBufferImpl(const gfx::Size& size, unsigned internalformat);

  const gfx::Size size_;
  const unsigned internalformat_;
  bool mapped_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_
