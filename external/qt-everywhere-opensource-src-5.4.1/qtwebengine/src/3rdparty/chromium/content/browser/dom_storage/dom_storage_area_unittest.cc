// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_database.h"
#include "content/browser/dom_storage/dom_storage_database_adapter.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/local_storage_database_adapter.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace content {

class DOMStorageAreaTest : public testing::Test {
 public:
  DOMStorageAreaTest()
    : kOrigin(GURL("http://dom_storage/")),
      kKey(ASCIIToUTF16("key")),
      kValue(ASCIIToUTF16("value")),
      kKey2(ASCIIToUTF16("key2")),
      kValue2(ASCIIToUTF16("value2")) {
  }

  const GURL kOrigin;
  const base::string16 kKey;
  const base::string16 kValue;
  const base::string16 kKey2;
  const base::string16 kValue2;

  // Method used in the CommitTasks test case.
  void InjectedCommitSequencingTask(DOMStorageArea* area) {
    // At this point the OnCommitTimer has run.
    // Verify that it put a commit in flight.
    EXPECT_EQ(1, area->commit_batches_in_flight_);
    EXPECT_FALSE(area->commit_batch_.get());
    EXPECT_TRUE(area->HasUncommittedChanges());
    // Make additional change and verify that a new commit batch
    // is created for that change.
    base::NullableString16 old_value;
    EXPECT_TRUE(area->SetItem(kKey2, kValue2, &old_value));
    EXPECT_TRUE(area->commit_batch_.get());
    EXPECT_EQ(1, area->commit_batches_in_flight_);
    EXPECT_TRUE(area->HasUncommittedChanges());
  }

  // Class used in the CommitChangesAtShutdown test case.
  class VerifyChangesCommittedDatabase : public DOMStorageDatabase {
   public:
    VerifyChangesCommittedDatabase() {}
    virtual ~VerifyChangesCommittedDatabase() {
      const base::string16 kKey(ASCIIToUTF16("key"));
      const base::string16 kValue(ASCIIToUTF16("value"));
      DOMStorageValuesMap values;
      ReadAllValues(&values);
      EXPECT_EQ(1u, values.size());
      EXPECT_EQ(kValue, values[kKey].string());
    }
  };

 private:
  base::MessageLoop message_loop_;
};

TEST_F(DOMStorageAreaTest, DOMStorageAreaBasics) {
  scoped_refptr<DOMStorageArea> area(
      new DOMStorageArea(1, std::string(), kOrigin, NULL, NULL));
  base::string16 old_value;
  base::NullableString16 old_nullable_value;
  scoped_refptr<DOMStorageArea> copy;

  // We don't focus on the underlying DOMStorageMap functionality
  // since that's covered by seperate unit tests.
  EXPECT_EQ(kOrigin, area->origin());
  EXPECT_EQ(1, area->namespace_id());
  EXPECT_EQ(0u, area->Length());
  EXPECT_TRUE(area->SetItem(kKey, kValue, &old_nullable_value));
  EXPECT_TRUE(area->SetItem(kKey2, kValue2, &old_nullable_value));
  EXPECT_FALSE(area->HasUncommittedChanges());

  // Verify that a copy shares the same map.
  copy = area->ShallowCopy(2, std::string());
  EXPECT_EQ(kOrigin, copy->origin());
  EXPECT_EQ(2, copy->namespace_id());
  EXPECT_EQ(area->Length(), copy->Length());
  EXPECT_EQ(area->GetItem(kKey).string(), copy->GetItem(kKey).string());
  EXPECT_EQ(area->Key(0).string(), copy->Key(0).string());
  EXPECT_EQ(copy->map_.get(), area->map_.get());

  // But will deep copy-on-write as needed.
  EXPECT_TRUE(area->RemoveItem(kKey, &old_value));
  EXPECT_NE(copy->map_.get(), area->map_.get());
  copy = area->ShallowCopy(2, std::string());
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_TRUE(area->SetItem(kKey, kValue, &old_nullable_value));
  EXPECT_NE(copy->map_.get(), area->map_.get());
  copy = area->ShallowCopy(2, std::string());
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_NE(0u, area->Length());
  EXPECT_TRUE(area->Clear());
  EXPECT_EQ(0u, area->Length());
  EXPECT_NE(copy->map_.get(), area->map_.get());

  // Verify that once Shutdown(), behaves that way.
  area->Shutdown();
  EXPECT_TRUE(area->is_shutdown_);
  EXPECT_FALSE(area->map_.get());
  EXPECT_EQ(0u, area->Length());
  EXPECT_TRUE(area->Key(0).is_null());
  EXPECT_TRUE(area->GetItem(kKey).is_null());
  EXPECT_FALSE(area->SetItem(kKey, kValue, &old_nullable_value));
  EXPECT_FALSE(area->RemoveItem(kKey, &old_value));
  EXPECT_FALSE(area->Clear());
}

TEST_F(DOMStorageAreaTest, BackingDatabaseOpened) {
  const int64 kSessionStorageNamespaceId = kLocalStorageNamespaceId + 1;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kExpectedOriginFilePath = temp_dir.path().Append(
      DOMStorageArea::DatabaseFileNameFromOrigin(kOrigin));

  // No directory, backing should be null.
  {
    scoped_refptr<DOMStorageArea> area(
        new DOMStorageArea(kOrigin, base::FilePath(), NULL));
    EXPECT_EQ(NULL, area->backing_.get());
    EXPECT_TRUE(area->is_initial_import_done_);
    EXPECT_FALSE(base::PathExists(kExpectedOriginFilePath));
  }

  // Valid directory and origin but no session storage backing. Backing should
  // be null.
  {
    scoped_refptr<DOMStorageArea> area(
        new DOMStorageArea(kSessionStorageNamespaceId, std::string(), kOrigin,
                           NULL, NULL));
    EXPECT_EQ(NULL, area->backing_.get());
    EXPECT_TRUE(area->is_initial_import_done_);

    base::NullableString16 old_value;
    EXPECT_TRUE(area->SetItem(kKey, kValue, &old_value));
    ASSERT_TRUE(old_value.is_null());

    // Check that saving a value has still left us without a backing database.
    EXPECT_EQ(NULL, area->backing_.get());
    EXPECT_FALSE(base::PathExists(kExpectedOriginFilePath));
  }

  // This should set up a DOMStorageArea that is correctly backed to disk.
  {
    scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
        kOrigin,
        temp_dir.path(),
        new MockDOMStorageTaskRunner(base::MessageLoopProxy::current().get())));

    EXPECT_TRUE(area->backing_.get());
    DOMStorageDatabase* database = static_cast<LocalStorageDatabaseAdapter*>(
        area->backing_.get())->db_.get();
    EXPECT_FALSE(database->IsOpen());
    EXPECT_FALSE(area->is_initial_import_done_);

    // Inject an in-memory db to speed up the test.
    // We will verify that something is written into the database but not
    // that a file is written to disk - DOMStorageDatabase unit tests cover
    // that.
    area->backing_.reset(new LocalStorageDatabaseAdapter());

    // Need to write something to ensure that the database is created.
    base::NullableString16 old_value;
    EXPECT_TRUE(area->SetItem(kKey, kValue, &old_value));
    ASSERT_TRUE(old_value.is_null());
    EXPECT_TRUE(area->is_initial_import_done_);
    EXPECT_TRUE(area->commit_batch_.get());
    EXPECT_EQ(0, area->commit_batches_in_flight_);

    base::MessageLoop::current()->RunUntilIdle();

    EXPECT_FALSE(area->commit_batch_.get());
    EXPECT_EQ(0, area->commit_batches_in_flight_);
    database = static_cast<LocalStorageDatabaseAdapter*>(
        area->backing_.get())->db_.get();
    EXPECT_TRUE(database->IsOpen());
    EXPECT_EQ(1u, area->Length());
    EXPECT_EQ(kValue, area->GetItem(kKey).string());

    // Verify the content made it to the in memory database.
    DOMStorageValuesMap values;
    area->backing_->ReadAllValues(&values);
    EXPECT_EQ(1u, values.size());
    EXPECT_EQ(kValue, values[kKey].string());
  }
}

TEST_F(DOMStorageAreaTest, CommitTasks) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kOrigin,
      temp_dir.path(),
      new MockDOMStorageTaskRunner(base::MessageLoopProxy::current().get())));
  // Inject an in-memory db to speed up the test.
  area->backing_.reset(new LocalStorageDatabaseAdapter());

  // Unrelated to commits, but while we're here, see that querying Length()
  // causes the backing database to be opened and presumably read from.
  EXPECT_FALSE(area->is_initial_import_done_);
  EXPECT_EQ(0u, area->Length());
  EXPECT_TRUE(area->is_initial_import_done_);

  DOMStorageValuesMap values;
  base::NullableString16 old_value;

  // See that changes are batched up.
  EXPECT_FALSE(area->commit_batch_.get());
  EXPECT_TRUE(area->SetItem(kKey, kValue, &old_value));
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_TRUE(area->commit_batch_.get());
  EXPECT_FALSE(area->commit_batch_->clear_all_first);
  EXPECT_EQ(1u, area->commit_batch_->changed_values.size());
  EXPECT_TRUE(area->SetItem(kKey2, kValue2, &old_value));
  EXPECT_TRUE(area->commit_batch_.get());
  EXPECT_FALSE(area->commit_batch_->clear_all_first);
  EXPECT_EQ(2u, area->commit_batch_->changed_values.size());
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(area->HasUncommittedChanges());
  EXPECT_FALSE(area->commit_batch_.get());
  EXPECT_EQ(0, area->commit_batches_in_flight_);
  // Verify the changes made it to the database.
  values.clear();
  area->backing_->ReadAllValues(&values);
  EXPECT_EQ(2u, values.size());
  EXPECT_EQ(kValue, values[kKey].string());
  EXPECT_EQ(kValue2, values[kKey2].string());

  // See that clear is handled properly.
  EXPECT_TRUE(area->Clear());
  EXPECT_TRUE(area->commit_batch_.get());
  EXPECT_TRUE(area->commit_batch_->clear_all_first);
  EXPECT_TRUE(area->commit_batch_->changed_values.empty());
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(area->commit_batch_.get());
  EXPECT_EQ(0, area->commit_batches_in_flight_);
  // Verify the changes made it to the database.
  values.clear();
  area->backing_->ReadAllValues(&values);
  EXPECT_TRUE(values.empty());

  // See that if changes accrue while a commit is "in flight"
  // those will also get committed.
  EXPECT_TRUE(area->SetItem(kKey, kValue, &old_value));
  EXPECT_TRUE(area->HasUncommittedChanges());
  // At this point the OnCommitTimer task has been posted. We inject
  // another task in the queue that will execute after the timer task,
  // but before the CommitChanges task. From within our injected task,
  // we'll make an additional SetItem() call.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DOMStorageAreaTest::InjectedCommitSequencingTask,
                 base::Unretained(this),
                 area));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(area->HasOneRef());
  EXPECT_FALSE(area->HasUncommittedChanges());
  // Verify the changes made it to the database.
  values.clear();
  area->backing_->ReadAllValues(&values);
  EXPECT_EQ(2u, values.size());
  EXPECT_EQ(kValue, values[kKey].string());
  EXPECT_EQ(kValue2, values[kKey2].string());
}

TEST_F(DOMStorageAreaTest, CommitChangesAtShutdown) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kOrigin,
      temp_dir.path(),
      new MockDOMStorageTaskRunner(base::MessageLoopProxy::current().get())));

  // Inject an in-memory db to speed up the test and also to verify
  // the final changes are commited in it's dtor.
  static_cast<LocalStorageDatabaseAdapter*>(area->backing_.get())->db_.reset(
      new VerifyChangesCommittedDatabase());

  DOMStorageValuesMap values;
  base::NullableString16 old_value;
  EXPECT_TRUE(area->SetItem(kKey, kValue, &old_value));
  EXPECT_TRUE(area->HasUncommittedChanges());
  area->backing_->ReadAllValues(&values);
  EXPECT_TRUE(values.empty());  // not committed yet
  area->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(area->HasOneRef());
  EXPECT_FALSE(area->backing_.get());
  // The VerifyChangesCommittedDatabase destructor verifies values
  // were committed.
}

TEST_F(DOMStorageAreaTest, DeleteOrigin) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kOrigin,
      temp_dir.path(),
      new MockDOMStorageTaskRunner(base::MessageLoopProxy::current().get())));

  // This test puts files on disk.
  base::FilePath db_file_path = static_cast<LocalStorageDatabaseAdapter*>(
      area->backing_.get())->db_->file_path();
  base::FilePath db_journal_file_path =
      DOMStorageDatabase::GetJournalFilePath(db_file_path);

  // Nothing bad should happen when invoked w/o any files on disk.
  area->DeleteOrigin();
  EXPECT_FALSE(base::PathExists(db_file_path));

  // Commit something in the database and then delete.
  base::NullableString16 old_value;
  area->SetItem(kKey, kValue, &old_value);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(base::PathExists(db_file_path));
  area->DeleteOrigin();
  EXPECT_EQ(0u, area->Length());
  EXPECT_FALSE(base::PathExists(db_file_path));
  EXPECT_FALSE(base::PathExists(db_journal_file_path));

  // Put some uncommitted changes to a non-existing database in
  // and then delete. No file ever gets created in this case.
  area->SetItem(kKey, kValue, &old_value);
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_EQ(1u, area->Length());
  area->DeleteOrigin();
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_EQ(0u, area->Length());
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(area->HasUncommittedChanges());
  EXPECT_FALSE(base::PathExists(db_file_path));

  // Put some uncommitted changes to a an existing database in
  // and then delete.
  area->SetItem(kKey, kValue, &old_value);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(base::PathExists(db_file_path));
  area->SetItem(kKey2, kValue2, &old_value);
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_EQ(2u, area->Length());
  area->DeleteOrigin();
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_EQ(0u, area->Length());
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(area->HasUncommittedChanges());
  // Since the area had uncommitted changes at the time delete
  // was called, the file will linger until the shutdown time.
  EXPECT_TRUE(base::PathExists(db_file_path));
  area->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(db_file_path));
}

TEST_F(DOMStorageAreaTest, PurgeMemory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kOrigin,
      temp_dir.path(),
      new MockDOMStorageTaskRunner(base::MessageLoopProxy::current().get())));

  // Inject an in-memory db to speed up the test.
  area->backing_.reset(new LocalStorageDatabaseAdapter());

  // Unowned ptrs we use to verify that 'purge' has happened.
  DOMStorageDatabase* original_backing =
      static_cast<LocalStorageDatabaseAdapter*>(
          area->backing_.get())->db_.get();
  DOMStorageMap* original_map = area->map_.get();

  // Should do no harm when called on a newly constructed object.
  EXPECT_FALSE(area->is_initial_import_done_);
  area->PurgeMemory();
  EXPECT_FALSE(area->is_initial_import_done_);
  DOMStorageDatabase* new_backing = static_cast<LocalStorageDatabaseAdapter*>(
      area->backing_.get())->db_.get();
  EXPECT_EQ(original_backing, new_backing);
  EXPECT_EQ(original_map, area->map_.get());

  // Should not do anything when commits are pending.
  base::NullableString16 old_value;
  area->SetItem(kKey, kValue, &old_value);
  EXPECT_TRUE(area->is_initial_import_done_);
  EXPECT_TRUE(area->HasUncommittedChanges());
  area->PurgeMemory();
  EXPECT_TRUE(area->is_initial_import_done_);
  EXPECT_TRUE(area->HasUncommittedChanges());
  new_backing = static_cast<LocalStorageDatabaseAdapter*>(
      area->backing_.get())->db_.get();
  EXPECT_EQ(original_backing, new_backing);
  EXPECT_EQ(original_map, area->map_.get());

  // Commit the changes from above,
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(area->HasUncommittedChanges());
  new_backing = static_cast<LocalStorageDatabaseAdapter*>(
      area->backing_.get())->db_.get();
  EXPECT_EQ(original_backing, new_backing);
  EXPECT_EQ(original_map, area->map_.get());

  // Should drop caches and reset database connections
  // when invoked on an area that's loaded up primed.
  area->PurgeMemory();
  EXPECT_FALSE(area->is_initial_import_done_);
  new_backing = static_cast<LocalStorageDatabaseAdapter*>(
      area->backing_.get())->db_.get();
  EXPECT_NE(original_backing, new_backing);
  EXPECT_NE(original_map, area->map_.get());
}

TEST_F(DOMStorageAreaTest, DatabaseFileNames) {
  struct {
    const char* origin;
    const char* file_name;
    const char* journal_file_name;
  } kCases[] = {
    { "https://www.google.com/",
      "https_www.google.com_0.localstorage",
      "https_www.google.com_0.localstorage-journal" },
    { "http://www.google.com:8080/",
      "http_www.google.com_8080.localstorage",
      "http_www.google.com_8080.localstorage-journal" },
    { "file:///",
      "file__0.localstorage",
      "file__0.localstorage-journal" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kCases); ++i) {
    GURL origin = GURL(kCases[i].origin).GetOrigin();
    base::FilePath file_name =
        base::FilePath().AppendASCII(kCases[i].file_name);
    base::FilePath journal_file_name =
        base::FilePath().AppendASCII(kCases[i].journal_file_name);

    EXPECT_EQ(file_name,
              DOMStorageArea::DatabaseFileNameFromOrigin(origin));
    EXPECT_EQ(origin,
              DOMStorageArea::OriginFromDatabaseFileName(file_name));
    EXPECT_EQ(journal_file_name,
              DOMStorageDatabase::GetJournalFilePath(file_name));
  }

  // Also test some DOMStorageDatabase::GetJournalFilePath cases here.
  base::FilePath parent = base::FilePath().AppendASCII("a").AppendASCII("b");
  EXPECT_EQ(
      parent.AppendASCII("file-journal"),
      DOMStorageDatabase::GetJournalFilePath(parent.AppendASCII("file")));
  EXPECT_EQ(
      base::FilePath().AppendASCII("-journal"),
      DOMStorageDatabase::GetJournalFilePath(base::FilePath()));
  EXPECT_EQ(
      base::FilePath().AppendASCII(".extensiononly-journal"),
      DOMStorageDatabase::GetJournalFilePath(
          base::FilePath().AppendASCII(".extensiononly")));
}

}  // namespace content
