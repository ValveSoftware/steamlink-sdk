// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/iterators/TextBufferBase.h"

namespace blink {

TextBufferBase::TextBufferBase() {
  m_buffer.reserveCapacity(1024);
  m_buffer.resize(capacity());
}

void TextBufferBase::shiftData(size_t) {}

void TextBufferBase::pushCharacters(UChar ch, size_t length) {
  if (length == 0)
    return;
  std::fill_n(ensureDestination(length), length, ch);
}

UChar* TextBufferBase::ensureDestination(size_t length) {
  if (m_size + length > capacity())
    grow(m_size + length);
  UChar* ans = calcDestination(length);
  m_size += length;
  return ans;
}

void TextBufferBase::grow(size_t demand) {
  size_t oldCapacity = capacity();
  m_buffer.resize(demand);
  m_buffer.resize(capacity());
  shiftData(oldCapacity);
}

}  // namespace blink
