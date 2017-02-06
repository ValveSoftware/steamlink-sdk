// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/trace_ring_buffer.h"

#include "base/threading/platform_thread.h"

namespace tracing {
namespace v2 {

TraceRingBuffer::TraceRingBuffer(uint8_t* begin, size_t size)
    : num_chunks_(size / Chunk::kSize), current_chunk_idx_(0) {
  DCHECK_GT(num_chunks_, 0u);
  DCHECK_EQ(0ul, reinterpret_cast<uintptr_t>(begin) % sizeof(uintptr_t));
  chunks_.reset(new Chunk[num_chunks_]);
  uint8_t* chunk_begin = begin;
  for (size_t i = 0; i < num_chunks_; ++i) {
    chunks_[i].Initialize(chunk_begin);
    chunk_begin += Chunk::kSize;
  }
}

TraceRingBuffer::~TraceRingBuffer() {}

TraceRingBuffer::Chunk* TraceRingBuffer::TakeChunk() {
  base::AutoLock lock(lock_);
  DCHECK_GT(num_chunks_, 0ul);
  DCHECK_LT(current_chunk_idx_, num_chunks_);
  for (size_t i = 0; i < num_chunks_; ++i) {
    Chunk* chunk = &chunks_[current_chunk_idx_];
    current_chunk_idx_ = (current_chunk_idx_ + 1) % num_chunks_;
    if (!chunk->is_owned()) {
      chunk->Clear();
      chunk->set_owner(base::PlatformThread::CurrentId());
      return chunk;
    }
  }

  // Bankrupcy: there are more threads than chunks. All chunks were in flight.
  if (!bankrupcy_chunk_storage_) {
    bankrupcy_chunk_storage_.reset(new uint8_t[Chunk::kSize]);
    bankrupcy_chunk_.Initialize(&bankrupcy_chunk_storage_.get()[0]);
  }
  bankrupcy_chunk_.Clear();
  return &bankrupcy_chunk_;
}

void TraceRingBuffer::ReturnChunk(TraceRingBuffer::Chunk* chunk,
                                  uint32_t used_size) {
  base::AutoLock lock(lock_);
  chunk->set_used_size(used_size);
  chunk->clear_owner();
}

TraceRingBuffer::Chunk::Chunk()
    : begin_(nullptr), owner_(base::kInvalidThreadId) {}

TraceRingBuffer::Chunk::~Chunk() {}

void TraceRingBuffer::Chunk::Clear() {
  set_used_size(0);
}

void TraceRingBuffer::Chunk::Initialize(uint8_t* begin) {
  DCHECK_EQ(0ul, reinterpret_cast<uintptr_t>(begin) % sizeof(uintptr_t));
  begin_ = begin;
}

}  // namespace v2
}  // namespace tracing
