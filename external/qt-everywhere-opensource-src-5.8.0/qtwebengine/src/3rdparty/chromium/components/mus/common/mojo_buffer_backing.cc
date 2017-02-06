// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/common/mojo_buffer_backing.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace mus {

MojoBufferBacking::MojoBufferBacking(mojo::ScopedSharedBufferMapping mapping,
                                     size_t size)
    : mapping_(std::move(mapping)), size_(size) {}

MojoBufferBacking::~MojoBufferBacking() = default;

// static
std::unique_ptr<gpu::BufferBacking> MojoBufferBacking::Create(
    mojo::ScopedSharedBufferHandle handle,
    size_t size) {
  mojo::ScopedSharedBufferMapping mapping = handle->Map(size);
  if (!mapping)
    return nullptr;
  return base::MakeUnique<MojoBufferBacking>(std::move(mapping), size);
}
void* MojoBufferBacking::GetMemory() const {
  return mapping_.get();
}
size_t MojoBufferBacking::GetSize() const {
  return size_;
}

}  // namespace mus
