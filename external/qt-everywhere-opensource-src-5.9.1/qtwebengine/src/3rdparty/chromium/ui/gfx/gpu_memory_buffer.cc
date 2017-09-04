// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {

GpuMemoryBufferHandle::GpuMemoryBufferHandle()
    : type(EMPTY_BUFFER), id(0), handle(base::SharedMemory::NULLHandle()) {
}

GpuMemoryBufferHandle::GpuMemoryBufferHandle(
    const GpuMemoryBufferHandle& other) = default;

GpuMemoryBufferHandle::~GpuMemoryBufferHandle() {}

void GpuMemoryBuffer::SetColorSpaceForScanout(
    const gfx::ColorSpace& color_space) {}

}  // namespace gfx
