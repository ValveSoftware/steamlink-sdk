// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CORE_TRACE_RING_BUFFER_H_
#define COMPONENTS_TRACING_CORE_TRACE_RING_BUFFER_H_

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "components/tracing/tracing_export.h"

namespace tracing {
namespace v2 {

class TRACING_EXPORT TraceRingBuffer {
 public:
  class Chunk {
   public:
    using Header = base::subtle::Atomic32;
    static constexpr size_t kSize = 32 * 1024;

    Chunk();
    ~Chunk();

    void Initialize(uint8_t* begin);
    void Clear();

    uint8_t* begin() const { return begin_; }
    Header* header() const { return reinterpret_cast<Header*>(begin_); }
    uint8_t* payload() const { return begin_ + sizeof(Header); }
    uint8_t* end() const { return begin_ + kSize; }

    void set_used_size(uint32_t size) {
      base::subtle::NoBarrier_Store(header(), size);
    }
    uint32_t used_size() const {
      return base::subtle::NoBarrier_Load(header());
    }

    // Accesses to |owner_| must happen under the buffer |lock_|.
    bool is_owned() const { return owner_ != base::kInvalidThreadId; }
    void clear_owner() { owner_ = base::kInvalidThreadId; }
    void set_owner(base::PlatformThreadId tid) { owner_ = tid; }

   private:
    uint8_t* begin_;
    base::PlatformThreadId owner_;  // kInvalidThreadId -> Chunk is not owned.

    DISALLOW_COPY_AND_ASSIGN(Chunk);
  };

  TraceRingBuffer(uint8_t* begin, size_t size);
  ~TraceRingBuffer();

  Chunk* TakeChunk();
  void ReturnChunk(Chunk* chunk, uint32_t used_size);

 private:
  base::Lock lock_;
  std::unique_ptr<Chunk[]> chunks_;
  const size_t num_chunks_;
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
