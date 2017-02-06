// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/media/shared_memory_chunk.h"

#include <utility>

#include "base/memory/shared_memory.h"

namespace chromecast {
namespace media {

SharedMemoryChunk::SharedMemoryChunk(
    std::unique_ptr<base::SharedMemory> shared_mem,
    size_t size)
    : shared_mem_(std::move(shared_mem)), size_(size) {}

SharedMemoryChunk::~SharedMemoryChunk() {
}

void* SharedMemoryChunk::data() const {
  return shared_mem_->memory();
}

size_t SharedMemoryChunk::size() const {
  return size_;
}

bool SharedMemoryChunk::valid() const {
  return true;
}

}  // namespace media
}  // namespace chromecast

