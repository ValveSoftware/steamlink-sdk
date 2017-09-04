// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBObserver.h"

#include <bitset>

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/modules/v8/IDBObserverCallback.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "core/dom/ExceptionCode.h"
#include "modules/IndexedDBNames.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBObserverChanges.h"
#include "modules/indexeddb/IDBObserverInit.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/WebIDBObserverImpl.h"

namespace blink {

IDBObserver* IDBObserver::create(IDBObserverCallback* callback) {
  return new IDBObserver(callback);
}

IDBObserver::IDBObserver(IDBObserverCallback* callback)
    : m_callback(callback) {}

void IDBObserver::observe(IDBDatabase* database,
                          IDBTransaction* transaction,
                          const IDBObserverInit& options,
                          ExceptionState& exceptionState) {
  if (transaction->isFinished() || transaction->isFinishing()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
    return;
  }
  if (!transaction->isActive()) {
    exceptionState.throwDOMException(
        TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
    return;
  }
  if (!database->backend()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return;
  }
  if (!options.hasOperationTypes()) {
    exceptionState.throwTypeError(
        "operationTypes not specified in observe options.");
    return;
  }
  if (options.operationTypes().isEmpty()) {
    exceptionState.throwTypeError("operationTypes must be populated.");
    return;
  }

  std::bitset<WebIDBOperationTypeCount> types;
  for (const auto& operationType : options.operationTypes()) {
    if (operationType == IndexedDBNames::add) {
      types[WebIDBAdd] = true;
    } else if (operationType == IndexedDBNames::put) {
      types[WebIDBPut] = true;
    } else if (operationType == IndexedDBNames::kDelete) {
      types[WebIDBDelete] = true;
    } else if (operationType == IndexedDBNames::clear) {
      types[WebIDBClear] = true;
    } else {
      exceptionState.throwTypeError(
          "Unknown operation type in observe options: " + operationType);
      return;
    }
  }

  std::unique_ptr<WebIDBObserverImpl> observer =
      WebIDBObserverImpl::create(this, options.transaction(), options.values(),
                                 options.noRecords(), types);
  WebIDBObserverImpl* observerPtr = observer.get();
  int32_t observerId =
      database->backend()->addObserver(std::move(observer), transaction->id());
  m_observerIds.add(observerId, database);
  observerPtr->setId(observerId);
}

void IDBObserver::unobserve(IDBDatabase* database,
                            ExceptionState& exceptionState) {
  if (!database->backend()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     IDBDatabase::databaseClosedErrorMessage);
    return;
  }

  Vector<int32_t> observerIdsToRemove;
  for (const auto& it : m_observerIds) {
    if (it.value == database)
      observerIdsToRemove.append(it.key);
  }
  m_observerIds.removeAll(observerIdsToRemove);

  if (!observerIdsToRemove.isEmpty())
    database->backend()->removeObservers(observerIdsToRemove);
}

void IDBObserver::removeObserver(int32_t id) {
  m_observerIds.remove(id);
}

void IDBObserver::onChange(int32_t id,
                           const WebVector<WebIDBObservation>& observations,
                           const WebVector<int32_t>& observationIndex) {
  auto it = m_observerIds.find(id);
  DCHECK(it != m_observerIds.end());
  m_callback->call(this, IDBObserverChanges::create(it->value, observations,
                                                    observationIndex));
}

DEFINE_TRACE(IDBObserver) {
  visitor->trace(m_callback);
  visitor->trace(m_observerIds);
}

}  // namespace blink
