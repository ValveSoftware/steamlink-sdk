// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_MEMORY_HISTORY_H_
#define CC_RESOURCES_MEMORY_HISTORY_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/debug/ring_buffer.h"

namespace cc {

// Maintains a history of memory for each frame.
class MemoryHistory {
 public:
  static scoped_ptr<MemoryHistory> Create();

  size_t HistorySize() const { return ring_buffer_.BufferSize(); }

  struct Entry {
    Entry()
        : total_budget_in_bytes(0),
          bytes_allocated(0),
          bytes_unreleasable(0),
          bytes_over(0) {}

    size_t total_budget_in_bytes;
    size_t bytes_allocated;
    size_t bytes_unreleasable;
    size_t bytes_over;
    size_t bytes_total() const {
      return bytes_allocated + bytes_unreleasable + bytes_over;
    }
  };

  void SaveEntry(const Entry& entry);
  void GetMinAndMax(size_t* min, size_t* max) const;

  typedef RingBuffer<Entry, 80> RingBufferType;
  RingBufferType::Iterator Begin() const { return ring_buffer_.Begin(); }
  RingBufferType::Iterator End() const { return ring_buffer_.End(); }

 private:
  MemoryHistory();

  RingBufferType ring_buffer_;

  DISALLOW_COPY_AND_ASSIGN(MemoryHistory);
};

}  // namespace cc

#endif  // CC_RESOURCES_MEMORY_HISTORY_H_
