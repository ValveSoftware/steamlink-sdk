// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBObserverChanges.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBObservation.h"
#include "public/platform/modules/indexeddb/WebIDBObservation.h"

namespace blink {

ScriptValue IDBObserverChanges::records(ScriptState* scriptState) {
  v8::Local<v8::Context> context(scriptState->context());
  v8::Isolate* isolate(scriptState->isolate());
  v8::Local<v8::Map> map = v8::Map::New(isolate);
  for (const auto& it : m_records) {
    v8::Local<v8::String> key =
        v8String(isolate, m_database->getObjectStoreName(it.key));
    v8::Local<v8::Value> value = toV8(it.value, context->Global(), isolate);
    map->Set(context, key, value).ToLocalChecked();
  }
  return ScriptValue::from(scriptState, map);
}

IDBObserverChanges* IDBObserverChanges::create(
    IDBDatabase* database,
    const WebVector<WebIDBObservation>& observations,
    const WebVector<int32_t>& observationIndex) {
  return new IDBObserverChanges(database, observations, observationIndex);
}

IDBObserverChanges::IDBObserverChanges(
    IDBDatabase* database,
    const WebVector<WebIDBObservation>& observations,
    const WebVector<int32_t>& observationIndex)
    : m_database(database) {
  extractChanges(observations, observationIndex);
}

void IDBObserverChanges::extractChanges(
    const WebVector<WebIDBObservation>& observations,
    const WebVector<int32_t>& observationIndex) {
  // TODO(dmurph): Avoid getting and setting repeated times.
  for (const auto& idx : observationIndex)
    m_records
        .add(observations[idx].objectStoreId,
             HeapVector<Member<IDBObservation>>())
        .storedValue->value.append(IDBObservation::create(observations[idx]));
}

DEFINE_TRACE(IDBObserverChanges) {
  visitor->trace(m_database);
  visitor->trace(m_transaction);
  visitor->trace(m_records);
}

}  // namespace blink
