// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/MediaKeyStatusMap.h"

#include "bindings/core/v8/ArrayBufferOrArrayBufferView.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayPiece.h"
#include "public/platform/WebData.h"
#include "wtf/text/WTFString.h"

#include <algorithm>
#include <limits>

namespace blink {

// Represents the key ID and associated status.
class MediaKeyStatusMap::MapEntry final
    : public GarbageCollectedFinalized<MediaKeyStatusMap::MapEntry> {
 public:
  static MapEntry* create(WebData keyId, const String& status) {
    return new MapEntry(keyId, status);
  }

  virtual ~MapEntry() {}

  DOMArrayBuffer* keyId() const { return m_keyId.get(); }

  const String& status() const { return m_status; }

  static bool compareLessThan(MapEntry* a, MapEntry* b) {
    // Compare the keyIds of 2 different MapEntries. Assume that |a| and |b|
    // are not null, but the keyId() may be. KeyIds are compared byte
    // by byte.
    DCHECK(a);
    DCHECK(b);

    // Handle null cases first (which shouldn't happen).
    //    |aKeyId|    |bKeyId|     result
    //      null        null         == (false)
    //      null      not-null       <  (true)
    //    not-null      null         >  (false)
    if (!a->keyId() || !b->keyId())
      return b->keyId();

    // Compare the bytes.
    int result =
        memcmp(a->keyId()->data(), b->keyId()->data(),
               std::min(a->keyId()->byteLength(), b->keyId()->byteLength()));
    if (result != 0)
      return result < 0;

    // KeyIds are equal to the shared length, so the shorter string is <.
    DCHECK_NE(a->keyId()->byteLength(), b->keyId()->byteLength());
    return a->keyId()->byteLength() < b->keyId()->byteLength();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(m_keyId); }

 private:
  MapEntry(WebData keyId, const String& status)
      : m_keyId(DOMArrayBuffer::create(keyId.data(), keyId.size())),
        m_status(status) {}

  const Member<DOMArrayBuffer> m_keyId;
  const String m_status;
};

// Represents an Iterator that loops through the set of MapEntrys.
class MapIterationSource final
    : public PairIterable<ArrayBufferOrArrayBufferView,
                          String>::IterationSource {
 public:
  MapIterationSource(MediaKeyStatusMap* map) : m_map(map), m_current(0) {}

  bool next(ScriptState* scriptState,
            ArrayBufferOrArrayBufferView& key,
            String& value,
            ExceptionState&) override {
    // This simply advances an index and returns the next value if any,
    // so if the iterated object is mutated values may be skipped.
    if (m_current >= m_map->size())
      return false;

    const auto& entry = m_map->at(m_current++);
    key.setArrayBuffer(entry.keyId());
    value = entry.status();
    return true;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_map);
    PairIterable<ArrayBufferOrArrayBufferView, String>::IterationSource::trace(
        visitor);
  }

 private:
  // m_map is stored just for keeping it alive. It needs to be kept
  // alive while JavaScript holds the iterator to it.
  const Member<const MediaKeyStatusMap> m_map;
  size_t m_current;
};

void MediaKeyStatusMap::clear() {
  m_entries.clear();
}

void MediaKeyStatusMap::addEntry(WebData keyId, const String& status) {
  // Insert new entry into sorted list.
  MapEntry* entry = MapEntry::create(keyId, status);
  size_t index = 0;
  while (index < m_entries.size() &&
         MapEntry::compareLessThan(m_entries[index], entry))
    ++index;
  m_entries.insert(index, entry);
}

const MediaKeyStatusMap::MapEntry& MediaKeyStatusMap::at(size_t index) const {
  DCHECK_LT(index, m_entries.size());
  return *m_entries.at(index);
}

size_t MediaKeyStatusMap::indexOf(const DOMArrayPiece& key) const {
  for (size_t index = 0; index < m_entries.size(); ++index) {
    const auto& current = m_entries.at(index)->keyId();
    if (key == *current)
      return index;
  }

  // Not found, so return an index outside the valid range. The caller
  // must ensure this value is not exposed outside this class.
  return std::numeric_limits<size_t>::max();
}

bool MediaKeyStatusMap::has(const ArrayBufferOrArrayBufferView& keyId) {
  size_t index = indexOf(keyId);
  return index < m_entries.size();
}

ScriptValue MediaKeyStatusMap::get(ScriptState* scriptState,
                                   const ArrayBufferOrArrayBufferView& keyId) {
  size_t index = indexOf(keyId);
  if (index >= m_entries.size())
    return ScriptValue(scriptState, v8::Undefined(scriptState->isolate()));
  return ScriptValue::from(scriptState, at(index).status());
}

PairIterable<ArrayBufferOrArrayBufferView, String>::IterationSource*
MediaKeyStatusMap::startIteration(ScriptState*, ExceptionState&) {
  return new MapIterationSource(this);
}

DEFINE_TRACE(MediaKeyStatusMap) {
  visitor->trace(m_entries);
}

}  // namespace blink
