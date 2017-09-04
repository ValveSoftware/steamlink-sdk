// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBObservation.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "modules/IndexedDBNames.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBValue.h"
#include "public/platform/modules/indexeddb/WebIDBObservation.h"

namespace blink {

IDBObservation::~IDBObservation() {}

ScriptValue IDBObservation::key(ScriptState* scriptState) {
  if (!m_keyRange)
    return ScriptValue::from(scriptState,
                             v8::Undefined(scriptState->isolate()));

  return ScriptValue::from(scriptState, m_keyRange);
}

ScriptValue IDBObservation::value(ScriptState* scriptState) {
  if (!m_value)
    return ScriptValue::from(scriptState,
                             v8::Undefined(scriptState->isolate()));

  return ScriptValue::from(scriptState, IDBAny::create(m_value));
}

WebIDBOperationType IDBObservation::stringToOperationType(const String& type) {
  if (type == IndexedDBNames::add)
    return WebIDBAdd;
  if (type == IndexedDBNames::put)
    return WebIDBPut;
  if (type == IndexedDBNames::kDelete)
    return WebIDBDelete;
  if (type == IndexedDBNames::clear)
    return WebIDBClear;

  NOTREACHED();
  return WebIDBAdd;
}

const String& IDBObservation::type() const {
  switch (m_operationType) {
    case WebIDBAdd:
      return IndexedDBNames::add;

    case WebIDBPut:
      return IndexedDBNames::put;

    case WebIDBDelete:
      return IndexedDBNames::kDelete;

    case WebIDBClear:
      return IndexedDBNames::clear;

    default:
      NOTREACHED();
      return IndexedDBNames::add;
  }
}

IDBObservation* IDBObservation::create(const WebIDBObservation& observation) {
  return new IDBObservation(observation);
}

IDBObservation::IDBObservation(const WebIDBObservation& observation)
    : m_keyRange(observation.keyRange),
      m_value(IDBValue::create(observation.value)),
      m_operationType(observation.type) {}

DEFINE_TRACE(IDBObservation) {
  visitor->trace(m_keyRange);
}

}  // namespace blink
