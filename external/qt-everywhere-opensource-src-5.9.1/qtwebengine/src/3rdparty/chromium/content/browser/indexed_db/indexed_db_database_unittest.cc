// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database.h"

#include <stdint.h>
#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_factory.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace {
const int kFakeChildProcessId = 0;
}

namespace content {

class IndexedDBDatabaseTest : public ::testing::Test {
 private:
  TestBrowserThreadBundle thread_bundle_;
};

TEST_F(IndexedDBDatabaseTest, BackingStoreRetention) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();
  EXPECT_TRUE(backing_store->HasOneRef());

  scoped_refptr<MockIndexedDBFactory> factory = new MockIndexedDBFactory();
  leveldb::Status s;
  scoped_refptr<IndexedDBDatabase> db =
      IndexedDBDatabase::Create(ASCIIToUTF16("db"),
                                backing_store.get(),
                                factory.get(),
                                IndexedDBDatabase::Identifier(),
                                &s);
  ASSERT_TRUE(s.ok());
  EXPECT_FALSE(backing_store->HasOneRef());  // local and db
  db = NULL;
  EXPECT_TRUE(backing_store->HasOneRef());  // local
}

TEST_F(IndexedDBDatabaseTest, ConnectionLifecycle) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();
  EXPECT_TRUE(backing_store->HasOneRef());  // local

  scoped_refptr<MockIndexedDBFactory> factory = new MockIndexedDBFactory();
  leveldb::Status s;
  scoped_refptr<IndexedDBDatabase> db =
      IndexedDBDatabase::Create(ASCIIToUTF16("db"),
                                backing_store.get(),
                                factory.get(),
                                IndexedDBDatabase::Identifier(),
                                &s);
  ASSERT_TRUE(s.ok());
  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  scoped_refptr<MockIndexedDBCallbacks> request1(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks1(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id1 = 1;
  std::unique_ptr<IndexedDBPendingConnection> connection1(
      base::MakeUnique<IndexedDBPendingConnection>(
          request1, callbacks1, kFakeChildProcessId, transaction_id1,
          IndexedDBDatabaseMetadata::DEFAULT_VERSION));
  db->OpenConnection(std::move(connection1));

  EXPECT_FALSE(backing_store->HasOneRef());  // db, connection count > 0

  scoped_refptr<MockIndexedDBCallbacks> request2(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks2(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id2 = 2;
  std::unique_ptr<IndexedDBPendingConnection> connection2(
      base::MakeUnique<IndexedDBPendingConnection>(
          request2, callbacks2, kFakeChildProcessId, transaction_id2,
          IndexedDBDatabaseMetadata::DEFAULT_VERSION));
  db->OpenConnection(std::move(connection2));

  EXPECT_FALSE(backing_store->HasOneRef());  // local and connection

  request1->connection()->ForceClose();
  EXPECT_FALSE(request1->connection()->IsConnected());

  EXPECT_FALSE(backing_store->HasOneRef());  // local and connection

  request2->connection()->ForceClose();
  EXPECT_FALSE(request2->connection()->IsConnected());

  EXPECT_TRUE(backing_store->HasOneRef());
  EXPECT_FALSE(db->backing_store());

  db = NULL;
}

TEST_F(IndexedDBDatabaseTest, ForcedClose) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();
  EXPECT_TRUE(backing_store->HasOneRef());

  scoped_refptr<MockIndexedDBFactory> factory = new MockIndexedDBFactory();
  leveldb::Status s;
  scoped_refptr<IndexedDBDatabase> database =
      IndexedDBDatabase::Create(ASCIIToUTF16("db"),
                                backing_store.get(),
                                factory.get(),
                                IndexedDBDatabase::Identifier(),
                                &s);
  ASSERT_TRUE(s.ok());
  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks(
      new MockIndexedDBDatabaseCallbacks());
  scoped_refptr<MockIndexedDBCallbacks> request(new MockIndexedDBCallbacks());
  const int64_t upgrade_transaction_id = 3;
  std::unique_ptr<IndexedDBPendingConnection> connection(
      base::MakeUnique<IndexedDBPendingConnection>(
          request, callbacks, kFakeChildProcessId, upgrade_transaction_id,
          IndexedDBDatabaseMetadata::DEFAULT_VERSION));
  database->OpenConnection(std::move(connection));
  EXPECT_EQ(database.get(), request->connection()->database());

  const int64_t transaction_id = 123;
  const std::vector<int64_t> scope;
  database->CreateTransaction(transaction_id,
                              request->connection(),
                              scope,
                              blink::WebIDBTransactionModeReadOnly);

  request->connection()->ForceClose();

  EXPECT_TRUE(backing_store->HasOneRef());  // local
  EXPECT_TRUE(callbacks->abort_called());
}

class MockCallbacks : public IndexedDBCallbacks {
 public:
  MockCallbacks() : IndexedDBCallbacks(NULL, 0, 0) {}

  void OnBlocked(int64_t existing_version) override { blocked_called_ = true; }
  void OnSuccess(int64_t result) override { success_called_ = true; }
  void OnError(const IndexedDBDatabaseError& error) override {
    error_called_ = true;
  }
  bool IsValid() const override { return valid_; }

  bool blocked_called() const { return blocked_called_; }
  bool success_called() const { return success_called_; }
  bool error_called() const { return error_called_; }
  void set_valid(bool valid) { valid_ = valid; }

 private:
  ~MockCallbacks() override {}

  bool blocked_called_ = false;
  bool success_called_ = false;
  bool error_called_ = false;
  bool valid_ = true;

  DISALLOW_COPY_AND_ASSIGN(MockCallbacks);
};

TEST_F(IndexedDBDatabaseTest, PendingDelete) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();
  EXPECT_TRUE(backing_store->HasOneRef());  // local

  scoped_refptr<MockIndexedDBFactory> factory = new MockIndexedDBFactory();
  leveldb::Status s;
  scoped_refptr<IndexedDBDatabase> db =
      IndexedDBDatabase::Create(ASCIIToUTF16("db"),
                                backing_store.get(),
                                factory.get(),
                                IndexedDBDatabase::Identifier(),
                                &s);
  ASSERT_TRUE(s.ok());
  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  scoped_refptr<MockIndexedDBCallbacks> request1(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks1(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id1 = 1;
  std::unique_ptr<IndexedDBPendingConnection> connection(
      base::MakeUnique<IndexedDBPendingConnection>(
          request1, callbacks1, kFakeChildProcessId, transaction_id1,
          IndexedDBDatabaseMetadata::DEFAULT_VERSION));
  db->OpenConnection(std::move(connection));

  EXPECT_EQ(db->ConnectionCount(), 1UL);
  EXPECT_EQ(db->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db->PendingOpenDeleteCount(), 0UL);
  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  scoped_refptr<MockCallbacks> request2(new MockCallbacks());
  db->DeleteDatabase(request2);
  EXPECT_EQ(db->ConnectionCount(), 1UL);
  EXPECT_EQ(db->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db->PendingOpenDeleteCount(), 0UL);

  EXPECT_FALSE(request2->blocked_called());
  db->VersionChangeIgnored();
  EXPECT_TRUE(request2->blocked_called());
  EXPECT_EQ(db->ConnectionCount(), 1UL);
  EXPECT_EQ(db->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db->PendingOpenDeleteCount(), 0UL);

  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  db->Close(request1->connection(), true /* forced */);
  EXPECT_EQ(db->ConnectionCount(), 0UL);
  EXPECT_EQ(db->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db->PendingOpenDeleteCount(), 0UL);

  EXPECT_FALSE(db->backing_store());
  EXPECT_TRUE(backing_store->HasOneRef());  // local
  EXPECT_TRUE(request2->success_called());
}

TEST_F(IndexedDBDatabaseTest, ConnectionRequestsNoLongerValid) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();

  const int64_t transaction_id1 = 1;
  scoped_refptr<MockIndexedDBFactory> factory = new MockIndexedDBFactory();
  leveldb::Status s;
  scoped_refptr<IndexedDBDatabase> db = IndexedDBDatabase::Create(
      ASCIIToUTF16("db"), backing_store.get(), factory.get(),
      IndexedDBDatabase::Identifier(), &s);

  // Make a connection request. This will be processed immediately.
  scoped_refptr<MockIndexedDBCallbacks> request1(new MockIndexedDBCallbacks());
  {
    std::unique_ptr<IndexedDBPendingConnection> connection(
        base::MakeUnique<IndexedDBPendingConnection>(
            request1, make_scoped_refptr(new MockIndexedDBDatabaseCallbacks()),
            kFakeChildProcessId, transaction_id1,
            IndexedDBDatabaseMetadata::DEFAULT_VERSION));
    db->OpenConnection(std::move(connection));
  }

  EXPECT_EQ(db->ConnectionCount(), 1UL);
  EXPECT_EQ(db->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db->PendingOpenDeleteCount(), 0UL);

  // Make a delete request. This will be blocked by the open.
  scoped_refptr<MockCallbacks> request2(new MockCallbacks());
  db->DeleteDatabase(request2);

  EXPECT_EQ(db->ConnectionCount(), 1UL);
  EXPECT_EQ(db->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db->PendingOpenDeleteCount(), 0UL);

  db->VersionChangeIgnored();

  EXPECT_TRUE(request2->blocked_called());

  // Make another delete request. This will be waiting in the queue.
  scoped_refptr<MockCallbacks> request3(new MockCallbacks());
  db->DeleteDatabase(request3);
  EXPECT_FALSE(request3->HasOneRef());  // local, db

  EXPECT_EQ(db->ConnectionCount(), 1UL);
  EXPECT_EQ(db->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db->PendingOpenDeleteCount(), 1UL);

  // Make another connection request. This will also be waiting in the queue.
  scoped_refptr<MockCallbacks> request4(new MockCallbacks());
  {
    std::unique_ptr<IndexedDBPendingConnection> connection(
        base::MakeUnique<IndexedDBPendingConnection>(
            request4, make_scoped_refptr(new MockIndexedDBDatabaseCallbacks()),
            kFakeChildProcessId, transaction_id1,
            IndexedDBDatabaseMetadata::DEFAULT_VERSION));
    db->OpenConnection(std::move(connection));
  }

  EXPECT_EQ(db->ConnectionCount(), 1UL);
  EXPECT_EQ(db->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db->PendingOpenDeleteCount(), 2UL);

  // Finally yet another delete request, also waiting in the queue.
  scoped_refptr<MockCallbacks> request5(new MockCallbacks());
  db->DeleteDatabase(request2);

  EXPECT_EQ(db->ConnectionCount(), 1UL);
  EXPECT_EQ(db->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db->PendingOpenDeleteCount(), 3UL);

  // Simulate renderer going away.
  request3->set_valid(false);
  request4->set_valid(false);

  // Close the blocking connection. First delete request should succeed.
  EXPECT_FALSE(request2->success_called());
  db->Close(request1->connection(), true /* forced */);
  EXPECT_TRUE(request2->success_called());

  // Delete requests that have lost dispatcher should still be processed so
  // that e.g. a delete followed by a window close is not ignored.
  EXPECT_FALSE(request3->blocked_called());
  EXPECT_TRUE(request3->success_called());
  EXPECT_FALSE(request3->error_called());
  EXPECT_TRUE(request3->HasOneRef());  // local

  // Open requests that have lost dispatcher should not be processed.
  EXPECT_FALSE(request4->blocked_called());
  EXPECT_FALSE(request4->success_called());
  EXPECT_FALSE(request4->error_called());
  EXPECT_TRUE(request4->HasOneRef());  // local

  // Final delete request should also run.
  EXPECT_TRUE(request2->success_called());

  // And everything else should be complete.
  EXPECT_EQ(db->ConnectionCount(), 0UL);
  EXPECT_EQ(db->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db->PendingOpenDeleteCount(), 0UL);
}

void DummyOperation(IndexedDBTransaction* transaction) {
}

class IndexedDBDatabaseOperationTest : public testing::Test {
 public:
  IndexedDBDatabaseOperationTest()
      : commit_success_(leveldb::Status::OK()),
        factory_(new MockIndexedDBFactory()) {}

  void SetUp() override {
    backing_store_ = new IndexedDBFakeBackingStore();
    leveldb::Status s;
    db_ = IndexedDBDatabase::Create(ASCIIToUTF16("db"),
                                    backing_store_.get(),
                                    factory_.get(),
                                    IndexedDBDatabase::Identifier(),
                                    &s);
    ASSERT_TRUE(s.ok());

    request_ = new MockIndexedDBCallbacks();
    callbacks_ = new MockIndexedDBDatabaseCallbacks();
    const int64_t transaction_id = 1;
    std::unique_ptr<IndexedDBPendingConnection> connection(
        base::MakeUnique<IndexedDBPendingConnection>(
            request_, callbacks_, kFakeChildProcessId, transaction_id,
            IndexedDBDatabaseMetadata::DEFAULT_VERSION));
    db_->OpenConnection(std::move(connection));
    EXPECT_EQ(IndexedDBDatabaseMetadata::NO_VERSION, db_->metadata().version);

    connection_ = base::MakeUnique<IndexedDBConnection>(db_, callbacks_);
    transaction_ = IndexedDBClassFactory::Get()->CreateIndexedDBTransaction(
        transaction_id, connection_->GetWeakPtr(),
        std::set<int64_t>() /*scope*/,
        blink::WebIDBTransactionModeVersionChange,
        new IndexedDBFakeBackingStore::FakeTransaction(commit_success_));
    db_->TransactionCreated(transaction_.get());

    // Add a dummy task which takes the place of the VersionChangeOperation
    // which kicks off the upgrade. This ensures that the transaction has
    // processed at least one task before the CreateObjectStore call.
    transaction_->ScheduleTask(base::Bind(&DummyOperation));
  }

  void RunPostedTasks() { base::RunLoop().RunUntilIdle(); }

 protected:
  scoped_refptr<IndexedDBFakeBackingStore> backing_store_;
  scoped_refptr<IndexedDBDatabase> db_;
  scoped_refptr<MockIndexedDBCallbacks> request_;
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks_;
  scoped_refptr<IndexedDBTransaction> transaction_;
  std::unique_ptr<IndexedDBConnection> connection_;

  leveldb::Status commit_success_;

 private:
  scoped_refptr<MockIndexedDBFactory> factory_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabaseOperationTest);
};

TEST_F(IndexedDBDatabaseOperationTest, CreateObjectStore) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  db_->CreateObjectStore(transaction_->id(),
                         store_id,
                         ASCIIToUTF16("store"),
                         IndexedDBKeyPath(),
                         false /*auto_increment*/);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  RunPostedTasks();
  transaction_->Commit();
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
}

TEST_F(IndexedDBDatabaseOperationTest, CreateIndex) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  db_->CreateObjectStore(transaction_->id(),
                         store_id,
                         ASCIIToUTF16("store"),
                         IndexedDBKeyPath(),
                         false /*auto_increment*/);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  const int64_t index_id = 2002;
  db_->CreateIndex(transaction_->id(),
                   store_id,
                   index_id,
                   ASCIIToUTF16("index"),
                   IndexedDBKeyPath(),
                   false /*unique*/,
                   false /*multi_entry*/);
  EXPECT_EQ(
      1ULL,
      db_->metadata().object_stores.find(store_id)->second.indexes.size());
  RunPostedTasks();
  transaction_->Commit();
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  EXPECT_EQ(
      1ULL,
      db_->metadata().object_stores.find(store_id)->second.indexes.size());
}

class IndexedDBDatabaseOperationAbortTest
    : public IndexedDBDatabaseOperationTest {
 public:
  IndexedDBDatabaseOperationAbortTest() {
    commit_success_ = leveldb::Status::NotFound("Bummer.");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabaseOperationAbortTest);
};

TEST_F(IndexedDBDatabaseOperationAbortTest, CreateObjectStore) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  db_->CreateObjectStore(transaction_->id(),
                         store_id,
                         ASCIIToUTF16("store"),
                         IndexedDBKeyPath(),
                         false /*auto_increment*/);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  RunPostedTasks();
  transaction_->Commit();
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
}

TEST_F(IndexedDBDatabaseOperationAbortTest, CreateIndex) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  db_->CreateObjectStore(transaction_->id(),
                         store_id,
                         ASCIIToUTF16("store"),
                         IndexedDBKeyPath(),
                         false /*auto_increment*/);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  const int64_t index_id = 2002;
  db_->CreateIndex(transaction_->id(),
                   store_id,
                   index_id,
                   ASCIIToUTF16("index"),
                   IndexedDBKeyPath(),
                   false /*unique*/,
                   false /*multi_entry*/);
  EXPECT_EQ(
      1ULL,
      db_->metadata().object_stores.find(store_id)->second.indexes.size());
  RunPostedTasks();
  transaction_->Commit();
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
}

TEST_F(IndexedDBDatabaseOperationTest, CreatePutDelete) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;

  // Creation is synchronous.
  db_->CreateObjectStore(transaction_->id(),
                         store_id,
                         ASCIIToUTF16("store"),
                         IndexedDBKeyPath(),
                         false /*auto_increment*/);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());


  // Put is asynchronous
  IndexedDBValue value("value1", std::vector<IndexedDBBlobInfo>());
  std::vector<std::unique_ptr<storage::BlobDataHandle>> handles;
  std::unique_ptr<IndexedDBKey> key(base::MakeUnique<IndexedDBKey>("key"));
  std::vector<IndexedDBIndexKeys> index_keys;
  scoped_refptr<MockIndexedDBCallbacks> request(
      new MockIndexedDBCallbacks(false));
  db_->Put(transaction_->id(), store_id, &value, &handles, std::move(key),
           blink::WebIDBPutModeAddOnly, request, index_keys);

  // Deletion is asynchronous.
  db_->DeleteObjectStore(transaction_->id(),
                         store_id);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());

  // This will execute the Put then Delete.
  RunPostedTasks();
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());

  transaction_->Commit();  // Cleans up the object hierarchy.
}

}  // namespace content
