// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {

GpuMemoryBufferHandle::GpuMemoryBufferHandle()
    : type(EMPTY_BUFFER),
      handle(base::SharedMemory::NULLHandle())
#if defined(OS_MACOSX)
      ,
      io_surface_id(0u)
#endif
#if defined(OS_ANDROID)
      ,
      native_buffer(NULL)
#endif
{
}

GpuMemoryBuffer::GpuMemoryBuffer() {}

GpuMemoryBuffer::~GpuMemoryBuffer() {}

}  // namespace gfx
