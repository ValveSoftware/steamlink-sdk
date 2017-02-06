// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CORE_SCATTERED_STREAM_WRITER_H_
#define COMPONENTS_TRACING_CORE_SCATTERED_STREAM_WRITER_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/tracing/tracing_export.h"

namespace tracing {
namespace v2 {

struct ContiguousMemoryRange {
  uint8_t* begin;
  uint8_t* end;  // STL style: one byte past the end of the buffer.
};

// This class deals with the following problem: append-only proto messages want
// to write a stream of bytes, without caring about the implementation of the
// underlying buffer (which concretely will be either the trace ring buffer
// or a heap-allocated buffer). The main deal is: proto messages don't know in
// advance what their size will be.
// Due to the tracing buffer being split into fixed-size chunks, on some
// occasions, these writes need to be spread over two (or more) non-contiguous
// chunks of memory. Similarly, when the buffer is backed by the heap, we want
// to avoid realloc() calls, as they might cause a full copy of the contents
// of the buffer.
// The purpose of this class is to abtract away the non-contiguous write logic.
// This class knows how to deal with writes as long as they fall in the same
// ContiguousMemoryRange and defers the chunk-chaining logic to the Delegate.
class TRACING_EXPORT ScatteredStreamWriter {
 public:
  class Delegate {
   public:
    virtual ContiguousMemoryRange GetNewBuffer() = 0;
  };

  explicit ScatteredStreamWriter(Delegate* delegate);
  ~ScatteredStreamWriter();

  void WriteByte(uint8_t value);
  void WriteBytes(const uint8_t* src, size_t size);

  // Reserves a fixed amount of bytes to be backfilled later. The reserved range
  // is guaranteed to be contiguous and not span across chunks.
  ContiguousMemoryRange ReserveBytes(size_t size);

  // Resets the buffer boundaries and the write pointer to the given |range|.
  // Subsequent WriteByte(s) will write into |range|.
  void Reset(ContiguousMemoryRange range);

  // Number of contiguous free bytes in |cur_range_| that can be written without
  // requesting a new buffer.
  size_t bytes_available() const {
    return static_cast<size_t>(cur_range_.end - write_ptr_);
  }

  uint8_t* write_ptr() const { return write_ptr_; }

 private:
  void Extend();

  Delegate* const delegate_;
  ContiguousMemoryRange cur_range_;
  uint8_t* write_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ScatteredStreamWriter);
};

}  // namespace v2
}  // namespace tracing

#endif  // COMPONENTS_TRACING_CORE_SCATTERED_STREAM_WRITER_H_
