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

#include "modules/indexeddb/IDBDatabase.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventQueue.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBEventDispatcher.h"
#include "modules/indexeddb/IDBIndex.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IDBVersionChangeEvent.h"
#include "modules/indexeddb/WebIDBDatabaseCallbacksImpl.h"
#include "platform/Histogram.h"
#include "public/platform/modules/indexeddb/WebIDBKeyPath.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "wtf/Atomics.h"
#include <limits>
#include <memory>

using blink::WebIDBDatabase;

namespace blink {

const char IDBDatabase::indexDeletedErrorMessage[] =
    "The index or its object store has been deleted.";
const char IDBDatabase::indexNameTakenErrorMessage[] =
    "An index with the specified name already exists.";
const char IDBDatabase::isKeyCursorErrorMessage[] =
    "The cursor is a key cursor.";
const char IDBDatabase::noKeyOrKeyRangeErrorMessage[] =
    "No key or key range specified.";
const char IDBDatabase::noSuchIndexErrorMessage[] =
    "The specified index was not found.";
const char IDBDatabase::noSuchObjectStoreErrorMessage[] =
    "The specified object store was not found.";
const char IDBDatabase::noValueErrorMessage[] =
    "The cursor is being iterated or has iterated past its end.";
const char IDBDatabase::notValidKeyErrorMessage[] =
    "The parameter is not a valid key.";
const char IDBDatabase::notVersionChangeTransactionErrorMessage[] =
    "The database is not running a version change transaction.";
const char IDBDatabase::objectStoreDeletedErrorMessage[] =
    "The object store has been deleted.";
const char IDBDatabase::objectStoreNameTakenErrorMessage[] =
    "An object store with the specified name already exists.";
const char IDBDatabase::requestNotFinishedErrorMessage[] =
    "The request has not finished.";
const char IDBDatabase::sourceDeletedErrorMessage[] =
    "The cursor's source or effective object store has been deleted.";
const char IDBDatabase::transactionInactiveErrorMessage[] =
    "The transaction is not active.";
const char IDBDatabase::transactionFinishedErrorMessage[] =
    "The transaction has finished.";
const char IDBDatabase::transactionReadOnlyErrorMessage[] =
    "The transaction is read-only.";
const char IDBDatabase::databaseClosedErrorMessage[] =
    "The database connection is closed.";

IDBDatabase* IDBDatabase::create(ExecutionContext* context,
                                 std::unique_ptr<WebIDBDatabase> database,
                                 IDBDatabaseCallbacks* callbacks) {
  IDBDatabase* idbDatabase =
      new IDBDatabase(context, std::move(database), callbacks);
  idbDatabase->suspendIfNeeded();
  return idbDatabase;
}

IDBDatabase::IDBDatabase(ExecutionContext* context,
                         std::unique_ptr<WebIDBDatabase> backend,
                         IDBDatabaseCallbacks* callbacks)
    : ActiveScriptWrappable(this),
      ActiveDOMObject(context),
      m_backend(std::move(backend)),
      m_databaseCallbacks(callbacks) {
  m_databaseCallbacks->connect(this);
}

IDBDatabase::~IDBDatabase() {
  if (!m_closePending && m_backend)
    m_backend->close();
}

DEFINE_TRACE(IDBDatabase) {
  visitor->trace(m_versionChangeTransaction);
  visitor->trace(m_transactions);
  visitor->trace(m_enqueuedEvents);
  visitor->trace(m_databaseCallbacks);
  EventTargetWithInlineData::trace(visitor);
  ActiveDOMObject::trace(visitor);
}

int64_t IDBDatabase::nextTransactionId() {
  // Only keep a 32-bit counter to allow ports to use the other 32
  // bits of the id.
  static int currentTransactionId = 0;
  return atomicIncrement(&currentTransactionId);
}

void IDBDatabase::setMetadata(const IDBDatabaseMetadata& metadata) {
  m_metadata = metadata;
}

void IDBDatabase::setDatabaseMetadata(const IDBDatabaseMetadata& metadata) {
  m_metadata.copyFrom(metadata);
}

void IDBDatabase::transactionCreated(IDBTransaction* transaction) {
  DCHECK(transaction);
  DCHECK(!m_transactions.contains(transaction->id()));
  m_transactions.add(transaction->id(), transaction);

  if (transaction->isVersionChange()) {
    DCHECK(!m_versionChangeTransaction);
    m_versionChangeTransaction = transaction;
  }
}

void IDBDatabase::transactionFinished(const IDBTransaction* transaction) {
  DCHECK(transaction);
  DCHECK(m_transactions.contains(transaction->id()));
  DCHECK_EQ(m_transactions.get(transaction->id()), transaction);
  m_transactions.remove(transaction->id());

  if (transaction->isVersionChange()) {
    DCHECK_EQ(m_versionChangeTransaction, transaction);
    m_versionChangeTransaction = nullptr;
  }

  if (m_closePending && m_transactions.isEmpty())
    closeConnection();
}

void IDBDatabase::onAbort(int64_t transactionId, DOMException* error) {
  DCHECK(m_transactions.contains(transactionId));
  m_transactions.get(transactionId)->onAbort(error);
}

void IDBDatabase::onComplete(int64_t transactionId) {
  DCHECK(m_transactions.contains(transactionId));
  m_transactions.get(transactionId)->onComplete();
}

DOMStringList* IDBDatabase::objectStoreNames() const {
  DOMStringList* objectStoreNames =
      DOMStringList::create(DOMStringList::IndexedDB);
  for (const auto& it : m_metadata.objectStores)
    objectStoreNames->append(it.value->name);
  objectStoreNames->sort();
  return objectStoreNames;
}

const String& IDBDatabase::getObjectStoreName(int64_t objectStoreId) const {
  const auto& it = m_metadata.objectStores.find(objectStoreId);
  DCHECK(it != m_metadata.objectStores.end());
  return it->value->name;
}

IDBObjectStore* IDBDatabase::createObjectStore(const String& name,
                                               const IDBKeyPath& keyPath,
                                               bool autoIncrement,
                                               ExceptionState& exceptionState) {
  IDB_TRACE("IDBDatabase::createObjectStore");
  recordApiCallsHistogram(IDBCreateObjectStoreCall);

  if (!m_versionChangeTransaction) {
    exceptionState.throwDOMException(
        InvalidStateError,
        IDBDatabase::notVersionChangeTransactionErrorMessage);
    return nullptr;
  }
  if (m_versionChangeTransaction->isFinished() ||
      m_versionChangeTransaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }
  if (!m_versionChangeTransaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return nullptr;
  }

  if (!keyPath.isNull() && !keyPath.isValid()) {
    exceptionState.throwDOMException(
        SyntaxError, "The keyPath option is not a valid key path.");
    return nullptr;
  }

  if (containsObjectStore(name)) {
    exceptionState.throwDOMException(
        ConstraintError, IDBDatabase::objectStoreNameTakenErrorMessage);
    return nullptr;
  }

  if (autoIncrement && ((keyPath.getType() == IDBKeyPath::StringType &&
                         keyPath.string().isEmpty()) ||
                        keyPath.getType() == IDBKeyPath::ArrayType)) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The autoIncrement option was set but the "
                                     "keyPath option was empty or an array.");
    return nullptr;
  }

  if (!m_backend) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  int64_t objectStoreId = m_metadata.maxObjectStoreId + 1;
  DCHECK_NE(objectStoreId, IDBObjectStoreMetadata::InvalidId);
  m_backend->createObjectStore(m_versionChangeTransaction->id(), objectStoreId,
                               name, keyPath, autoIncrement);

  RefPtr<IDBObjectStoreMetadata> storeMetadata = adoptRef(
      new IDBObjectStoreMetadata(name, objectStoreId, keyPath, autoIncrement,
                                 WebIDBDatabase::minimumIndexId));
  IDBObjectStore* objectStore =
      IDBObjectStore::create(storeMetadata, m_versionChangeTransaction.get());
  m_versionChangeTransaction->objectStoreCreated(name, objectStore);
  m_metadata.objectStores.set(objectStoreId, std::move(storeMetadata));
  ++m_metadata.maxObjectStoreId;

  return objectStore;
}

void IDBDatabase::deleteObjectStore(const String& name,
                                    ExceptionState& exceptionState) {
  IDB_TRACE("IDBDatabase::deleteObjectStore");
  recordApiCallsHistogram(IDBDeleteObjectStoreCall);
  if (!m_versionChangeTransaction) {
    exceptionState.throwDOMException(
        InvalidStateError,
        IDBDatabase::notVersionChangeTransactionErrorMessage);
    return;
  }
  if (m_versionChangeTransaction->isFinished() ||
      m_versionChangeTransaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return;
  }
  if (!m_versionChangeTransaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return;
  }

  int64_t objectStoreId = findObjectStoreId(name);
  if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
    exceptionState.throwDOMException(
        NotFoundError, "The specified object store was not found.");
    return;
  }

  if (!m_backend) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return;
  }

  m_backend->deleteObjectStore(m_versionChangeTransaction->id(), objectStoreId);
  m_versionChangeTransaction->objectStoreDeleted(objectStoreId, name);
  m_metadata.objectStores.remove(objectStoreId);
}

IDBTransaction* IDBDatabase::transaction(
    ScriptState* scriptState,
    const StringOrStringSequenceOrDOMStringList& storeNames,
    const String& modeString,
    ExceptionState& exceptionState) {
  IDB_TRACE("IDBDatabase::transaction");
  recordApiCallsHistogram(IDBTransactionCall);

  HashSet<String> scope;
  if (storeNames.isString()) {
    scope.add(storeNames.getAsString());
  } else if (storeNames.isStringSequence()) {
    for (const String& name : storeNames.getAsStringSequence())
      scope.add(name);
  } else if (storeNames.isDOMStringList()) {
    const Vector<String>& list = *storeNames.getAsDOMStringList();
    for (const String& name : list)
      scope.add(name);
  } else {
    NOTREACHED();
  }

  if (m_versionChangeTransaction) {
    exceptionState.throwDOMException(
        InvalidStateError, "A version change transaction is running.");
    return nullptr;
  }

  if (m_closePending) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "The database connection is closing.");
    return nullptr;
  }

  if (!m_backend) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return nullptr;
  }

  if (scope.isEmpty()) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The storeNames parameter was empty.");
    return nullptr;
  }

  Vector<int64_t> objectStoreIds;
  for (const String& name : scope) {
    int64_t objectStoreId = findObjectStoreId(name);
    if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
      exceptionState.throwDOMException(
          NotFoundError, "One of the specified object stores was not found.");
      return nullptr;
    }
    objectStoreIds.append(objectStoreId);
  }

  WebIDBTransactionMode mode = IDBTransaction::stringToMode(modeString);
  if (mode != WebIDBTransactionModeReadOnly &&
      mode != WebIDBTransactionModeReadWrite) {
    exceptionState.throwTypeError(
        "The mode provided ('" + modeString +
        "') is not one of 'readonly' or 'readwrite'.");
    return nullptr;
  }

  int64_t transactionId = nextTransactionId();
  m_backend->createTransaction(transactionId, objectStoreIds, mode);

  return IDBTransaction::createNonVersionChange(scriptState, transactionId,
                                                scope, mode, this);
}

void IDBDatabase::forceClose() {
  for (const auto& it : m_transactions)
    it.value->abort(IGNORE_EXCEPTION);
  this->close();
  enqueueEvent(Event::create(EventTypeNames::close));
}

void IDBDatabase::close() {
  IDB_TRACE("IDBDatabase::close");
  if (m_closePending)
    return;

  m_closePending = true;

  if (m_transactions.isEmpty())
    closeConnection();
}

void IDBDatabase::closeConnection() {
  DCHECK(m_closePending);
  DCHECK(m_transactions.isEmpty());

  if (m_backend) {
    m_backend->close();
    m_backend.reset();
  }

  if (m_databaseCallbacks)
    m_databaseCallbacks->detachWebCallbacks();

  if (!getExecutionContext())
    return;

  EventQueue* eventQueue = getExecutionContext()->getEventQueue();
  // Remove any pending versionchange events scheduled to fire on this
  // connection. They would have been scheduled by the backend when another
  // connection attempted an upgrade, but the frontend connection is being
  // closed before they could fire.
  for (size_t i = 0; i < m_enqueuedEvents.size(); ++i) {
    bool removed = eventQueue->cancelEvent(m_enqueuedEvents[i].get());
    DCHECK(removed);
  }
}

void IDBDatabase::onVersionChange(int64_t oldVersion, int64_t newVersion) {
  IDB_TRACE("IDBDatabase::onVersionChange");
  if (!getExecutionContext())
    return;

  if (m_closePending) {
    // If we're pending, that means there's a busy transaction. We won't
    // fire 'versionchange' but since we're not closing immediately the
    // back-end should still send out 'blocked'.
    m_backend->versionChangeIgnored();
    return;
  }

  Nullable<unsigned long long> newVersionNullable =
      (newVersion == IDBDatabaseMetadata::NoVersion)
          ? Nullable<unsigned long long>()
          : Nullable<unsigned long long>(newVersion);
  enqueueEvent(IDBVersionChangeEvent::create(EventTypeNames::versionchange,
                                             oldVersion, newVersionNullable));
}

void IDBDatabase::enqueueEvent(Event* event) {
  DCHECK(getExecutionContext());
  EventQueue* eventQueue = getExecutionContext()->getEventQueue();
  event->setTarget(this);
  eventQueue->enqueueEvent(event);
  m_enqueuedEvents.append(event);
}

DispatchEventResult IDBDatabase::dispatchEventInternal(Event* event) {
  IDB_TRACE("IDBDatabase::dispatchEvent");
  if (!getExecutionContext())
    return DispatchEventResult::CanceledBeforeDispatch;
  DCHECK(event->type() == EventTypeNames::versionchange ||
         event->type() == EventTypeNames::close);
  for (size_t i = 0; i < m_enqueuedEvents.size(); ++i) {
    if (m_enqueuedEvents[i].get() == event)
      m_enqueuedEvents.remove(i);
  }

  DispatchEventResult dispatchResult =
      EventTarget::dispatchEventInternal(event);
  if (event->type() == EventTypeNames::versionchange && !m_closePending &&
      m_backend)
    m_backend->versionChangeIgnored();
  return dispatchResult;
}

int64_t IDBDatabase::findObjectStoreId(const String& name) const {
  for (const auto& it : m_metadata.objectStores) {
    if (it.value->name == name) {
      DCHECK_NE(it.key, IDBObjectStoreMetadata::InvalidId);
      return it.key;
    }
  }
  return IDBObjectStoreMetadata::InvalidId;
}

void IDBDatabase::renameObjectStore(int64_t objectStoreId,
                                    const String& newName) {
  DCHECK(m_versionChangeTransaction)
      << "Object store renamed on database without a versionchange transaction";
  DCHECK(m_versionChangeTransaction->isActive())
      << "Object store renamed when versionchange transaction is not active";
  DCHECK(m_backend) << "Object store renamed after database connection closed";
  DCHECK(m_metadata.objectStores.contains(objectStoreId));

  m_backend->renameObjectStore(m_versionChangeTransaction->id(), objectStoreId,
                               newName);

  IDBObjectStoreMetadata* objectStoreMetadata =
      m_metadata.objectStores.get(objectStoreId);
  m_versionChangeTransaction->objectStoreRenamed(objectStoreMetadata->name,
                                                 newName);
  objectStoreMetadata->name = newName;
}

void IDBDatabase::revertObjectStoreCreation(int64_t objectStoreId) {
  DCHECK(m_versionChangeTransaction) << "Object store metadata reverted on "
                                        "database without a versionchange "
                                        "transaction";
  DCHECK(!m_versionChangeTransaction->isActive())
      << "Object store metadata reverted when versionchange transaction is "
         "still active";
  DCHECK(m_metadata.objectStores.contains(objectStoreId));
  m_metadata.objectStores.remove(objectStoreId);
}

void IDBDatabase::revertObjectStoreMetadata(
    RefPtr<IDBObjectStoreMetadata> oldMetadata) {
  DCHECK(m_versionChangeTransaction) << "Object store metadata reverted on "
                                        "database without a versionchange "
                                        "transaction";
  DCHECK(!m_versionChangeTransaction->isActive())
      << "Object store metadata reverted when versionchange transaction is "
         "still active";
  DCHECK(oldMetadata.get());
  m_metadata.objectStores.set(oldMetadata->id, std::move(oldMetadata));
}

bool IDBDatabase::hasPendingActivity() const {
  // The script wrapper must not be collected before the object is closed or
  // we can't fire a "versionchange" event to let script manually close the
  // connection.
  return !m_closePending && hasEventListeners() && getExecutionContext();
}

void IDBDatabase::contextDestroyed() {
  // Immediately close the connection to the back end. Don't attempt a
  // normal close() since that may wait on transactions which require a
  // round trip to the back-end to abort.
  if (m_backend) {
    m_backend->close();
    m_backend.reset();
  }

  if (m_databaseCallbacks)
    m_databaseCallbacks->detachWebCallbacks();
}

const AtomicString& IDBDatabase::interfaceName() const {
  return EventTargetNames::IDBDatabase;
}

ExecutionContext* IDBDatabase::getExecutionContext() const {
  return ActiveDOMObject::getExecutionContext();
}

void IDBDatabase::recordApiCallsHistogram(IndexedDatabaseMethods method) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, apiCallsHistogram,
      new EnumerationHistogram("WebCore.IndexedDB.FrontEndAPICalls",
                               IDBMethodsMax));
  apiCallsHistogram.count(method);
}

}  // namespace blink
