// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseException.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace content {

namespace {

const int64_t kInactivityTimeoutPeriodSeconds = 60;

// Helper for posting a task to call IndexedDBTransaction::Commit when we know
// the transaction had no requests and therefore the commit must succeed.
void CommitUnused(scoped_refptr<IndexedDBTransaction> transaction) {
  leveldb::Status status = transaction->Commit();
  DCHECK(status.ok());
}

}  // namespace

IndexedDBTransaction::TaskQueue::TaskQueue() {}
IndexedDBTransaction::TaskQueue::~TaskQueue() { clear(); }

void IndexedDBTransaction::TaskQueue::clear() {
  while (!queue_.empty())
    queue_.pop();
}

IndexedDBTransaction::Operation IndexedDBTransaction::TaskQueue::pop() {
  DCHECK(!queue_.empty());
  Operation task(queue_.front());
  queue_.pop();
  return task;
}

IndexedDBTransaction::TaskStack::TaskStack() {}
IndexedDBTransaction::TaskStack::~TaskStack() { clear(); }

void IndexedDBTransaction::TaskStack::clear() {
  while (!stack_.empty())
    stack_.pop();
}

IndexedDBTransaction::Operation IndexedDBTransaction::TaskStack::pop() {
  DCHECK(!stack_.empty());
  Operation task(stack_.top());
  stack_.pop();
  return task;
}

IndexedDBTransaction::IndexedDBTransaction(
    int64_t id,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks,
    const std::set<int64_t>& object_store_ids,
    blink::WebIDBTransactionMode mode,
    IndexedDBDatabase* database,
    IndexedDBBackingStore::Transaction* backing_store_transaction)
    : id_(id),
      object_store_ids_(object_store_ids),
      mode_(mode),
      used_(false),
      state_(CREATED),
      commit_pending_(false),
      callbacks_(callbacks),
      database_(database),
      transaction_(backing_store_transaction),
      backing_store_transaction_begun_(false),
      should_process_queue_(false),
      pending_preemptive_events_(0) {
  database_->transaction_coordinator().DidCreateTransaction(this);

  diagnostics_.tasks_scheduled = 0;
  diagnostics_.tasks_completed = 0;
  diagnostics_.creation_time = base::Time::Now();
}

IndexedDBTransaction::~IndexedDBTransaction() {
  // It shouldn't be possible for this object to get deleted until it's either
  // complete or aborted.
  DCHECK_EQ(state_, FINISHED);
  DCHECK(preemptive_task_queue_.empty());
  DCHECK_EQ(pending_preemptive_events_, 0);
  DCHECK(task_queue_.empty());
  DCHECK(abort_task_stack_.empty());
}

void IndexedDBTransaction::ScheduleTask(blink::WebIDBTaskType type,
                                        Operation task) {
  DCHECK_NE(state_, COMMITTING);
  if (state_ == FINISHED)
    return;

  timeout_timer_.Stop();
  used_ = true;
  if (type == blink::WebIDBTaskTypeNormal) {
    task_queue_.push(task);
    ++diagnostics_.tasks_scheduled;
  } else {
    preemptive_task_queue_.push(task);
  }
  RunTasksIfStarted();
}

void IndexedDBTransaction::ScheduleAbortTask(Operation abort_task) {
  DCHECK_NE(FINISHED, state_);
  DCHECK(used_);
  abort_task_stack_.push(abort_task);
}

void IndexedDBTransaction::RunTasksIfStarted() {
  DCHECK(used_);

  // Not started by the coordinator yet.
  if (state_ != STARTED)
    return;

  // A task is already posted.
  if (should_process_queue_)
    return;

  should_process_queue_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&IndexedDBTransaction::ProcessTaskQueue, this));
}

void IndexedDBTransaction::Abort() {
  Abort(IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                               "Internal error (unknown cause)"));
}

void IndexedDBTransaction::Abort(const IndexedDBDatabaseError& error) {
  IDB_TRACE1("IndexedDBTransaction::Abort", "txn.id", id());
  if (state_ == FINISHED)
    return;

  // The last reference to this object may be released while performing the
  // abort steps below. We therefore take a self reference to keep ourselves
  // alive while executing this method.
  scoped_refptr<IndexedDBTransaction> protect(this);

  timeout_timer_.Stop();

  state_ = FINISHED;
  should_process_queue_ = false;

  if (backing_store_transaction_begun_)
    transaction_->Rollback();

  // Run the abort tasks, if any.
  while (!abort_task_stack_.empty())
    abort_task_stack_.pop().Run(NULL);

  preemptive_task_queue_.clear();
  pending_preemptive_events_ = 0;
  task_queue_.clear();

  // Backing store resources (held via cursors) must be released
  // before script callbacks are fired, as the script callbacks may
  // release references and allow the backing store itself to be
  // released, and order is critical.
  CloseOpenCursors();
  transaction_->Reset();

  // Transactions must also be marked as completed before the
  // front-end is notified, as the transaction completion unblocks
  // operations like closing connections.
  database_->transaction_coordinator().DidFinishTransaction(this);
#ifndef NDEBUG
  DCHECK(!database_->transaction_coordinator().IsActive(this));
#endif

  if (callbacks_.get())
    callbacks_->OnAbort(id_, error);

  database_->TransactionFinished(this, false);

  database_ = NULL;
}

bool IndexedDBTransaction::IsTaskQueueEmpty() const {
  return preemptive_task_queue_.empty() && task_queue_.empty();
}

bool IndexedDBTransaction::HasPendingTasks() const {
  return pending_preemptive_events_ || !IsTaskQueueEmpty();
}

void IndexedDBTransaction::RegisterOpenCursor(IndexedDBCursor* cursor) {
  open_cursors_.insert(cursor);
}

void IndexedDBTransaction::UnregisterOpenCursor(IndexedDBCursor* cursor) {
  open_cursors_.erase(cursor);
}

void IndexedDBTransaction::Start() {
  // TransactionCoordinator has started this transaction.
  DCHECK_EQ(CREATED, state_);
  state_ = STARTED;
  diagnostics_.start_time = base::Time::Now();

  if (!used_) {
    if (commit_pending_) {
      // The transaction has never had requests issued against it, but the
      // front-end previously requested a commit; do the commit now, but not
      // re-entrantly as that may renter the coordinator.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&CommitUnused, make_scoped_refptr(this)));
    }
    return;
  }

  RunTasksIfStarted();
}

class BlobWriteCallbackImpl : public IndexedDBBackingStore::BlobWriteCallback {
 public:
  explicit BlobWriteCallbackImpl(
      scoped_refptr<IndexedDBTransaction> transaction)
      : transaction_(transaction) {}
  void Run(bool succeeded) override {
    transaction_->BlobWriteComplete(succeeded);
  }

 protected:
  ~BlobWriteCallbackImpl() override {}

 private:
  scoped_refptr<IndexedDBTransaction> transaction_;
};

void IndexedDBTransaction::BlobWriteComplete(bool success) {
  IDB_TRACE("IndexedDBTransaction::BlobWriteComplete");
  if (state_ == FINISHED)  // aborted
    return;
  DCHECK_EQ(state_, COMMITTING);
  if (success)
    CommitPhaseTwo();
  else
    Abort(IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionDataError,
                                 "Failed to write blobs."));
}

leveldb::Status IndexedDBTransaction::Commit() {
  IDB_TRACE1("IndexedDBTransaction::Commit", "txn.id", id());

  timeout_timer_.Stop();

  // In multiprocess ports, front-end may have requested a commit but
  // an abort has already been initiated asynchronously by the
  // back-end.
  if (state_ == FINISHED)
    return leveldb::Status::OK();
  DCHECK_NE(state_, COMMITTING);

  DCHECK(!used_ || state_ == STARTED);
  commit_pending_ = true;

  // Front-end has requested a commit, but this transaction is blocked by
  // other transactions. The commit will be initiated when the transaction
  // coordinator unblocks this transaction.
  if (state_ != STARTED)
    return leveldb::Status::OK();

  // Front-end has requested a commit, but there may be tasks like
  // create_index which are considered synchronous by the front-end
  // but are processed asynchronously.
  if (HasPendingTasks())
    return leveldb::Status::OK();

  state_ = COMMITTING;

  leveldb::Status s;
  if (!used_) {
    s = CommitPhaseTwo();
  } else {
    scoped_refptr<IndexedDBBackingStore::BlobWriteCallback> callback(
        new BlobWriteCallbackImpl(this));
    // CommitPhaseOne will call the callback synchronously if there are no blobs
    // to write.
    s = transaction_->CommitPhaseOne(callback);
    if (!s.ok())
      Abort(IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionDataError,
                                   "Error processing blob journal."));
  }

  return s;
}

leveldb::Status IndexedDBTransaction::CommitPhaseTwo() {
  // Abort may have been called just as the blob write completed.
  if (state_ == FINISHED)
    return leveldb::Status::OK();

  DCHECK_EQ(state_, COMMITTING);

  // The last reference to this object may be released while performing the
  // commit steps below. We therefore take a self reference to keep ourselves
  // alive while executing this method.
  scoped_refptr<IndexedDBTransaction> protect(this);

  state_ = FINISHED;

  leveldb::Status s;
  bool committed;
  if (!used_) {
    committed = true;
  } else {
    s = transaction_->CommitPhaseTwo();
    committed = s.ok();
  }

  // Backing store resources (held via cursors) must be released
  // before script callbacks are fired, as the script callbacks may
  // release references and allow the backing store itself to be
  // released, and order is critical.
  CloseOpenCursors();
  transaction_->Reset();

  // Transactions must also be marked as completed before the
  // front-end is notified, as the transaction completion unblocks
  // operations like closing connections.
  database_->transaction_coordinator().DidFinishTransaction(this);

  if (committed) {
    abort_task_stack_.clear();
    {
      IDB_TRACE1(
          "IndexedDBTransaction::CommitPhaseTwo.TransactionCompleteCallbacks",
          "txn.id", id());
      callbacks_->OnComplete(id_);
    }
    database_->TransactionFinished(this, true);
  } else {
    while (!abort_task_stack_.empty())
      abort_task_stack_.pop().Run(NULL);

    IndexedDBDatabaseError error;
    if (leveldb_env::IndicatesDiskFull(s)) {
      error = IndexedDBDatabaseError(
          blink::WebIDBDatabaseExceptionQuotaError,
          "Encountered disk full while committing transaction.");
    } else {
      error = IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                                     "Internal error committing transaction.");
    }
    callbacks_->OnAbort(id_, error);

    database_->TransactionFinished(this, false);
    database_->TransactionCommitFailed(s);
  }

  database_ = NULL;
  return s;
}

void IndexedDBTransaction::ProcessTaskQueue() {
  IDB_TRACE1("IndexedDBTransaction::ProcessTaskQueue", "txn.id", id());

  // May have been aborted.
  if (!should_process_queue_)
    return;

  DCHECK(!IsTaskQueueEmpty());
  should_process_queue_ = false;

  if (!backing_store_transaction_begun_) {
    transaction_->Begin();
    backing_store_transaction_begun_ = true;
  }

  // The last reference to this object may be released while performing the
  // tasks. Take take a self reference to keep this object alive so that
  // the loop termination conditions can be checked.
  scoped_refptr<IndexedDBTransaction> protect(this);

  TaskQueue* task_queue =
      pending_preemptive_events_ ? &preemptive_task_queue_ : &task_queue_;
  while (!task_queue->empty() && state_ != FINISHED) {
    DCHECK_EQ(state_, STARTED);
    Operation task(task_queue->pop());
    task.Run(this);
    if (!pending_preemptive_events_) {
      DCHECK(diagnostics_.tasks_completed < diagnostics_.tasks_scheduled);
      ++diagnostics_.tasks_completed;
    }

    // Event itself may change which queue should be processed next.
    task_queue =
        pending_preemptive_events_ ? &preemptive_task_queue_ : &task_queue_;
  }

  // If there are no pending tasks, we haven't already committed/aborted,
  // and the front-end requested a commit, it is now safe to do so.
  if (!HasPendingTasks() && state_ != FINISHED && commit_pending_) {
    Commit();
    return;
  }

  // The transaction may have been aborted while processing tasks.
  if (state_ == FINISHED)
    return;

  DCHECK(state_ == STARTED);

  // Otherwise, start a timer in case the front-end gets wedged and
  // never requests further activity. Read-only transactions don't
  // block other transactions, so don't time those out.
  if (mode_ != blink::WebIDBTransactionModeReadOnly) {
    timeout_timer_.Start(FROM_HERE, GetInactivityTimeout(),
                         base::Bind(&IndexedDBTransaction::Timeout, this));
  }
}

base::TimeDelta IndexedDBTransaction::GetInactivityTimeout() const {
  return base::TimeDelta::FromSeconds(kInactivityTimeoutPeriodSeconds);
}

void IndexedDBTransaction::Timeout() {
  Abort(IndexedDBDatabaseError(
      blink::WebIDBDatabaseExceptionTimeoutError,
      base::ASCIIToUTF16("Transaction timed out due to inactivity.")));
}

void IndexedDBTransaction::CloseOpenCursors() {
  IDB_TRACE1("IndexedDBTransaction::CloseOpenCursors", "txn.id", id());
  for (auto* cursor : open_cursors_)
    cursor->Close();
  open_cursors_.clear();
}

}  // namespace content
