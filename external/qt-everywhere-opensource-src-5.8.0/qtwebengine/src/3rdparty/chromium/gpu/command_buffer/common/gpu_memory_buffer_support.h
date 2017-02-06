// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
#define GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_

#include "gpu/gpu_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

struct Capabilities;

// Returns a valid GpuMemoryBuffer format given a valid internalformat as
// defined by CHROMIUM_gpu_memory_buffer_image.
GPU_EXPORT gfx::BufferFormat DefaultBufferFormatForImageFormat(
    unsigned internalformat);

// Returns true if |internalformat| is compatible with |format|.
GPU_EXPORT bool IsImageFormatCompatibleWithGpuMemoryBufferFormat(
    unsigned internalformat,
    gfx::BufferFormat format);

// Returns true if |format| is supported by |capabilities|.
GPU_EXPORT bool IsGpuMemoryBufferFormatSupported(
    gfx::BufferFormat format,
    const Capabilities& capabilities);

// Returns true if |size| is valid for |format|.
GPU_EXPORT bool IsImageSizeValidForGpuMemoryBufferFormat(
    const gfx::Size& size,
    gfx::BufferFormat format);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
