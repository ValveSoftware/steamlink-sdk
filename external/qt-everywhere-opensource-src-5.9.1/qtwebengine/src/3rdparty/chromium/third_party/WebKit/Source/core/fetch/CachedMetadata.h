/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CachedMetadata_h
#define CachedMetadata_h

#include "core/CoreExport.h"
#include "wtf/Assertions.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include <stdint.h>

namespace blink {

// Metadata retrieved from the embedding application's cache.
//
// Serialized data is NOT portable across architectures. However, reading the
// data type ID will reject data generated with a different byte-order.
class CORE_EXPORT CachedMetadata : public RefCounted<CachedMetadata> {
 public:
  static PassRefPtr<CachedMetadata> create(uint32_t dataTypeID,
                                           const char* data,
                                           size_t size) {
    return adoptRef(new CachedMetadata(dataTypeID, data, size));
  }

  static PassRefPtr<CachedMetadata> createFromSerializedData(const char* data,
                                                             size_t size) {
    return adoptRef(new CachedMetadata(data, size));
  }

  ~CachedMetadata() {}

  const Vector<char>& serializedData() const { return m_serializedData; }

  uint32_t dataTypeID() const {
    // We need to define a local variable to use the constant in DCHECK.
    constexpr auto kDataStart = CachedMetadata::kDataStart;
    DCHECK_GE(m_serializedData.size(), kDataStart);
    return *reinterpret_cast_ptr<uint32_t*>(
        const_cast<char*>(m_serializedData.data()));
  }

  const char* data() const {
    constexpr auto kDataStart = CachedMetadata::kDataStart;
    DCHECK_GE(m_serializedData.size(), kDataStart);
    return m_serializedData.data() + kDataStart;
  }

  size_t size() const {
    constexpr auto kDataStart = CachedMetadata::kDataStart;
    DCHECK_GE(m_serializedData.size(), kDataStart);
    return m_serializedData.size() - kDataStart;
  }

 private:
  CachedMetadata(const char* data, size_t);
  CachedMetadata(uint32_t dataTypeID, const char* data, size_t);

  // Since the serialization format supports random access, storing it in
  // serialized form avoids need for a copy during serialization.
  Vector<char> m_serializedData;

  // |m_serializedData| consists of 32 bits type ID and and actual data.
  static constexpr size_t kDataStart = sizeof(uint32_t);
};

}  // namespace blink

#endif
