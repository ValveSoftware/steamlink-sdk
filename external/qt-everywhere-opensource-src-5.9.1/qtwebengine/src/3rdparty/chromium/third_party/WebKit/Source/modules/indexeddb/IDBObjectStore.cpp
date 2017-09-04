/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/indexeddb/IDBObjectStore.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBCursorWithValue.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBTracing.h"
#include "platform/SharedBuffer.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/WebData.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/indexeddb/WebIDBKey.h"
#include "public/platform/modules/indexeddb/WebIDBKeyRange.h"
#include <memory>
#include <v8.h>

using blink::WebBlobInfo;
using blink::WebIDBCallbacks;
using blink::WebIDBCursor;
using blink::WebIDBDatabase;
using blink::WebVector;

namespace blink {

namespace {

using IndexKeys = HeapVector<Member<IDBKey>>;
}

IDBObjectStore::IDBObjectStore(RefPtr<IDBObjectStoreMetadata> metadata,
                               IDBTransaction* transaction)
    : m_metadata(std::move(metadata)), m_transaction(transaction) {
  DCHECK(m_transaction);
  DCHECK(m_metadata.get());
}

DEFINE_TRACE(IDBObjectStore) {
  visitor->trace(m_transaction);
  visitor->trace(m_indexMap);
}

void IDBObjectStore::setName(const String& name,
                             ExceptionState& exceptionState) {
  if (!RuntimeEnabledFeatures::indexedDBExperimentalEnabled())
    return;

  IDB_TRACE("IDBObjectStore::setName");
  if (!m_transaction->isVersionChange()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        IDBDatabase::notVersionChangeTransactionErrorMessage);
    return;
  }
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return;
  }

  if (this->name() == name)
    return;
  if (m_transaction->db()->containsObjectStore(name)) {
    exceptionState.throwDOMException(
        ConstraintError, IDBDatabase::objectStoreNameTakenErrorMessage);
    return;
  }
  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return;
  }

  m_transaction->db()->renameObjectStore(id(), name);
}

ScriptValue IDBObjectStore::keyPath(ScriptState* scriptState) const {
  return ScriptValue::from(scriptState, metadata().keyPath);
}

DOMStringList* IDBObjectStore::indexNames() const {
  IDB_TRACE("IDBObjectStore::indexNames");
  DOMStringList* indexNames = DOMStringList::create(DOMStringList::IndexedDB);
  for (const auto& it : metadata().indexes)
    indexNames->append(it.value->name);
  indexNames->sort();
  return indexNames;
}

IDBRequest* IDBObjectStore::get(ScriptState* scriptState,
                                const ScriptValue& key,
                                ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::get");
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }
  IDBKeyRange* keyRange = IDBKeyRange::fromScriptValue(
      scriptState->getExecutionContext(), key, exceptionState);
  if (exceptionState.hadException())
    return nullptr;
  if (!keyRange) {
    exceptionState.throwDOMException(DataError,
                                     IDBDatabase::noKeyOrKeyRangeErrorMessage);
    return nullptr;
  }
  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::create(scriptState, IDBAny::create(this),
                                           m_transaction.get());
  backendDB()->get(m_transaction->id(), id(), IDBIndexMetadata::InvalidId,
                   keyRange, false /* keyOnly */,
                   request->createWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::getKey(ScriptState* scriptState,
                                   const ScriptValue& key,
                                   ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::getKey");
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }
  IDBKeyRange* keyRange = IDBKeyRange::fromScriptValue(
      scriptState->getExecutionContext(), key, exceptionState);
  if (exceptionState.hadException())
    return nullptr;
  if (!keyRange) {
    exceptionState.throwDOMException(DataError,
                                     IDBDatabase::noKeyOrKeyRangeErrorMessage);
    return nullptr;
  }
  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::create(scriptState, IDBAny::create(this),
                                           m_transaction.get());
  backendDB()->get(m_transaction->id(), id(), IDBIndexMetadata::InvalidId,
                   keyRange, true /* keyOnly */,
                   request->createWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::getAll(ScriptState* scriptState,
                                   const ScriptValue& keyRange,
                                   ExceptionState& exceptionState) {
  return getAll(scriptState, keyRange, std::numeric_limits<uint32_t>::max(),
                exceptionState);
}

IDBRequest* IDBObjectStore::getAll(ScriptState* scriptState,
                                   const ScriptValue& keyRange,
                                   unsigned long maxCount,
                                   ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::getAll");
  if (!maxCount)
    maxCount = std::numeric_limits<uint32_t>::max();

  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }
  IDBKeyRange* range = IDBKeyRange::fromScriptValue(
      scriptState->getExecutionContext(), keyRange, exceptionState);
  if (exceptionState.hadException())
    return nullptr;
  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::create(scriptState, IDBAny::create(this),
                                           m_transaction.get());
  backendDB()->getAll(m_transaction->id(), id(), IDBIndexMetadata::InvalidId,
                      range, maxCount, false,
                      request->createWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::getAllKeys(ScriptState* scriptState,
                                       const ScriptValue& keyRange,
                                       ExceptionState& exceptionState) {
  return getAllKeys(scriptState, keyRange, std::numeric_limits<uint32_t>::max(),
                    exceptionState);
}

IDBRequest* IDBObjectStore::getAllKeys(ScriptState* scriptState,
                                       const ScriptValue& keyRange,
                                       unsigned long maxCount,
                                       ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::getAll");
  if (!maxCount)
    maxCount = std::numeric_limits<uint32_t>::max();

  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }
  IDBKeyRange* range = IDBKeyRange::fromScriptValue(
      scriptState->getExecutionContext(), keyRange, exceptionState);
  if (exceptionState.hadException())
    return nullptr;
  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::create(scriptState, IDBAny::create(this),
                                           m_transaction.get());
  backendDB()->getAll(m_transaction->id(), id(), IDBIndexMetadata::InvalidId,
                      range, maxCount, true,
                      request->createWebCallbacks().release());
  return request;
}

static void generateIndexKeysForValue(v8::Isolate* isolate,
                                      const IDBIndexMetadata& indexMetadata,
                                      const ScriptValue& objectValue,
                                      IndexKeys* indexKeys) {
  DCHECK(indexKeys);
  NonThrowableExceptionState exceptionState;
  IDBKey* indexKey = ScriptValue::to<IDBKey*>(
      isolate, objectValue, exceptionState, indexMetadata.keyPath);

  if (!indexKey)
    return;

  if (!indexMetadata.multiEntry || indexKey->getType() != IDBKey::ArrayType) {
    if (!indexKey->isValid())
      return;

    indexKeys->append(indexKey);
  } else {
    DCHECK(indexMetadata.multiEntry);
    DCHECK_EQ(indexKey->getType(), IDBKey::ArrayType);
    indexKey = IDBKey::createMultiEntryArray(indexKey->array());

    for (size_t i = 0; i < indexKey->array().size(); ++i)
      indexKeys->append(indexKey->array()[i]);
  }
}

IDBRequest* IDBObjectStore::add(ScriptState* scriptState,
                                const ScriptValue& value,
                                const ScriptValue& key,
                                ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::add");
  return put(scriptState, WebIDBPutModeAddOnly, IDBAny::create(this), value,
             key, exceptionState);
}

IDBRequest* IDBObjectStore::put(ScriptState* scriptState,
                                const ScriptValue& value,
                                const ScriptValue& key,
                                ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::put");
  return put(scriptState, WebIDBPutModeAddOrUpdate, IDBAny::create(this), value,
             key, exceptionState);
}

IDBRequest* IDBObjectStore::put(ScriptState* scriptState,
                                WebIDBPutMode putMode,
                                IDBAny* source,
                                const ScriptValue& value,
                                const ScriptValue& keyValue,
                                ExceptionState& exceptionState) {
  IDBKey* key = keyValue.isUndefined()
                    ? nullptr
                    : ScriptValue::to<IDBKey*>(scriptState->isolate(), keyValue,
                                               exceptionState);
  if (exceptionState.hadException())
    return nullptr;
  return put(scriptState, putMode, source, value, key, exceptionState);
}

IDBRequest* IDBObjectStore::put(ScriptState* scriptState,
                                WebIDBPutMode putMode,
                                IDBAny* source,
                                const ScriptValue& value,
                                IDBKey* key,
                                ExceptionState& exceptionState) {
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }
  if (m_transaction->isReadOnly()) {
    exceptionState.throwDOMException(
        ReadOnlyError, IDBDatabase::transactionReadOnlyErrorMessage);
    return nullptr;
  }

  v8::Isolate* isolate = scriptState->isolate();
  DCHECK(isolate->InContext());
  Vector<WebBlobInfo> blobInfo;
  RefPtr<SerializedScriptValue> serializedValue =
      SerializedScriptValue::serialize(isolate, value.v8Value(), nullptr,
                                       &blobInfo, exceptionState);
  if (exceptionState.hadException())
    return nullptr;

  // Keys that need to be extracted must be taken from a clone so that
  // side effects (i.e. getters) are not triggered. Construct the
  // clone lazily since the operation may be expensive.
  ScriptValue clone;

  const IDBKeyPath& keyPath = idbKeyPath();
  const bool usesInLineKeys = !keyPath.isNull();
  const bool hasKeyGenerator = autoIncrement();

  if (putMode != WebIDBPutModeCursorUpdate && usesInLineKeys && key) {
    exceptionState.throwDOMException(DataError,
                                     "The object store uses in-line keys and "
                                     "the key parameter was provided.");
    return nullptr;
  }

  // This test logically belongs in IDBCursor, but must operate on the cloned
  // value.
  if (putMode == WebIDBPutModeCursorUpdate && usesInLineKeys) {
    DCHECK(key);
    if (clone.isEmpty())
      clone =
          deserializeScriptValue(scriptState, serializedValue.get(), &blobInfo);
    IDBKey* keyPathKey = ScriptValue::to<IDBKey*>(scriptState->isolate(), clone,
                                                  exceptionState, keyPath);
    if (exceptionState.hadException())
      return nullptr;
    if (!keyPathKey || !keyPathKey->isEqual(key)) {
      exceptionState.throwDOMException(
          DataError,
          "The effective object store of this cursor uses in-line keys and "
          "evaluating the key path of the value parameter results in a "
          "different value than the cursor's effective key.");
      return nullptr;
    }
  }

  if (!usesInLineKeys && !hasKeyGenerator && !key) {
    exceptionState.throwDOMException(DataError,
                                     "The object store uses out-of-line keys "
                                     "and has no key generator and the key "
                                     "parameter was not provided.");
    return nullptr;
  }
  if (usesInLineKeys) {
    if (clone.isEmpty())
      clone =
          deserializeScriptValue(scriptState, serializedValue.get(), &blobInfo);
    IDBKey* keyPathKey = ScriptValue::to<IDBKey*>(scriptState->isolate(), clone,
                                                  exceptionState, keyPath);
    if (exceptionState.hadException())
      return nullptr;
    if (keyPathKey && !keyPathKey->isValid()) {
      exceptionState.throwDOMException(DataError,
                                       "Evaluating the object store's key path "
                                       "yielded a value that is not a valid "
                                       "key.");
      return nullptr;
    }
    if (!hasKeyGenerator && !keyPathKey) {
      exceptionState.throwDOMException(
          DataError,
          "Evaluating the object store's key path did not yield a value.");
      return nullptr;
    }
    if (hasKeyGenerator && !keyPathKey) {
      if (!canInjectIDBKeyIntoScriptValue(scriptState->isolate(), clone,
                                          keyPath)) {
        exceptionState.throwDOMException(
            DataError, "A generated key could not be inserted into the value.");
        return nullptr;
      }
    }
    if (keyPathKey)
      key = keyPathKey;
  }
  if (key && !key->isValid()) {
    exceptionState.throwDOMException(DataError,
                                     IDBDatabase::notValidKeyErrorMessage);
    return nullptr;
  }

  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  Vector<int64_t> indexIds;
  HeapVector<IndexKeys> indexKeys;
  for (const auto& it : metadata().indexes) {
    if (clone.isEmpty())
      clone =
          deserializeScriptValue(scriptState, serializedValue.get(), &blobInfo);
    IndexKeys keys;
    generateIndexKeysForValue(scriptState->isolate(), *it.value, clone, &keys);
    indexIds.append(it.key);
    indexKeys.append(keys);
  }

  IDBRequest* request =
      IDBRequest::create(scriptState, source, m_transaction.get());
  Vector<char> wireBytes;
  serializedValue->toWireBytes(wireBytes);
  RefPtr<SharedBuffer> valueBuffer = SharedBuffer::adoptVector(wireBytes);

  backendDB()->put(m_transaction->id(), id(), WebData(valueBuffer), blobInfo,
                   key, static_cast<WebIDBPutMode>(putMode),
                   request->createWebCallbacks().release(), indexIds,
                   indexKeys);
  return request;
}

IDBRequest* IDBObjectStore::deleteFunction(ScriptState* scriptState,
                                           const ScriptValue& key,
                                           ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::delete");
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }
  if (m_transaction->isReadOnly()) {
    exceptionState.throwDOMException(
        ReadOnlyError, IDBDatabase::transactionReadOnlyErrorMessage);
    return nullptr;
  }

  IDBKeyRange* keyRange = IDBKeyRange::fromScriptValue(
      scriptState->getExecutionContext(), key, exceptionState);
  if (exceptionState.hadException())
    return nullptr;
  if (!keyRange) {
    exceptionState.throwDOMException(DataError,
                                     IDBDatabase::noKeyOrKeyRangeErrorMessage);
    return nullptr;
  }
  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::create(scriptState, IDBAny::create(this),
                                           m_transaction.get());
  backendDB()->deleteRange(m_transaction->id(), id(), keyRange,
                           request->createWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::clear(ScriptState* scriptState,
                                  ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::clear");
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }
  if (m_transaction->isReadOnly()) {
    exceptionState.throwDOMException(
        ReadOnlyError, IDBDatabase::transactionReadOnlyErrorMessage);
    return nullptr;
  }
  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::create(scriptState, IDBAny::create(this),
                                           m_transaction.get());
  backendDB()->clear(m_transaction->id(), id(),
                     request->createWebCallbacks().release());
  return request;
}

namespace {
// This class creates the index keys for a given index by extracting
// them from the SerializedScriptValue, for all the existing values in
// the objectStore. It only needs to be kept alive by virtue of being
// a listener on an IDBRequest object, in the same way that JavaScript
// cursor success handlers are kept alive.
class IndexPopulator final : public EventListener {
 public:
  static IndexPopulator* create(ScriptState* scriptState,
                                IDBDatabase* database,
                                int64_t transactionId,
                                int64_t objectStoreId,
                                RefPtr<const IDBIndexMetadata> indexMetadata) {
    return new IndexPopulator(scriptState, database, transactionId,
                              objectStoreId, std::move(indexMetadata));
  }

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_database);
    EventListener::trace(visitor);
  }

 private:
  IndexPopulator(ScriptState* scriptState,
                 IDBDatabase* database,
                 int64_t transactionId,
                 int64_t objectStoreId,
                 RefPtr<const IDBIndexMetadata> indexMetadata)
      : EventListener(CPPEventListenerType),
        m_scriptState(scriptState),
        m_database(database),
        m_transactionId(transactionId),
        m_objectStoreId(objectStoreId),
        m_indexMetadata(std::move(indexMetadata)) {
    DCHECK(m_indexMetadata.get());
  }

  const IDBIndexMetadata& indexMetadata() const { return *m_indexMetadata; }

  void handleEvent(ExecutionContext* executionContext, Event* event) override {
    if (!m_scriptState->contextIsValid())
      return;

    DCHECK_EQ(m_scriptState->getExecutionContext(), executionContext);
    DCHECK_EQ(event->type(), EventTypeNames::success);
    EventTarget* target = event->target();
    IDBRequest* request = static_cast<IDBRequest*>(target);

    if (!m_database->backend())  // If database is stopped?
      return;

    ScriptState::Scope scope(m_scriptState.get());

    IDBAny* cursorAny = request->resultAsAny();
    IDBCursorWithValue* cursor = nullptr;
    if (cursorAny->getType() == IDBAny::IDBCursorWithValueType)
      cursor = cursorAny->idbCursorWithValue();

    Vector<int64_t> indexIds;
    indexIds.append(indexMetadata().id);
    if (cursor && !cursor->isDeleted()) {
      cursor->continueFunction(nullptr, nullptr, ASSERT_NO_EXCEPTION);

      IDBKey* primaryKey = cursor->idbPrimaryKey();
      ScriptValue value = cursor->value(m_scriptState.get());

      IndexKeys indexKeys;
      generateIndexKeysForValue(m_scriptState->isolate(), indexMetadata(),
                                value, &indexKeys);

      HeapVector<IndexKeys> indexKeysList;
      indexKeysList.append(indexKeys);

      m_database->backend()->setIndexKeys(m_transactionId, m_objectStoreId,
                                          primaryKey, indexIds, indexKeysList);
    } else {
      // Now that we are done indexing, tell the backend to go
      // back to processing tasks of type NormalTask.
      m_database->backend()->setIndexesReady(m_transactionId, m_objectStoreId,
                                             indexIds);
      m_database.clear();
    }
  }

  RefPtr<ScriptState> m_scriptState;
  Member<IDBDatabase> m_database;
  const int64_t m_transactionId;
  const int64_t m_objectStoreId;
  RefPtr<const IDBIndexMetadata> m_indexMetadata;
};
}  // namespace

IDBIndex* IDBObjectStore::createIndex(ScriptState* scriptState,
                                      const String& name,
                                      const IDBKeyPath& keyPath,
                                      const IDBIndexParameters& options,
                                      ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::createIndex");
  if (!m_transaction->isVersionChange()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        IDBDatabase::notVersionChangeTransactionErrorMessage);
    return nullptr;
  }
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }
  if (containsIndex(name)) {
    exceptionState.throwDOMException(ConstraintError,
                                     IDBDatabase::indexNameTakenErrorMessage);
    return nullptr;
  }
  if (!keyPath.isValid()) {
    exceptionState.throwDOMException(
        SyntaxError, "The keyPath argument contains an invalid key path.");
    return nullptr;
  }
  if (keyPath.getType() == IDBKeyPath::ArrayType && options.multiEntry()) {
    exceptionState.throwDOMException(
        InvalidAccessError,
        "The keyPath argument was an array and the multiEntry option is true.");
    return nullptr;
  }
  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  int64_t indexId = m_metadata->maxIndexId + 1;
  DCHECK_NE(indexId, IDBIndexMetadata::InvalidId);
  backendDB()->createIndex(m_transaction->id(), id(), indexId, name, keyPath,
                           options.unique(), options.multiEntry());

  ++m_metadata->maxIndexId;

  RefPtr<IDBIndexMetadata> indexMetadata = adoptRef(new IDBIndexMetadata(
      name, indexId, keyPath, options.unique(), options.multiEntry()));
  IDBIndex* index = IDBIndex::create(indexMetadata, this, m_transaction.get());
  m_indexMap.set(name, index);
  m_metadata->indexes.set(indexId, indexMetadata);

  DCHECK(!exceptionState.hadException());
  if (exceptionState.hadException())
    return nullptr;

  IDBRequest* indexRequest =
      openCursor(scriptState, nullptr, WebIDBCursorDirectionNext,
                 WebIDBTaskTypePreemptive);
  indexRequest->preventPropagation();

  // This is kept alive by being the success handler of the request, which is in
  // turn kept alive by the owning transaction.
  IndexPopulator* indexPopulator = IndexPopulator::create(
      scriptState, transaction()->db(), m_transaction->id(), id(),
      std::move(indexMetadata));
  indexRequest->setOnsuccess(indexPopulator);
  return index;
}

IDBIndex* IDBObjectStore::index(const String& name,
                                ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::index");
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }

  IDBIndexMap::iterator it = m_indexMap.find(name);
  if (it != m_indexMap.end())
    return it->value;

  int64_t indexId = findIndexId(name);
  if (indexId == IDBIndexMetadata::InvalidId) {
    exceptionState.throwDOMException(NotFoundError,
                                     IDBDatabase::noSuchIndexErrorMessage);
    return nullptr;
  }

  DCHECK(metadata().indexes.contains(indexId));
  RefPtr<IDBIndexMetadata> indexMetadata = metadata().indexes.get(indexId);
  DCHECK(indexMetadata.get());
  IDBIndex* index =
      IDBIndex::create(std::move(indexMetadata), this, m_transaction.get());
  m_indexMap.set(name, index);
  return index;
}

void IDBObjectStore::deleteIndex(const String& name,
                                 ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::deleteIndex");
  if (!m_transaction->isVersionChange()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        IDBDatabase::notVersionChangeTransactionErrorMessage);
    return;
  }
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return;
  }
  int64_t indexId = findIndexId(name);
  if (indexId == IDBIndexMetadata::InvalidId) {
    exceptionState.throwDOMException(NotFoundError,
                                     IDBDatabase::noSuchIndexErrorMessage);
    return;
  }
  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return;
  }

  backendDB()->deleteIndex(m_transaction->id(), id(), indexId);

  m_metadata->indexes.remove(indexId);
  IDBIndexMap::iterator it = m_indexMap.find(name);
  if (it != m_indexMap.end()) {
    m_transaction->indexDeleted(it->value);
    it->value->markDeleted();
    m_indexMap.remove(name);
  }
}

IDBRequest* IDBObjectStore::openCursor(ScriptState* scriptState,
                                       const ScriptValue& range,
                                       const String& directionString,
                                       ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::openCursor");
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }

  WebIDBCursorDirection direction =
      IDBCursor::stringToDirection(directionString);
  IDBKeyRange* keyRange = IDBKeyRange::fromScriptValue(
      scriptState->getExecutionContext(), range, exceptionState);
  if (exceptionState.hadException())
    return nullptr;

  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  return openCursor(scriptState, keyRange, direction, WebIDBTaskTypeNormal);
}

IDBRequest* IDBObjectStore::openCursor(ScriptState* scriptState,
                                       IDBKeyRange* range,
                                       WebIDBCursorDirection direction,
                                       WebIDBTaskType taskType) {
  IDBRequest* request = IDBRequest::create(scriptState, IDBAny::create(this),
                                           m_transaction.get());
  request->setCursorDetails(IndexedDB::CursorKeyAndValue, direction);

  backendDB()->openCursor(m_transaction->id(), id(),
                          IDBIndexMetadata::InvalidId, range, direction, false,
                          taskType, request->createWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::openKeyCursor(ScriptState* scriptState,
                                          const ScriptValue& range,
                                          const String& directionString,
                                          ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::openKeyCursor");
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }

  WebIDBCursorDirection direction =
      IDBCursor::stringToDirection(directionString);
  IDBKeyRange* keyRange = IDBKeyRange::fromScriptValue(
      scriptState->getExecutionContext(), range, exceptionState);
  if (exceptionState.hadException())
    return nullptr;

  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::create(scriptState, IDBAny::create(this),
                                           m_transaction.get());
  request->setCursorDetails(IndexedDB::CursorKeyOnly, direction);

  backendDB()->openCursor(m_transaction->id(), id(),
                          IDBIndexMetadata::InvalidId, keyRange, direction,
                          true, WebIDBTaskTypeNormal,
                          request->createWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::count(ScriptState* scriptState,
                                  const ScriptValue& range,
                                  ExceptionState& exceptionState) {
  IDB_TRACE("IDBObjectStore::count");
  if (isDeleted()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (m_transaction->isFinished() || m_transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }

  IDBKeyRange* keyRange = IDBKeyRange::fromScriptValue(
      scriptState->getExecutionContext(), range, exceptionState);
  if (exceptionState.hadException())
    return nullptr;

  if (!backendDB()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::create(scriptState, IDBAny::create(this),
                                           m_transaction.get());
  backendDB()->count(m_transaction->id(), id(), IDBIndexMetadata::InvalidId,
                     keyRange, request->createWebCallbacks().release());
  return request;
}

void IDBObjectStore::markDeleted() {
  DCHECK(m_transaction->isVersionChange())
      << "An object store got deleted outside a versionchange transaction.";

  m_deleted = true;
  m_metadata->indexes.clear();

  for (auto& it : m_indexMap) {
    IDBIndex* index = it.value;
    index->markDeleted();
  }
}

void IDBObjectStore::clearIndexCache() {
  DCHECK(!m_transaction->isActive() || (isDeleted() && isNewlyCreated()));

// There is no harm in having clearIndexCache() happen multiple times for
// the same object. We assert that it is called once to uncover potential
// object store accounting bugs.
#if DCHECK_IS_ON()
  DCHECK(!m_clearIndexCacheCalled);
  m_clearIndexCacheCalled = true;
#endif  // DCHECK_IS_ON()

  m_indexMap.clear();
}

void IDBObjectStore::revertMetadata(
    RefPtr<IDBObjectStoreMetadata> oldMetadata) {
  DCHECK(m_transaction->isVersionChange());
  DCHECK(!m_transaction->isActive());
  DCHECK(oldMetadata.get());
  DCHECK(id() == oldMetadata->id);

  for (auto& index : m_indexMap.values()) {
    const int64_t indexId = index->id();

    if (index->isNewlyCreated(*oldMetadata)) {
      // The index was created by this transaction. According to the spec,
      // its metadata will remain as-is.
      DCHECK(!oldMetadata->indexes.contains(indexId));
      index->markDeleted();
      continue;
    }

    // The index was created in a previous transaction. We need to revert
    // its metadata. The index might have been deleted, so we
    // unconditionally reset the deletion marker.
    DCHECK(oldMetadata->indexes.contains(indexId));
    RefPtr<IDBIndexMetadata> oldIndexMetadata =
        oldMetadata->indexes.get(indexId);
    index->revertMetadata(std::move(oldIndexMetadata));
  }
  m_metadata = std::move(oldMetadata);

  // An object store's metadata will only get reverted if the index was in the
  // database when the versionchange transaction started.
  m_deleted = false;
}

void IDBObjectStore::revertDeletedIndexMetadata(IDBIndex& deletedIndex) {
  DCHECK(m_transaction->isVersionChange());
  DCHECK(!m_transaction->isActive());
  DCHECK(deletedIndex.objectStore() == this);
  DCHECK(deletedIndex.isDeleted());

  const int64_t indexId = deletedIndex.id();
  DCHECK(m_metadata->indexes.contains(indexId))
      << "The object store's metadata was not correctly reverted";
  RefPtr<IDBIndexMetadata> oldIndexMetadata = m_metadata->indexes.get(indexId);
  deletedIndex.revertMetadata(std::move(oldIndexMetadata));
}

void IDBObjectStore::renameIndex(int64_t indexId, const String& newName) {
  DCHECK(m_transaction->isVersionChange());
  DCHECK(m_transaction->isActive());

  backendDB()->renameIndex(m_transaction->id(), id(), indexId, newName);

  auto metadataIterator = m_metadata->indexes.find(indexId);
  DCHECK_NE(metadataIterator, m_metadata->indexes.end()) << "Invalid indexId";
  const String& oldName = metadataIterator->value->name;

  DCHECK(m_indexMap.contains(oldName))
      << "The index had to be accessed in order to be renamed.";
  DCHECK(!m_indexMap.contains(newName));
  m_indexMap.set(newName, m_indexMap.take(oldName));

  metadataIterator->value->name = newName;
}

int64_t IDBObjectStore::findIndexId(const String& name) const {
  for (const auto& it : metadata().indexes) {
    if (it.value->name == name) {
      DCHECK_NE(it.key, IDBIndexMetadata::InvalidId);
      return it.key;
    }
  }
  return IDBIndexMetadata::InvalidId;
}

WebIDBDatabase* IDBObjectStore::backendDB() const {
  return m_transaction->backendDB();
}

}  // namespace blink
