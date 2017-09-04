// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeyStatusMap_h
#define MediaKeyStatusMap_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayPiece.h"
#include "platform/heap/Heap.h"

namespace blink {

class ExceptionState;
class ScriptState;
class WebData;

// Represents a read-only map (to JavaScript) of key IDs and their current
// status known to a particular session. Since it can be updated any time there
// is a keychange event, iteration order and completeness is not guaranteed
// if the event loop runs.
class MediaKeyStatusMap final
    : public GarbageCollected<MediaKeyStatusMap>,
      public ScriptWrappable,
      public PairIterable<ArrayBufferOrArrayBufferView, String> {
  DEFINE_WRAPPERTYPEINFO();

 private:
  // MapEntry holds the keyId (DOMArrayBuffer) and status (MediaKeyStatus as
  // String) for each entry.
  class MapEntry;

  // The key ids and their status are kept in a list, as order is important.
  // Note that order (or lack of it) is not specified in the EME spec.
  using MediaKeyStatusMapType = HeapVector<Member<MapEntry>>;

 public:
  MediaKeyStatusMap() {}

  void clear();
  void addEntry(WebData keyId, const String& status);
  const MapEntry& at(size_t) const;

  // IDL attributes / methods
  size_t size() const { return m_entries.size(); }
  bool has(const ArrayBufferOrArrayBufferView& keyId);
  ScriptValue get(ScriptState*, const ArrayBufferOrArrayBufferView& keyId);

  DECLARE_VIRTUAL_TRACE();

 private:
  // PairIterable<> implementation.
  IterationSource* startIteration(ScriptState*, ExceptionState&) override;

  size_t indexOf(const DOMArrayPiece& keyId) const;

  MediaKeyStatusMapType m_entries;
};

}  // namespace blink

#endif  // MediaKeyStatusMap_h
