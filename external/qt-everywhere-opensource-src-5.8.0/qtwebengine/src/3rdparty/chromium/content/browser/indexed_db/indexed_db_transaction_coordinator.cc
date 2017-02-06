// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"

#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"

namespace content {

IndexedDBTransactionCoordinator::IndexedDBTransactionCoordinator() {}

IndexedDBTransactionCoordinator::~IndexedDBTransactionCoordinator() {
  DCHECK(!queued_transactions_.size());
  DCHECK(!started_transactions_.size());
}

void IndexedDBTransactionCoordinator::DidCreateTransaction(
    scoped_refptr<IndexedDBTransaction> transaction) {
  DCHECK(!queued_transactions_.count(transaction));
  DCHECK(!started_transactions_.count(transaction));
  DCHECK_EQ(IndexedDBTransaction::CREATED, transaction->state());

  queued_transactions_.insert(transaction);
  ProcessQueuedTransactions();
}

void IndexedDBTransactionCoordinator::DidFinishTransaction(
    IndexedDBTransaction* transaction) {
  if (queued_transactions_.count(transaction)) {
    DCHECK(!started_transactions_.count(transaction));
    queued_transactions_.erase(transaction);
  } else {
    DCHECK(started_transactions_.count(transaction));
    started_transactions_.erase(transaction);
  }

  ProcessQueuedTransactions();
}

bool IndexedDBTransactionCoordinator::IsRunningVersionChangeTransaction()
    const {
  return !started_transactions_.empty() &&
         (*started_transactions_.begin())->mode() ==
             blink::WebIDBTransactionModeVersionChange;
}

#ifndef NDEBUG
// Verifies internal consistency while returning whether anything is found.
bool IndexedDBTransactionCoordinator::IsActive(
    IndexedDBTransaction* transaction) {
  bool found = false;
  if (queued_transactions_.count(transaction))
    found = true;
  if (started_transactions_.count(transaction)) {
    DCHECK(!found);
    found = true;
  }
  return found;
}
#endif

std::vector<const IndexedDBTransaction*>
IndexedDBTransactionCoordinator::GetTransactions() const {
  std::vector<const IndexedDBTransaction*> result;

  for (const auto& transaction : started_transactions_)
    result.push_back(transaction.get());
  for (const auto& transaction : queued_transactions_)
    result.push_back(transaction.get());

  return result;
}

void IndexedDBTransactionCoordinator::ProcessQueuedTransactions() {
  if (queued_transactions_.empty())
    return;

  DCHECK(!IsRunningVersionChangeTransaction());

  // The locked_scope set accumulates the ids of object stores in the scope of
  // running read/write transactions. Other read-write transactions with
  // stores in this set may not be started. Read-only transactions may start,
  // taking a snapshot of the database, which does not include uncommitted
  // data. ("Version change" transactions are exclusive, but handled by the
  // connection sequencing in IndexedDBDatabase.)
  std::set<int64_t> locked_scope;
  for (const auto& transaction : started_transactions_) {
    if (transaction->mode() == blink::WebIDBTransactionModeReadWrite) {
      // Started read/write transactions have exclusive access to the object
      // stores within their scopes.
      locked_scope.insert(transaction->scope().begin(),
                          transaction->scope().end());
    }
  }

  TransactionSet::const_iterator it = queued_transactions_.begin();
  while (it != queued_transactions_.end()) {
    scoped_refptr<IndexedDBTransaction> transaction = *it;
    ++it;
    if (CanStartTransaction(transaction.get(), locked_scope)) {
      DCHECK_EQ(IndexedDBTransaction::CREATED, transaction->state());
      queued_transactions_.erase(transaction);
      started_transactions_.insert(transaction);
      transaction->Start();
      DCHECK_EQ(IndexedDBTransaction::STARTED, transaction->state());
    }
    if (transaction->mode() == blink::WebIDBTransactionModeReadWrite) {
      // Either the transaction started, so it has exclusive access to the
      // stores in its scope, or per the spec the transaction which was
      // created first must get access first, so the stores are also locked.
      locked_scope.insert(transaction->scope().begin(),
                          transaction->scope().end());
    }
  }
}

template<typename T>
static bool DoSetsIntersect(const std::set<T>& set1,
                            const std::set<T>& set2) {
  typename std::set<T>::const_iterator it1 = set1.begin();
  typename std::set<T>::const_iterator it2 = set2.begin();
  while (it1 != set1.end() && it2 != set2.end()) {
    if (*it1 < *it2)
      ++it1;
    else if (*it2 < *it1)
      ++it2;
    else
      return true;
  }
  return false;
}

bool IndexedDBTransactionCoordinator::CanStartTransaction(
    IndexedDBTransaction* const transaction,
    const std::set<int64_t>& locked_scope) const {
  DCHECK(queued_transactions_.count(transaction));
  switch (transaction->mode()) {
    case blink::WebIDBTransactionModeVersionChange:
      DCHECK_EQ(1u, queued_transactions_.size());
      DCHECK(started_transactions_.empty());
      DCHECK(locked_scope.empty());
      return true;

    case blink::WebIDBTransactionModeReadOnly:
    case blink::WebIDBTransactionModeReadWrite:
      return !DoSetsIntersect(transaction->scope(), locked_scope);
  }
  NOTREACHED();
  return false;
}

}  // namespace content
