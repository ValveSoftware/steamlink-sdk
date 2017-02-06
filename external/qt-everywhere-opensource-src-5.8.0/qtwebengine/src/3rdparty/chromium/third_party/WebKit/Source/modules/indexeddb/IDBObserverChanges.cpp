// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBObserverChanges.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "modules/indexeddb/IDBAny.h"

namespace blink {

ScriptValue IDBObserverChanges::records(ScriptState* scriptState)
{
    return ScriptValue::from(scriptState, m_records);
}

IDBObserverChanges* IDBObserverChanges::create(IDBDatabase* database, IDBTransaction* transaction, IDBAny* records)
{
    return new IDBObserverChanges(database, transaction, records);
}

IDBObserverChanges::IDBObserverChanges(IDBDatabase* database, IDBTransaction* transaction, IDBAny* records)
    : m_database(database)
    , m_transaction(transaction)
    , m_records(records)
{
}

DEFINE_TRACE(IDBObserverChanges)
{
    visitor->trace(m_database);
    visitor->trace(m_transaction);
    visitor->trace(m_records);
}

} // namespace blink
