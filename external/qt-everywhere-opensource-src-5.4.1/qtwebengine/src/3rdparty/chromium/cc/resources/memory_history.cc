// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/memory_history.h"

#include <limits>

namespace cc {

// static
scoped_ptr<MemoryHistory> MemoryHistory::Create() {
  return make_scoped_ptr(new MemoryHistory());
}

MemoryHistory::MemoryHistory() {}

void MemoryHistory::SaveEntry(const MemoryHistory::Entry& entry) {
  ring_buffer_.SaveToBuffer(entry);
}

void MemoryHistory::GetMinAndMax(size_t* min, size_t* max) const {
  *min = std::numeric_limits<size_t>::max();
  *max = 0;

  for (RingBufferType::Iterator it = ring_buffer_.Begin(); it; ++it) {
    size_t bytes_total = it->bytes_total();

    if (bytes_total < *min)
      *min = bytes_total;
    if (bytes_total > *max)
      *max = bytes_total;
  }

  if (*min > *max)
    *min = *max;
}

}  // namespace cc
