// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/gles2/mojo_buffer_backing.h"

#include "base/logging.h"

namespace mojo {
namespace gles2 {

MojoBufferBacking::MojoBufferBacking(mojo::ScopedSharedBufferHandle handle,
                                     void* memory,
                                     size_t size)
    : handle_(handle.Pass()), memory_(memory), size_(size) {}

MojoBufferBacking::~MojoBufferBacking() { mojo::UnmapBuffer(memory_); }

// static
scoped_ptr<gpu::BufferBacking> MojoBufferBacking::Create(
    mojo::ScopedSharedBufferHandle handle,
    size_t size) {
  void* memory = NULL;
  MojoResult result = mojo::MapBuffer(
      handle.get(), 0, size, &memory, MOJO_MAP_BUFFER_FLAG_NONE);
  if (result != MOJO_RESULT_OK)
    return scoped_ptr<BufferBacking>();
  DCHECK(memory);
  return scoped_ptr<BufferBacking>(
      new MojoBufferBacking(handle.Pass(), memory, size));
}
void* MojoBufferBacking::GetMemory() const { return memory_; }
size_t MojoBufferBacking::GetSize() const { return size_; }

}  // namespace gles2
}  // namespace mojo
