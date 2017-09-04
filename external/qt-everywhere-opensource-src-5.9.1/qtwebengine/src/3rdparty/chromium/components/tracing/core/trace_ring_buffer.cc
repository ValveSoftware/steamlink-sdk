// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/trace_ring_buffer.h"

#include "base/threading/platform_thread.h"

namespace tracing {
namespace v2 {

TraceRingBuffer::TraceRingBuffer(uint8_t* begin, size_t size)
    : num_chunks_(size / kChunkSize),
      num_chunks_taken_(0),
      current_chunk_idx_(0) {
  DCHECK_GT(num_chunks_, 0u);
  DCHECK_EQ(0ul, reinterpret_cast<uintptr_t>(begin) % sizeof(uintptr_t));
  chunks_.reset(new Chunk[num_chunks_]);
  uint8_t* chunk_begin = begin;
  for (size_t i = 0; i < num_chunks_; ++i) {
    chunks_[i].Initialize(chunk_begin);
    chunk_begin += kChunkSize;
  }
}

TraceRingBuffer::~TraceRingBuffer() {}

TraceRingBuffer::Chunk* TraceRingBuffer::TakeChunk(uint32_t writer_id) {
  base::AutoLock lock(lock_);
  DCHECK_GT(num_chunks_, 0ul);
  DCHECK_LT(current_chunk_idx_, num_chunks_);
  for (size_t i = 0; i < num_chunks_; ++i) {
    Chunk* chunk = &chunks_[current_chunk_idx_];
    current_chunk_idx_ = (current_chunk_idx_ + 1) % num_chunks_;
    if (!chunk->is_owned()) {
      chunk->Clear();
      DCHECK_NE(0u, writer_id);
      chunk->set_owner(writer_id);
      num_chunks_taken_++;
      return chunk;
    }
  }

  // Bankrupcy: there are more threads than chunks. All chunks were in flight.
  if (!bankrupcy_chunk_storage_) {
    bankrupcy_chunk_storage_.reset(new uint8_t[kChunkSize]);
    bankrupcy_chunk_.Initialize(&bankrupcy_chunk_storage_.get()[0]);
  }
  bankrupcy_chunk_.Clear();
  return &bankrupcy_chunk_;
}

void TraceRingBuffer::ReturnChunk(TraceRingBuffer::Chunk* chunk) {
  // Returning a chunk without using it is quite odd, very likely a bug.
  DCHECK_GT(chunk->used_size(), 0u);

  // The caller should never return chunks which are part of a retaining chain.
  DCHECK(!chunk->next_in_owner_list());

  if (chunk == &bankrupcy_chunk_)
    return;

  // TODO(primiano): this lock might be removed in favor of acquire/release
  // semantics on the two vars below. Check once the reader of these are landed.
  base::AutoLock lock(lock_);
  chunk->clear_owner();
  num_chunks_taken_--;
}

bool TraceRingBuffer::IsBankrupcyChunkForTesting(const Chunk* chunk) const {
  return chunk == &bankrupcy_chunk_;
}

size_t TraceRingBuffer::GetNumChunksTaken() const {
  base::AutoLock lock(lock_);
  return num_chunks_taken_;
}

TraceRingBuffer::Chunk::Chunk()
    : begin_(nullptr), owner_(kNoChunkOwner), next_in_owner_list_(nullptr) {}

TraceRingBuffer::Chunk::~Chunk() {}

void TraceRingBuffer::Chunk::Clear() {
  set_used_size(0);
  set_next_in_owner_list(nullptr);
}

void TraceRingBuffer::Chunk::Initialize(uint8_t* begin) {
  DCHECK_EQ(0ul, reinterpret_cast<uintptr_t>(begin) % sizeof(uintptr_t));
  begin_ = begin;
}

}  // namespace v2
}  // namespace tracing
