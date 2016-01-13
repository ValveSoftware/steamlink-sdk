// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GPU_MEMORY_BUFFER_H_
#define UI_GFX_GPU_MEMORY_BUFFER_H_

#include "base/memory/shared_memory.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"

#if defined(OS_ANDROID)
#include <third_party/khronos/EGL/egl.h>
#endif

namespace gfx {

enum GpuMemoryBufferType {
  EMPTY_BUFFER,
  SHARED_MEMORY_BUFFER,
  IO_SURFACE_BUFFER,
  ANDROID_NATIVE_BUFFER,
  SURFACE_TEXTURE_BUFFER,
  GPU_MEMORY_BUFFER_TYPE_LAST = SURFACE_TEXTURE_BUFFER
};

// TODO(alexst): Merge this with GpuMemoryBufferId as part of switchover to
// the new API for GpuMemoryBuffer allocation when it's done.
#if defined(OS_ANDROID)
struct SurfaceTextureId {
  SurfaceTextureId() : primary_id(0), secondary_id(0) {}
  SurfaceTextureId(int32 primary_id, int32 secondary_id)
      : primary_id(primary_id), secondary_id(secondary_id) {}
  int32 primary_id;
  int32 secondary_id;
};
#endif

struct GpuMemoryBufferId {
  GpuMemoryBufferId() : primary_id(0), secondary_id(0) {}
  GpuMemoryBufferId(int32 primary_id, int32 secondary_id)
      : primary_id(primary_id), secondary_id(secondary_id) {}
  int32 primary_id;
  int32 secondary_id;
};

struct GFX_EXPORT GpuMemoryBufferHandle {
  GpuMemoryBufferHandle();
  bool is_null() const { return type == EMPTY_BUFFER; }
  GpuMemoryBufferType type;
  base::SharedMemoryHandle handle;
  GpuMemoryBufferId global_id;
#if defined(OS_MACOSX)
  uint32 io_surface_id;
#endif
#if defined(OS_ANDROID)
  EGLClientBuffer native_buffer;
  SurfaceTextureId surface_texture_id;
#endif
};

// This interface typically correspond to a type of shared memory that is also
// shared with the GPU. A GPU memory buffer can be written to directly by
// regular CPU code, but can also be read by the GPU.
class GFX_EXPORT GpuMemoryBuffer {
 public:
  GpuMemoryBuffer();
  virtual ~GpuMemoryBuffer();

  // Maps the buffer into the client's address space so it can be written to by
  // the CPU. This call may block, for instance if the GPU needs to finish
  // accessing the buffer or if CPU caches need to be synchronized. Returns NULL
  // on failure.
  virtual void* Map() = 0;

  // Unmaps the buffer. It's illegal to use the pointer returned by Map() after
  // this has been called.
  virtual void Unmap() = 0;

  // Returns true iff the buffer is mapped.
  virtual bool IsMapped() const = 0;

  // Returns the stride in bytes for the buffer.
  virtual uint32 GetStride() const = 0;

  // Returns a platform specific handle for this buffer.
  virtual GpuMemoryBufferHandle GetHandle() const = 0;
};

}  // namespace gfx

#endif  // UI_GFX_GPU_MEMORY_BUFFER_H_
