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

#include "modules/indexeddb/IDBTransaction.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventQueue.h"
#include "modules/IndexedDBNames.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBEventDispatcher.h"
#include "modules/indexeddb/IDBIndex.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"
#include "modules/indexeddb/IDBTracing.h"
#include "wtf/PtrUtil.h"
#include <memory>

using blink::WebIDBDatabase;

namespace blink {

IDBTransaction* IDBTransaction::createNonVersionChange(
    ScriptState* scriptState,
    int64_t id,
    const HashSet<String>& scope,
    WebIDBTransactionMode mode,
    IDBDatabase* db) {
  DCHECK_NE(mode, WebIDBTransactionModeVersionChange);
  DCHECK(!scope.isEmpty()) << "Non-version transactions should operate on a "
                              "well-defined set of stores";
  IDBTransaction* transaction =
      new IDBTransaction(scriptState, id, scope, mode, db);
  transaction->suspendIfNeeded();
  return transaction;
}

IDBTransaction* IDBTransaction::createVersionChange(
    ExecutionContext* executionContext,
    int64_t id,
    IDBDatabase* db,
    IDBOpenDBRequest* openDBRequest,
    const IDBDatabaseMetadata& oldMetadata) {
  IDBTransaction* transaction =
      new IDBTransaction(executionContext, id, db, openDBRequest, oldMetadata);
  transaction->suspendIfNeeded();
  return transaction;
}

namespace {

class DeactivateTransactionTask : public V8PerIsolateData::EndOfScopeTask {
 public:
  static std::unique_ptr<DeactivateTransactionTask> create(
      IDBTransaction* transaction) {
    return wrapUnique(new DeactivateTransactionTask(transaction));
  }

  void run() override {
    m_transaction->setActive(false);
    m_transaction.clear();
  }

 private:
  explicit DeactivateTransactionTask(IDBTransaction* transaction)
      : m_transaction(transaction) {}

  Persistent<IDBTransaction> m_transaction;
};

}  // namespace

IDBTransaction::IDBTransaction(ScriptState* scriptState,
                               int64_t id,
                               const HashSet<String>& scope,
                               WebIDBTransactionMode mode,
                               IDBDatabase* db)
    : ActiveScriptWrappable(this),
      ActiveDOMObject(scriptState->getExecutionContext()),
      m_id(id),
      m_database(db),
      m_mode(mode),
      m_scope(scope) {
  DCHECK(m_database);
  DCHECK(!m_scope.isEmpty()) << "Non-versionchange transactions must operate "
                                "on a well-defined set of stores";
  DCHECK(m_mode == WebIDBTransactionModeReadOnly ||
         m_mode == WebIDBTransactionModeReadWrite)
      << "Invalid transaction mode";

  DCHECK_EQ(m_state, Active);
  V8PerIsolateData::from(scriptState->isolate())
      ->addEndOfScopeTask(DeactivateTransactionTask::create(this));

  m_database->transactionCreated(this);
}

IDBTransaction::IDBTransaction(ExecutionContext* executionContext,
                               int64_t id,
                               IDBDatabase* db,
                               IDBOpenDBRequest* openDBRequest,
                               const IDBDatabaseMetadata& oldMetadata)
    : ActiveScriptWrappable(this),
      ActiveDOMObject(executionContext),
      m_id(id),
      m_database(db),
      m_openDBRequest(openDBRequest),
      m_mode(WebIDBTransactionModeVersionChange),
      m_state(Inactive),
      m_oldDatabaseMetadata(oldMetadata),
      m_scope() {
  DCHECK(m_database);
  DCHECK(m_openDBRequest);
  DCHECK(m_scope.isEmpty());

  m_database->transactionCreated(this);
}

IDBTransaction::~IDBTransaction() {
  DCHECK(m_state == Finished || !getExecutionContext());
  DCHECK(m_requestList.isEmpty() || !getExecutionContext());
}

DEFINE_TRACE(IDBTransaction) {
  visitor->trace(m_database);
  visitor->trace(m_openDBRequest);
  visitor->trace(m_error);
  visitor->trace(m_requestList);
  visitor->trace(m_objectStoreMap);
  visitor->trace(m_oldStoreMetadata);
  visitor->trace(m_deletedIndexes);
  EventTargetWithInlineData::trace(visitor);
  ActiveDOMObject::trace(visitor);
}

void IDBTransaction::setError(DOMException* error) {
  DCHECK_NE(m_state, Finished);
  DCHECK(error);

  // The first error to be set is the true cause of the
  // transaction abort.
  if (!m_error)
    m_error = error;
}

IDBObjectStore* IDBTransaction::objectStore(const String& name,
                                            ExceptionState& exceptionState) {
  if (isFinished()) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::transactionFinishedErrorMessage);
    return nullptr;
  }

  IDBObjectStoreMap::iterator it = m_objectStoreMap.find(name);
  if (it != m_objectStoreMap.end())
    return it->value;

  if (!isVersionChange() && !m_scope.contains(name)) {
    exceptionState.throwDOMException(
        NotFoundError, IDBDatabase::noSuchObjectStoreErrorMessage);
    return nullptr;
  }

  int64_t objectStoreId = m_database->findObjectStoreId(name);
  if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
    DCHECK(isVersionChange());
    exceptionState.throwDOMException(
        NotFoundError, IDBDatabase::noSuchObjectStoreErrorMessage);
    return nullptr;
  }

  DCHECK(m_database->metadata().objectStores.contains(objectStoreId));
  RefPtr<IDBObjectStoreMetadata> objectStoreMetadata =
      m_database->metadata().objectStores.get(objectStoreId);
  DCHECK(objectStoreMetadata.get());

  IDBObjectStore* objectStore =
      IDBObjectStore::create(std::move(objectStoreMetadata), this);
  DCHECK(!m_objectStoreMap.contains(name));
  m_objectStoreMap.set(name, objectStore);

  if (isVersionChange()) {
    DCHECK(!objectStore->isNewlyCreated())
        << "Object store IDs are not assigned sequentially";
    RefPtr<IDBObjectStoreMetadata> backupMetadata =
        objectStore->metadata().createCopy();
    m_oldStoreMetadata.set(objectStore, std::move(backupMetadata));
  }
  return objectStore;
}

void IDBTransaction::objectStoreCreated(const String& name,
                                        IDBObjectStore* objectStore) {
  DCHECK_NE(m_state, Finished)
      << "A finished transaction created an object store";
  DCHECK_EQ(m_mode, WebIDBTransactionModeVersionChange)
      << "A non-versionchange transaction created an object store";
  DCHECK(!m_objectStoreMap.contains(name))
      << "An object store was created with the name of an existing store";
  DCHECK(objectStore->isNewlyCreated())
      << "Object store IDs are not assigned sequentially";
  m_objectStoreMap.set(name, objectStore);
}

void IDBTransaction::objectStoreDeleted(const int64_t objectStoreId,
                                        const String& name) {
  DCHECK_NE(m_state, Finished)
      << "A finished transaction deleted an object store";
  DCHECK_EQ(m_mode, WebIDBTransactionModeVersionChange)
      << "A non-versionchange transaction deleted an object store";
  IDBObjectStoreMap::iterator it = m_objectStoreMap.find(name);
  if (it == m_objectStoreMap.end()) {
    // No IDBObjectStore instance was created for the deleted store in this
    // transaction. This happens if IDBDatabase.deleteObjectStore() is called
    // with the name of a store that wasn't instantated. We need to be able to
    // revert the metadata change if the transaction aborts, in order to return
    // correct values from IDB{Database, Transaction}.objectStoreNames.
    DCHECK(m_database->metadata().objectStores.contains(objectStoreId));
    RefPtr<IDBObjectStoreMetadata> metadata =
        m_database->metadata().objectStores.get(objectStoreId);
    DCHECK(metadata.get());
    DCHECK_EQ(metadata->name, name);
    m_deletedObjectStores.append(std::move(metadata));
  } else {
    IDBObjectStore* objectStore = it->value;
    m_objectStoreMap.remove(name);
    objectStore->markDeleted();
    if (objectStore->id() > m_oldDatabaseMetadata.maxObjectStoreId) {
      // The store was created and deleted in this transaction, so it will
      // not be restored even if the transaction aborts. We have just
      // removed our last reference to it.
      DCHECK(!m_oldStoreMetadata.contains(objectStore));
      objectStore->clearIndexCache();
    } else {
      // The store was created before this transaction, and we created an
      // IDBObjectStore instance for it. When that happened, we must have
      // snapshotted the store's metadata as well.
      DCHECK(m_oldStoreMetadata.contains(objectStore));
    }
  }
}

void IDBTransaction::objectStoreRenamed(const String& oldName,
                                        const String& newName) {
  DCHECK_NE(m_state, Finished)
      << "A finished transaction renamed an object store";
  DCHECK_EQ(m_mode, WebIDBTransactionModeVersionChange)
      << "A non-versionchange transaction renamed an object store";

  DCHECK(!m_objectStoreMap.contains(newName));
  DCHECK(m_objectStoreMap.contains(oldName))
      << "The object store had to be accessed in order to be renamed.";
  m_objectStoreMap.set(newName, m_objectStoreMap.take(oldName));
}

void IDBTransaction::indexDeleted(IDBIndex* index) {
  DCHECK(index);
  DCHECK(!index->isDeleted()) << "indexDeleted called twice for the same index";

  IDBObjectStore* objectStore = index->objectStore();
  DCHECK_EQ(objectStore->transaction(), this);
  DCHECK(m_objectStoreMap.contains(objectStore->name()))
      << "An index was deleted without accessing its object store";

  const auto& objectStoreIterator = m_oldStoreMetadata.find(objectStore);
  if (objectStoreIterator == m_oldStoreMetadata.end()) {
    // The index's object store was created in this transaction, so this
    // index was also created (and deleted) in this transaction, and will
    // not be restored if the transaction aborts.
    //
    // Subtle proof for the first sentence above: Deleting an index requires
    // calling deleteIndex() on the store's IDBObjectStore instance.
    // Whenever we create an IDBObjectStore instance for a previously
    // created store, we snapshot the store's metadata. So, deleting an
    // index of an "old" store can only be done after the store's metadata
    // is snapshotted.
    return;
  }

  const IDBObjectStoreMetadata* oldStoreMetadata =
      objectStoreIterator->value.get();
  DCHECK(oldStoreMetadata);
  if (!oldStoreMetadata->indexes.contains(index->id())) {
    // The index's object store was created before this transaction, but the
    // index was created (and deleted) in this transaction, so it will not
    // be restored if the transaction aborts.
    return;
  }

  m_deletedIndexes.append(index);
}

void IDBTransaction::setActive(bool active) {
  DCHECK_NE(m_state, Finished) << "A finished transaction tried to setActive("
                               << (active ? "true" : "false") << ")";
  if (m_state == Finishing)
    return;
  DCHECK_NE(active, (m_state == Active));
  m_state = active ? Active : Inactive;

  if (!active && m_requestList.isEmpty() && backendDB())
    backendDB()->commit(m_id);
}

void IDBTransaction::abort(ExceptionState& exceptionState) {
  if (m_state == Finishing || m_state == Finished) {
    exceptionState.throwDOMException(
        InvalidStateError, IDBDatabase::transactionFinishedErrorMessage);
    return;
  }

  m_state = Finishing;

  if (!getExecutionContext())
    return;

  abortOutstandingRequests();
  revertDatabaseMetadata();

  if (backendDB())
    backendDB()->abort(m_id);
}

void IDBTransaction::registerRequest(IDBRequest* request) {
  DCHECK(request);
  DCHECK_EQ(m_state, Active);
  m_requestList.add(request);
}

void IDBTransaction::unregisterRequest(IDBRequest* request) {
  DCHECK(request);
  // If we aborted the request, it will already have been removed.
  m_requestList.remove(request);
}

void IDBTransaction::onAbort(DOMException* error) {
  IDB_TRACE("IDBTransaction::onAbort");
  if (!getExecutionContext()) {
    finished();
    return;
  }

  DCHECK_NE(m_state, Finished);
  if (m_state != Finishing) {
    // Abort was not triggered by front-end.
    DCHECK(error);
    setError(error);

    abortOutstandingRequests();
    revertDatabaseMetadata();

    m_state = Finishing;
  }

  if (isVersionChange())
    m_database->close();

  // Enqueue events before notifying database, as database may close which
  // enqueues more events and order matters.
  enqueueEvent(Event::createBubble(EventTypeNames::abort));
  finished();
}

void IDBTransaction::onComplete() {
  IDB_TRACE("IDBTransaction::onComplete");
  if (!getExecutionContext()) {
    finished();
    return;
  }

  DCHECK_NE(m_state, Finished);
  m_state = Finishing;

  // Enqueue events before notifying database, as database may close which
  // enqueues more events and order matters.
  enqueueEvent(Event::create(EventTypeNames::complete));
  finished();
}

bool IDBTransaction::hasPendingActivity() const {
  // FIXME: In an ideal world, we should return true as long as anyone has a or
  // can get a handle to us or any child request object and any of those have
  // event listeners. This is  in order to handle user generated events
  // properly.
  return m_hasPendingActivity && getExecutionContext();
}

WebIDBTransactionMode IDBTransaction::stringToMode(const String& modeString) {
  if (modeString == IndexedDBNames::readonly)
    return WebIDBTransactionModeReadOnly;
  if (modeString == IndexedDBNames::readwrite)
    return WebIDBTransactionModeReadWrite;
  if (modeString == IndexedDBNames::versionchange)
    return WebIDBTransactionModeVersionChange;
  NOTREACHED();
  return WebIDBTransactionModeReadOnly;
}

WebIDBDatabase* IDBTransaction::backendDB() const {
  return m_database->backend();
}

const String& IDBTransaction::mode() const {
  switch (m_mode) {
    case WebIDBTransactionModeReadOnly:
      return IndexedDBNames::readonly;

    case WebIDBTransactionModeReadWrite:
      return IndexedDBNames::readwrite;

    case WebIDBTransactionModeVersionChange:
      return IndexedDBNames::versionchange;
  }

  NOTREACHED();
  return IndexedDBNames::readonly;
}

DOMStringList* IDBTransaction::objectStoreNames() const {
  if (isVersionChange())
    return m_database->objectStoreNames();

  DOMStringList* objectStoreNames =
      DOMStringList::create(DOMStringList::IndexedDB);
  for (const String& objectStoreName : m_scope)
    objectStoreNames->append(objectStoreName);
  objectStoreNames->sort();
  return objectStoreNames;
}

const AtomicString& IDBTransaction::interfaceName() const {
  return EventTargetNames::IDBTransaction;
}

ExecutionContext* IDBTransaction::getExecutionContext() const {
  return ActiveDOMObject::getExecutionContext();
}

DispatchEventResult IDBTransaction::dispatchEventInternal(Event* event) {
  IDB_TRACE("IDBTransaction::dispatchEvent");
  if (!getExecutionContext()) {
    m_state = Finished;
    return DispatchEventResult::CanceledBeforeDispatch;
  }
  DCHECK_NE(m_state, Finished);
  DCHECK(m_hasPendingActivity);
  DCHECK(getExecutionContext());
  DCHECK_EQ(event->target(), this);
  m_state = Finished;

  HeapVector<Member<EventTarget>> targets;
  targets.append(this);
  targets.append(db());

  // FIXME: When we allow custom event dispatching, this will probably need to
  // change.
  DCHECK(event->type() == EventTypeNames::complete ||
         event->type() == EventTypeNames::abort);
  DispatchEventResult dispatchResult =
      IDBEventDispatcher::dispatch(event, targets);
  // FIXME: Try to construct a test where |this| outlives openDBRequest and we
  // get a crash.
  if (m_openDBRequest) {
    DCHECK(isVersionChange());
    m_openDBRequest->transactionDidFinishAndDispatch();
  }
  m_hasPendingActivity = false;
  return dispatchResult;
}

void IDBTransaction::enqueueEvent(Event* event) {
  DCHECK_NE(m_state, Finished)
      << "A finished transaction tried to enqueue an event of type "
      << event->type() << ".";
  if (!getExecutionContext())
    return;

  EventQueue* eventQueue = getExecutionContext()->getEventQueue();
  event->setTarget(this);
  eventQueue->enqueueEvent(event);
}

void IDBTransaction::abortOutstandingRequests() {
  for (IDBRequest* request : m_requestList)
    request->abort();
  m_requestList.clear();
}

void IDBTransaction::revertDatabaseMetadata() {
  DCHECK_NE(m_state, Active);
  if (!isVersionChange())
    return;

  // Mark stores created by this transaction as deleted.
  for (auto& objectStore : m_objectStoreMap.values()) {
    const int64_t objectStoreId = objectStore->id();
    if (!objectStore->isNewlyCreated()) {
      DCHECK(m_oldStoreMetadata.contains(objectStore));
      continue;
    }

    DCHECK(!m_oldStoreMetadata.contains(objectStore));
    m_database->revertObjectStoreCreation(objectStoreId);
    objectStore->markDeleted();
  }

  for (auto& it : m_oldStoreMetadata) {
    IDBObjectStore* objectStore = it.key;
    RefPtr<IDBObjectStoreMetadata> oldMetadata = it.value;

    m_database->revertObjectStoreMetadata(oldMetadata);
    objectStore->revertMetadata(oldMetadata);
  }
  for (auto& index : m_deletedIndexes)
    index->objectStore()->revertDeletedIndexMetadata(*index);
  for (auto& oldMedata : m_deletedObjectStores)
    m_database->revertObjectStoreMetadata(std::move(oldMedata));

  // We only need to revert the database's own metadata because we have reverted
  // the metadata for the database's object stores above.
  m_database->setDatabaseMetadata(m_oldDatabaseMetadata);
}

void IDBTransaction::finished() {
#if DCHECK_IS_ON()
  DCHECK(!m_finishCalled);
  m_finishCalled = true;
#endif  // DCHECK_IS_ON()

  m_database->transactionFinished(this);

  // Remove references to the IDBObjectStore and IDBIndex instances held by
  // this transaction, so Oilpan can garbage-collect the instances that aren't
  // used by JavaScript.

  for (auto& it : m_objectStoreMap) {
    IDBObjectStore* objectStore = it.value;
    if (!isVersionChange() || objectStore->isNewlyCreated()) {
      DCHECK(!m_oldStoreMetadata.contains(objectStore));
      objectStore->clearIndexCache();
    } else {
      // We'll call clearIndexCache() on this store in the loop below.
      DCHECK(m_oldStoreMetadata.contains(objectStore));
    }
  }
  m_objectStoreMap.clear();

  for (auto& it : m_oldStoreMetadata) {
    IDBObjectStore* objectStore = it.key;
    objectStore->clearIndexCache();
  }
  m_oldStoreMetadata.clear();

  m_deletedIndexes.clear();
  m_deletedObjectStores.clear();
}

}  // namespace blink
