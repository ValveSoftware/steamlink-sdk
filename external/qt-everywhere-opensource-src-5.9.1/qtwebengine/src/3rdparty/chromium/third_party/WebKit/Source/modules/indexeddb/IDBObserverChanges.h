// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBObserverChanges_h
#define IDBObserverChanges_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBObservation.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebVector.h"

namespace blink {

class ScriptState;

class IDBObserverChanges final : public GarbageCollected<IDBObserverChanges>,
                                 public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBObserverChanges* create(IDBDatabase*,
                                    const WebVector<WebIDBObservation>&,
                                    const WebVector<int32_t>& observationIndex);

  DECLARE_TRACE();

  // Implement IDL
  IDBTransaction* transaction() const { return m_transaction.get(); }
  IDBDatabase* database() const { return m_database.get(); }
  ScriptValue records(ScriptState*);

 private:
  IDBObserverChanges(IDBDatabase*,
                     const WebVector<WebIDBObservation>&,
                     const WebVector<int32_t>& observationIndex);

  void extractChanges(const WebVector<WebIDBObservation>&,
                      const WebVector<int32_t>& observationIndex);

  Member<IDBDatabase> m_database;
  Member<IDBTransaction> m_transaction;
  // Map objectStoreId to IDBObservation list.
  HeapHashMap<int64_t, HeapVector<Member<IDBObservation>>> m_records;
};

}  // namespace blink

#endif  // IDBObserverChanges_h
