// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/FuzzedDataProvider.h"

namespace blink {

FuzzedDataProvider::FuzzedDataProvider(const uint8_t* bytes, size_t numBytes)
    : m_provider(bytes, numBytes) {}

CString FuzzedDataProvider::ConsumeBytesInRange(uint32_t minBytes,
                                                uint32_t maxBytes) {
  size_t numBytes =
      static_cast<size_t>(m_provider.ConsumeUint32InRange(minBytes, maxBytes));
  std::string bytes = m_provider.ConsumeBytes(numBytes);
  return CString(bytes.data(), bytes.size());
}

CString FuzzedDataProvider::ConsumeRemainingBytes() {
  std::string bytes = m_provider.ConsumeRemainingBytes();
  return CString(bytes.data(), bytes.size());
}

bool FuzzedDataProvider::ConsumeBool() {
  return m_provider.ConsumeBool();
}

}  // namespace blink
