// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_H_

#include <queue>
#include <set>
#include <stack>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"

namespace content {

class BlobWriteCallbackImpl;
class IndexedDBCursor;
class IndexedDBDatabaseCallbacks;

class CONTENT_EXPORT IndexedDBTransaction
    : public NON_EXPORTED_BASE(base::RefCounted<IndexedDBTransaction>) {
 public:
  typedef base::Callback<void(IndexedDBTransaction*)> Operation;

  IndexedDBTransaction(
      int64 id,
      scoped_refptr<IndexedDBDatabaseCallbacks> callbacks,
      const std::set<int64>& object_store_ids,
      indexed_db::TransactionMode,
      IndexedDBDatabase* db,
      IndexedDBBackingStore::Transaction* backing_store_transaction);

  virtual void Abort();
  void Commit();
  void Abort(const IndexedDBDatabaseError& error);

  // Called by the transaction coordinator when this transaction is unblocked.
  void Start();

  indexed_db::TransactionMode mode() const { return mode_; }
  const std::set<int64>& scope() const { return object_store_ids_; }

  void ScheduleTask(Operation task) {
    ScheduleTask(IndexedDBDatabase::NORMAL_TASK, task);
  }
  void ScheduleTask(IndexedDBDatabase::TaskType, Operation task);
  void ScheduleAbortTask(Operation abort_task);
  void RegisterOpenCursor(IndexedDBCursor* cursor);
  void UnregisterOpenCursor(IndexedDBCursor* cursor);
  void AddPreemptiveEvent() { pending_preemptive_events_++; }
  void DidCompletePreemptiveEvent() {
    pending_preemptive_events_--;
    DCHECK_GE(pending_preemptive_events_, 0);
  }
  IndexedDBBackingStore::Transaction* BackingStoreTransaction() {
    return transaction_.get();
  }
  int64 id() const { return id_; }

  IndexedDBDatabase* database() const { return database_; }
  IndexedDBDatabaseCallbacks* connection() const { return callbacks_; }

  enum State {
    CREATED,     // Created, but not yet started by coordinator.
    STARTED,     // Started by the coordinator.
    COMMITTING,  // In the process of committing, possibly waiting for blobs
                 // to be written.
    FINISHED,    // Either aborted or committed.
  };

  State state() const { return state_; }
  bool IsTimeoutTimerRunning() const { return timeout_timer_.IsRunning(); }

  struct Diagnostics {
    base::Time creation_time;
    base::Time start_time;
    int tasks_scheduled;
    int tasks_completed;
  };

  const Diagnostics& diagnostics() const { return diagnostics_; }

 private:
  friend class BlobWriteCallbackImpl;

  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTestMode, AbortPreemptive);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTest, Timeout);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTest,
                           SchedulePreemptiveTask);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTestMode,
                           ScheduleNormalTask);

  friend class base::RefCounted<IndexedDBTransaction>;
  virtual ~IndexedDBTransaction();

  void RunTasksIfStarted();

  bool IsTaskQueueEmpty() const;
  bool HasPendingTasks() const;

  void BlobWriteComplete(bool success);
  void ProcessTaskQueue();
  void CloseOpenCursors();
  void CommitPhaseTwo();
  void Timeout();

  const int64 id_;
  const std::set<int64> object_store_ids_;
  const indexed_db::TransactionMode mode_;

  bool used_;
  State state_;
  bool commit_pending_;
  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks_;
  scoped_refptr<IndexedDBDatabase> database_;

  class TaskQueue {
   public:
    TaskQueue();
    ~TaskQueue();
    bool empty() const { return queue_.empty(); }
    void push(Operation task) { queue_.push(task); }
    Operation pop();
    void clear();

   private:
    std::queue<Operation> queue_;

    DISALLOW_COPY_AND_ASSIGN(TaskQueue);
  };

  class TaskStack {
   public:
    TaskStack();
    ~TaskStack();
    bool empty() const { return stack_.empty(); }
    void push(Operation task) { stack_.push(task); }
    Operation pop();
    void clear();

   private:
    std::stack<Operation> stack_;

    DISALLOW_COPY_AND_ASSIGN(TaskStack);
  };

  TaskQueue task_queue_;
  TaskQueue preemptive_task_queue_;
  TaskStack abort_task_stack_;

  scoped_ptr<IndexedDBBackingStore::Transaction> transaction_;
  bool backing_store_transaction_begun_;

  bool should_process_queue_;
  int pending_preemptive_events_;

  std::set<IndexedDBCursor*> open_cursors_;

  // This timer is started after requests have been processed. If no subsequent
  // requests are processed before the timer fires, assume the script is
  // unresponsive and abort to unblock the transaction queue.
  base::OneShotTimer<IndexedDBTransaction> timeout_timer_;

  Diagnostics diagnostics_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_H_
