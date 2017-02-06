// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBObserverChanges_h
#define IDBObserverChanges_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "platform/heap/Handle.h"

namespace blink {

class ScriptState;
class IDBObserverChangesRecord;

class IDBObserverChanges final : public GarbageCollected<IDBObserverChanges>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static IDBObserverChanges* create(IDBDatabase*, IDBTransaction*, IDBAny* records);

    DECLARE_TRACE();

    // Implement IDL
    IDBTransaction* transaction() const { return m_transaction.get(); }
    IDBDatabase* database() const { return m_database.get(); }
    ScriptValue records(ScriptState*);

private:
    IDBObserverChanges(IDBDatabase*, IDBTransaction*, IDBAny* records);

    Member<IDBDatabase> m_database;
    Member<IDBTransaction> m_transaction;
    // TODO(palakj) : change to appropriate type Map<String, sequence<IDBObserverChangesRecord>>.
    Member<IDBAny> m_records;
};

} // namespace blink

#endif // IDBObserverChanges_h
