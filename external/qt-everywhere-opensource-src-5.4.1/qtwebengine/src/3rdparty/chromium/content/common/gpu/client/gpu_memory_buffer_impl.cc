// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

#include "ui/gl/gl_bindings.h"

namespace content {

GpuMemoryBufferImpl::GpuMemoryBufferImpl(const gfx::Size& size,
                                         unsigned internalformat)
    : size_(size), internalformat_(internalformat), mapped_(false) {
  DCHECK(IsFormatValid(internalformat));
}

GpuMemoryBufferImpl::~GpuMemoryBufferImpl() {}

// static
bool GpuMemoryBufferImpl::IsFormatValid(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return true;
    default:
      return false;
  }
}

// static
bool GpuMemoryBufferImpl::IsUsageValid(unsigned usage) {
  switch (usage) {
    case GL_IMAGE_MAP_CHROMIUM:
    case GL_IMAGE_SCANOUT_CHROMIUM:
      return true;
    default:
      return false;
  }
}

// static
size_t GpuMemoryBufferImpl::BytesPerPixel(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return 4;
    default:
      NOTREACHED();
      return 0;
  }
}

bool GpuMemoryBufferImpl::IsMapped() const { return mapped_; }

}  // namespace content
