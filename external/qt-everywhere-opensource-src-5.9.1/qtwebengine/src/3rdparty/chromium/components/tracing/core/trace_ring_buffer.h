// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CORE_TRACE_RING_BUFFER_H_
#define COMPONENTS_TRACING_CORE_TRACE_RING_BUFFER_H_

#include <memory>

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "components/tracing/tracing_export.h"

namespace tracing {
namespace v2 {

static const uint32_t kNoChunkOwner = 0;
static const size_t kChunkSize = 32 * 1024;

class TRACING_EXPORT TraceRingBuffer {
 public:
  class Chunk {
   public:
    using Header = base::subtle::Atomic32;

    Chunk();
    ~Chunk();

    void Initialize(uint8_t* begin);
    void Clear();

    uint8_t* begin() const { return begin_; }
    Header* header() const { return reinterpret_cast<Header*>(begin_); }
    uint8_t* payload() const { return begin_ + sizeof(Header); }
    uint8_t* end() const { return begin_ + kChunkSize; }

    void set_used_size(uint32_t size) {
      base::subtle::NoBarrier_Store(header(), size);
    }
    uint32_t used_size() const {
      return base::subtle::NoBarrier_Load(header());
    }

    void set_next_in_owner_list(Chunk* next) { next_in_owner_list_ = next; }
    Chunk* next_in_owner_list() const { return next_in_owner_list_; }

    // Owner is a flag matching the id of the TraceBufferWriter, 0 if not owned.
    // Accesses to |owner_| must happen under the buffer |lock_|.
    bool is_owned() const { return owner_ != kNoChunkOwner; }
    uint32_t owner() const { return owner_; }
    void clear_owner() { owner_ = kNoChunkOwner; }
    void set_owner(uint32_t owner) {
      DCHECK_NE(kNoChunkOwner, owner);
      owner_ = owner;
    }

   private:
    uint8_t* begin_;
    uint32_t owner_;

    // When a chunk is owned, this is the next pointer to keep track of all
    // owned chunks in a singly linked list.
    Chunk* next_in_owner_list_;

    DISALLOW_COPY_AND_ASSIGN(Chunk);
  };

  TraceRingBuffer(uint8_t* begin, size_t size);
  ~TraceRingBuffer();

  Chunk* TakeChunk(uint32_t writer_id);
  void ReturnChunk(Chunk* chunk);

  size_t num_chunks() const { return num_chunks_; }

  // Returns the number of chunks taken and not returned, without counting any
  // bankrupcy chunk obtained when the ring buffer was full.
  size_t GetNumChunksTaken() const;

  const Chunk* chunks_for_testing() const { return chunks_.get(); }
  bool IsBankrupcyChunkForTesting(const Chunk*) const;

 private:
  mutable base::Lock lock_;
  std::unique_ptr<Chunk[]> chunks_;
  const size_t num_chunks_;
  size_t num_chunks_taken_;
  size_t current_chunk_idx_;

  // An emergency chunk used in the rare case in which all chunks are in flight.
  // This chunk is not part of the ring buffer and its contents are always
  // discarded. Its only purpose is to avoid a crash (due to TakeChunk returning
  // nullptr) in the case of a thread storm.
  Chunk bankrupcy_chunk_;
  std::unique_ptr<uint8_t[]> bankrupcy_chunk_storage_;

  DISALLOW_COPY_AND_ASSIGN(TraceRingBuffer);
};

}  // namespace v2
}  // namespace tracing

#endif  // COMPONENTS_TRACING_CORE_TRACE_RING_BUFFER_H_
