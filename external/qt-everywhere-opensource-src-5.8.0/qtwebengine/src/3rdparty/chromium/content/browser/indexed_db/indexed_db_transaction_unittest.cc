// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class AbortObserver {
 public:
  AbortObserver() : abort_task_called_(false) {}

  void AbortTask(IndexedDBTransaction* transaction) {
    abort_task_called_ = true;
  }

  bool abort_task_called() const { return abort_task_called_; }

 private:
  bool abort_task_called_;
  DISALLOW_COPY_AND_ASSIGN(AbortObserver);
};

class IndexedDBTransactionTest : public testing::Test {
 public:
  IndexedDBTransactionTest() : factory_(new MockIndexedDBFactory()) {
    backing_store_ = new IndexedDBFakeBackingStore();
    CreateDB();
  }

  void CreateDB() {
    // DB is created here instead of the constructor to workaround a
    // "peculiarity of C++". More info at
    // https://github.com/google/googletest/blob/master/googletest/docs/FAQ.md#my-compiler-complains-that-a-constructor-or-destructor-cannot-return-a-value-whats-going-on
    leveldb::Status s;
    db_ = IndexedDBDatabase::Create(base::ASCIIToUTF16("db"),
                                    backing_store_.get(),
                                    factory_.get(),
                                    IndexedDBDatabase::Identifier(),
                                    &s);
    ASSERT_TRUE(s.ok());
  }

  void RunPostedTasks() { message_loop_.RunUntilIdle(); }
  void DummyOperation(IndexedDBTransaction* transaction) {}
  void AbortableOperation(AbortObserver* observer,
                          IndexedDBTransaction* transaction) {
    transaction->ScheduleAbortTask(
        base::Bind(&AbortObserver::AbortTask, base::Unretained(observer)));
  }

 protected:
  scoped_refptr<IndexedDBFakeBackingStore> backing_store_;
  scoped_refptr<IndexedDBDatabase> db_;

 private:
  base::MessageLoop message_loop_;
  scoped_refptr<MockIndexedDBFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBTransactionTest);
};

class IndexedDBTransactionTestMode
    : public IndexedDBTransactionTest,
      public testing::WithParamInterface<blink::WebIDBTransactionMode> {
 public:
  IndexedDBTransactionTestMode() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBTransactionTestMode);
};

TEST_F(IndexedDBTransactionTest, Timeout) {
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_success = leveldb::Status::OK();
  scoped_refptr<IndexedDBTransaction> transaction = new IndexedDBTransaction(
      id,
      new MockIndexedDBDatabaseCallbacks(),
      scope,
      blink::WebIDBTransactionModeReadWrite,
      db_.get(),
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));
  db_->TransactionCreated(transaction.get());

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  // Schedule a task - timer won't be started until it's processed.
  transaction->ScheduleTask(base::Bind(
      &IndexedDBTransactionTest::DummyOperation, base::Unretained(this)));
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  RunPostedTasks();
  EXPECT_TRUE(transaction->IsTimeoutTimerRunning());

  transaction->Timeout();
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);

  // This task will be ignored.
  transaction->ScheduleTask(base::Bind(
      &IndexedDBTransactionTest::DummyOperation, base::Unretained(this)));
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);
}

TEST_F(IndexedDBTransactionTest, NoTimeoutReadOnly) {
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_success = leveldb::Status::OK();
  scoped_refptr<IndexedDBTransaction> transaction = new IndexedDBTransaction(
      id,
      new MockIndexedDBDatabaseCallbacks(),
      scope,
      blink::WebIDBTransactionModeReadOnly,
      db_.get(),
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));
  db_->TransactionCreated(transaction.get());

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  // Schedule a task - timer won't be started until it's processed.
  transaction->ScheduleTask(base::Bind(
      &IndexedDBTransactionTest::DummyOperation, base::Unretained(this)));
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  // Transaction is read-only, so no need to time it out.
  RunPostedTasks();
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  // Clean up to avoid leaks.
  transaction->Abort();
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
}

TEST_P(IndexedDBTransactionTestMode, ScheduleNormalTask) {
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_success = leveldb::Status::OK();
  scoped_refptr<IndexedDBTransaction> transaction = new IndexedDBTransaction(
      id,
      new MockIndexedDBDatabaseCallbacks(),
      scope,
      GetParam(),
      db_.get(),
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  db_->TransactionCreated(transaction.get());

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());

  transaction->ScheduleTask(
      blink::WebIDBTaskTypeNormal,
      base::Bind(&IndexedDBTransactionTest::DummyOperation,
                 base::Unretained(this)));

  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  EXPECT_TRUE(transaction->HasPendingTasks());
  EXPECT_FALSE(transaction->IsTaskQueueEmpty());
  EXPECT_FALSE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());

  // Pump the message loop so that the transaction completes all pending tasks,
  // otherwise it will defer the commit.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);

  transaction->Commit();

  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);
}

TEST_F(IndexedDBTransactionTest, SchedulePreemptiveTask) {
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_failure = leveldb::Status::Corruption("Ouch.");
  scoped_refptr<IndexedDBTransaction> transaction = new IndexedDBTransaction(
      id,
      new MockIndexedDBDatabaseCallbacks(),
      scope,
      blink::WebIDBTransactionModeVersionChange,
      db_.get(),
      new IndexedDBFakeBackingStore::FakeTransaction(commit_failure));

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  db_->TransactionCreated(transaction.get());

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());

  transaction->ScheduleTask(
      blink::WebIDBTaskTypePreemptive,
      base::Bind(&IndexedDBTransactionTest::DummyOperation,
                 base::Unretained(this)));
  transaction->AddPreemptiveEvent();

  EXPECT_TRUE(transaction->HasPendingTasks());
  EXPECT_FALSE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_FALSE(transaction->preemptive_task_queue_.empty());

  // Pump the message loop so that the transaction completes all pending tasks,
  // otherwise it will defer the commit.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  transaction->DidCompletePreemptiveEvent();
  transaction->Commit();

  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);
}

TEST_P(IndexedDBTransactionTestMode, AbortTasks) {
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_failure = leveldb::Status::Corruption("Ouch.");
  scoped_refptr<IndexedDBTransaction> transaction = new IndexedDBTransaction(
      id,
      new MockIndexedDBDatabaseCallbacks(),
      scope,
      GetParam(),
      db_.get(),
      new IndexedDBFakeBackingStore::FakeTransaction(commit_failure));
  db_->TransactionCreated(transaction.get());

  AbortObserver observer;
  transaction->ScheduleTask(
      base::Bind(&IndexedDBTransactionTest::AbortableOperation,
                 base::Unretained(this),
                 base::Unretained(&observer)));

  // Pump the message loop so that the transaction completes all pending tasks,
  // otherwise it will defer the commit.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(observer.abort_task_called());
  transaction->Commit();
  EXPECT_TRUE(observer.abort_task_called());
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
}

TEST_P(IndexedDBTransactionTestMode, AbortPreemptive) {
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_success = leveldb::Status::OK();
  scoped_refptr<IndexedDBTransaction> transaction = new IndexedDBTransaction(
      id,
      new MockIndexedDBDatabaseCallbacks(),
      scope,
      GetParam(),
      db_.get(),
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));
  db_->TransactionCreated(transaction.get());

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  transaction->ScheduleTask(
      blink::WebIDBTaskTypePreemptive,
      base::Bind(&IndexedDBTransactionTest::DummyOperation,
                 base::Unretained(this)));
  EXPECT_EQ(0, transaction->pending_preemptive_events_);
  transaction->AddPreemptiveEvent();
  EXPECT_EQ(1, transaction->pending_preemptive_events_);

  RunPostedTasks();

  transaction->Abort();
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(0, transaction->pending_preemptive_events_);
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_EQ(transaction->diagnostics().tasks_completed,
            transaction->diagnostics().tasks_scheduled);
  EXPECT_FALSE(transaction->should_process_queue_);
  EXPECT_TRUE(transaction->backing_store_transaction_begun_);
  EXPECT_TRUE(transaction->used_);
  EXPECT_FALSE(transaction->commit_pending_);

  // This task will be ignored.
  transaction->ScheduleTask(base::Bind(
      &IndexedDBTransactionTest::DummyOperation, base::Unretained(this)));
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_EQ(transaction->diagnostics().tasks_completed,
            transaction->diagnostics().tasks_scheduled);
}

static const blink::WebIDBTransactionMode kTestModes[] = {
    blink::WebIDBTransactionModeReadOnly, blink::WebIDBTransactionModeReadWrite,
    blink::WebIDBTransactionModeVersionChange};

INSTANTIATE_TEST_CASE_P(IndexedDBTransactions,
                        IndexedDBTransactionTestMode,
                        ::testing::ValuesIn(kTestModes));

}  // namespace content
