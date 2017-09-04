// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/iterators/BackwardsTextBuffer.h"

namespace blink {

const UChar* BackwardsTextBuffer::data() const {
  return bufferEnd() - size();
}

UChar* BackwardsTextBuffer::calcDestination(size_t length) {
  DCHECK_LE(size() + length, capacity());
  return bufferEnd() - size() - length;
}

void BackwardsTextBuffer::shiftData(size_t oldCapacity) {
  DCHECK_LE(oldCapacity, capacity());
  DCHECK_LE(size(), oldCapacity);
  std::copy_backward(bufferBegin() + oldCapacity - size(),
                     bufferBegin() + oldCapacity, bufferEnd());
}

}  // namespace blink
