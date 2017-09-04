// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/CachedMetadata.h"

namespace blink {

CachedMetadata::CachedMetadata(const char* data, size_t size) {
  // We need to define a local variable to use the constant in DCHECK.
  constexpr auto kDataStart = CachedMetadata::kDataStart;
  // Serialized metadata should have non-empty data.
  DCHECK_GT(size, kDataStart);
  DCHECK(data);

  m_serializedData.reserveInitialCapacity(size);
  m_serializedData.append(data, size);
}

CachedMetadata::CachedMetadata(uint32_t dataTypeID,
                               const char* data,
                               size_t size) {
  // Don't allow an ID of 0, it is used internally to indicate errors.
  DCHECK(dataTypeID);
  DCHECK(data);

  m_serializedData.reserveInitialCapacity(sizeof(uint32_t) + size);
  m_serializedData.append(reinterpret_cast<const char*>(&dataTypeID),
                          sizeof(uint32_t));
  m_serializedData.append(data, size);
}

}  // namespace blink
