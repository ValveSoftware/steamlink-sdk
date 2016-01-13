// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/browser/quota/special_storage_policy.h"
#include "webkit/common/database/database_identifier.h"

namespace content {

class IndexedDBTest : public testing::Test {
 public:
  const GURL kNormalOrigin;
  const GURL kSessionOnlyOrigin;

  IndexedDBTest()
      : kNormalOrigin("http://normal/"),
        kSessionOnlyOrigin("http://session-only/"),
        task_runner_(new base::TestSimpleTaskRunner),
        special_storage_policy_(new MockSpecialStoragePolicy),
        file_thread_(BrowserThread::FILE_USER_BLOCKING, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
    special_storage_policy_->AddSessionOnly(kSessionOnlyOrigin);
  }

 protected:
  void FlushIndexedDBTaskRunner() { task_runner_->RunUntilIdle(); }

  base::MessageLoopForIO message_loop_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;

 private:
  BrowserThreadImpl file_thread_;
  BrowserThreadImpl io_thread_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBTest);
};

TEST_F(IndexedDBTest, ClearSessionOnlyDatabases) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Create the scope which will ensure we run the destructor of the context
  // which should trigger the clean up.
  {
    scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
        temp_dir.path(), special_storage_policy_, NULL, task_runner_);

    normal_path = idb_context->GetFilePathForTesting(
        webkit_database::GetIdentifierFromOrigin(kNormalOrigin));
    session_only_path = idb_context->GetFilePathForTesting(
        webkit_database::GetIdentifierFromOrigin(kSessionOnlyOrigin));
    ASSERT_TRUE(base::CreateDirectory(normal_path));
    ASSERT_TRUE(base::CreateDirectory(session_only_path));
    FlushIndexedDBTaskRunner();
    message_loop_.RunUntilIdle();
  }

  FlushIndexedDBTaskRunner();
  message_loop_.RunUntilIdle();

  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_FALSE(base::DirectoryExists(session_only_path));
}

TEST_F(IndexedDBTest, SetForceKeepSessionState) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Create the scope which will ensure we run the destructor of the context.
  {
    // Create some indexedDB paths.
    // With the levelDB backend, these are directories.
    scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
        temp_dir.path(), special_storage_policy_, NULL, task_runner_);

    // Save session state. This should bypass the destruction-time deletion.
    idb_context->SetForceKeepSessionState();

    normal_path = idb_context->GetFilePathForTesting(
        webkit_database::GetIdentifierFromOrigin(kNormalOrigin));
    session_only_path = idb_context->GetFilePathForTesting(
        webkit_database::GetIdentifierFromOrigin(kSessionOnlyOrigin));
    ASSERT_TRUE(base::CreateDirectory(normal_path));
    ASSERT_TRUE(base::CreateDirectory(session_only_path));
    message_loop_.RunUntilIdle();
  }

  // Make sure we wait until the destructor has run.
  message_loop_.RunUntilIdle();

  // No data was cleared because of SetForceKeepSessionState.
  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_TRUE(base::DirectoryExists(session_only_path));
}

class ForceCloseDBCallbacks : public IndexedDBCallbacks {
 public:
  ForceCloseDBCallbacks(scoped_refptr<IndexedDBContextImpl> idb_context,
                        const GURL& origin_url)
      : IndexedDBCallbacks(NULL, 0, 0),
        idb_context_(idb_context),
        origin_url_(origin_url) {}

  virtual void OnSuccess() OVERRIDE {}
  virtual void OnSuccess(const std::vector<base::string16>&) OVERRIDE {}
  virtual void OnSuccess(scoped_ptr<IndexedDBConnection> connection,
                         const IndexedDBDatabaseMetadata& metadata) OVERRIDE {
    connection_ = connection.Pass();
    idb_context_->ConnectionOpened(origin_url_, connection_.get());
  }

  IndexedDBConnection* connection() { return connection_.get(); }

 protected:
  virtual ~ForceCloseDBCallbacks() {}

 private:
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  GURL origin_url_;
  scoped_ptr<IndexedDBConnection> connection_;
  DISALLOW_COPY_AND_ASSIGN(ForceCloseDBCallbacks);
};

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnDelete) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<MockIndexedDBDatabaseCallbacks> open_db_callbacks(
      new MockIndexedDBDatabaseCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> closed_db_callbacks(
      new MockIndexedDBDatabaseCallbacks());

  base::FilePath test_path;

  // Create the scope which will ensure we run the destructor of the context.
  {
    TestBrowserContext browser_context;

    const GURL kTestOrigin("http://test/");

    scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
        temp_dir.path(), special_storage_policy_, NULL, task_runner_);

    scoped_refptr<ForceCloseDBCallbacks> open_callbacks =
        new ForceCloseDBCallbacks(idb_context, kTestOrigin);

    scoped_refptr<ForceCloseDBCallbacks> closed_callbacks =
        new ForceCloseDBCallbacks(idb_context, kTestOrigin);

    IndexedDBFactory* factory = idb_context->GetIDBFactory();

    test_path = idb_context->GetFilePathForTesting(
        webkit_database::GetIdentifierFromOrigin(kTestOrigin));

    IndexedDBPendingConnection open_connection(open_callbacks,
                                               open_db_callbacks,
                                               0 /* child_process_id */,
                                               0 /* host_transaction_id */,
                                               0 /* version */);
    factory->Open(base::ASCIIToUTF16("opendb"),
                  open_connection,
                  NULL /* request_context */,
                  kTestOrigin,
                  idb_context->data_path());
    IndexedDBPendingConnection closed_connection(closed_callbacks,
                                                 closed_db_callbacks,
                                                 0 /* child_process_id */,
                                                 0 /* host_transaction_id */,
                                                 0 /* version */);
    factory->Open(base::ASCIIToUTF16("closeddb"),
                  closed_connection,
                  NULL /* request_context */,
                  kTestOrigin,
                  idb_context->data_path());

    closed_callbacks->connection()->Close();

    idb_context->TaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(
            &IndexedDBContextImpl::DeleteForOrigin, idb_context, kTestOrigin));
    FlushIndexedDBTaskRunner();
    message_loop_.RunUntilIdle();
  }

  // Make sure we wait until the destructor has run.
  message_loop_.RunUntilIdle();

  EXPECT_TRUE(open_db_callbacks->forced_close_called());
  EXPECT_FALSE(closed_db_callbacks->forced_close_called());
  EXPECT_FALSE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, DeleteFailsIfDirectoryLocked) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const GURL kTestOrigin("http://test/");

  scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
      temp_dir.path(), special_storage_policy_, NULL, task_runner_);

  base::FilePath test_path = idb_context->GetFilePathForTesting(
      webkit_database::GetIdentifierFromOrigin(kTestOrigin));
  ASSERT_TRUE(base::CreateDirectory(test_path));

  scoped_ptr<LevelDBLock> lock =
      LevelDBDatabase::LockForTesting(test_path);
  ASSERT_TRUE(lock);

  idb_context->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &IndexedDBContextImpl::DeleteForOrigin, idb_context, kTestOrigin));
  FlushIndexedDBTaskRunner();

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnCommitFailure) {
  const GURL kTestOrigin("http://test/");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<IndexedDBContextImpl> context = new IndexedDBContextImpl(
      temp_dir.path(), special_storage_policy_, NULL, task_runner_);

  scoped_refptr<IndexedDBFactory> factory = context->GetIDBFactory();

  scoped_refptr<MockIndexedDBCallbacks> callbacks(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks(
      new MockIndexedDBDatabaseCallbacks());
  const int64 transaction_id = 1;
  IndexedDBPendingConnection connection(
      callbacks,
      db_callbacks,
      0 /* child_process_id */,
      transaction_id,
      IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);
  factory->Open(base::ASCIIToUTF16("db"),
                connection,
                NULL /* request_context */,
                kTestOrigin,
                temp_dir.path());

  EXPECT_TRUE(callbacks->connection());

  // ConnectionOpened() is usually called by the dispatcher.
  context->ConnectionOpened(kTestOrigin, callbacks->connection());

  EXPECT_TRUE(factory->IsBackingStoreOpen(kTestOrigin));

  // Simulate the write failure.
  callbacks->connection()->database()->TransactionCommitFailed();

  EXPECT_TRUE(db_callbacks->forced_close_called());
  EXPECT_FALSE(factory->IsBackingStoreOpen(kTestOrigin));
}

}  // namespace content
