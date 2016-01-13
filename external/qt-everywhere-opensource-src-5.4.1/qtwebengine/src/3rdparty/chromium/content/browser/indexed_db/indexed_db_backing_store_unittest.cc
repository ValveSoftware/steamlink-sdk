// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_backing_store.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebIDBTypes.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/quota/special_storage_policy.h"

using base::ASCIIToUTF16;

namespace content {

namespace {

class Comparator : public LevelDBComparator {
 public:
  virtual int Compare(const base::StringPiece& a,
                      const base::StringPiece& b) const OVERRIDE {
    return content::Compare(a, b, false /*index_keys*/);
  }
  virtual const char* Name() const OVERRIDE { return "idb_cmp1"; }
};

class DefaultLevelDBFactory : public LevelDBFactory {
 public:
  DefaultLevelDBFactory() {}
  virtual leveldb::Status OpenLevelDB(const base::FilePath& file_name,
                                      const LevelDBComparator* comparator,
                                      scoped_ptr<LevelDBDatabase>* db,
                                      bool* is_disk_full) OVERRIDE {
    return LevelDBDatabase::Open(file_name, comparator, db, is_disk_full);
  }
  virtual leveldb::Status DestroyLevelDB(
      const base::FilePath& file_name) OVERRIDE {
    return LevelDBDatabase::Destroy(file_name);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultLevelDBFactory);
};

class TestableIndexedDBBackingStore : public IndexedDBBackingStore {
 public:
  static scoped_refptr<TestableIndexedDBBackingStore> Open(
      IndexedDBFactory* indexed_db_factory,
      const GURL& origin_url,
      const base::FilePath& path_base,
      net::URLRequestContext* request_context,
      LevelDBFactory* leveldb_factory,
      base::TaskRunner* task_runner) {
    DCHECK(!path_base.empty());

    scoped_ptr<LevelDBComparator> comparator(new Comparator());

    if (!base::CreateDirectory(path_base))
      return scoped_refptr<TestableIndexedDBBackingStore>();

    const base::FilePath file_path = path_base.AppendASCII("test_db_path");
    const base::FilePath blob_path = path_base.AppendASCII("test_blob_path");

    scoped_ptr<LevelDBDatabase> db;
    bool is_disk_full = false;
    leveldb::Status status = leveldb_factory->OpenLevelDB(
        file_path, comparator.get(), &db, &is_disk_full);

    if (!db || !status.ok())
      return scoped_refptr<TestableIndexedDBBackingStore>();

    scoped_refptr<TestableIndexedDBBackingStore> backing_store(
        new TestableIndexedDBBackingStore(indexed_db_factory,
                                          origin_url,
                                          blob_path,
                                          request_context,
                                          db.Pass(),
                                          comparator.Pass(),
                                          task_runner));

    if (!backing_store->SetUpMetadata())
      return scoped_refptr<TestableIndexedDBBackingStore>();

    return backing_store;
  }

  const std::vector<IndexedDBBackingStore::Transaction::WriteDescriptor>&
  writes() const {
    return writes_;
  }
  void ClearWrites() { writes_.clear(); }
  const std::vector<int64>& removals() const { return removals_; }
  void ClearRemovals() { removals_.clear(); }

 protected:
  virtual ~TestableIndexedDBBackingStore() {}

  virtual bool WriteBlobFile(
      int64 database_id,
      const Transaction::WriteDescriptor& descriptor,
      Transaction::ChainedBlobWriter* chained_blob_writer) OVERRIDE {
    if (KeyPrefix::IsValidDatabaseId(database_id_)) {
      if (database_id_ != database_id) {
        return false;
      }
    } else {
      database_id_ = database_id;
    }
    writes_.push_back(descriptor);
    task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&Transaction::ChainedBlobWriter::ReportWriteCompletion,
                   chained_blob_writer,
                   true,
                   1));
    return true;
  }

  virtual bool RemoveBlobFile(int64 database_id, int64 key) OVERRIDE {
    if (database_id_ != database_id ||
        !KeyPrefix::IsValidDatabaseId(database_id)) {
      return false;
    }
    removals_.push_back(key);
    return true;
  }

  // Timers don't play nicely with unit tests.
  virtual void StartJournalCleaningTimer() OVERRIDE {
    CleanPrimaryJournalIgnoreReturn();
  }

 private:
  TestableIndexedDBBackingStore(IndexedDBFactory* indexed_db_factory,
                                const GURL& origin_url,
                                const base::FilePath& blob_path,
                                net::URLRequestContext* request_context,
                                scoped_ptr<LevelDBDatabase> db,
                                scoped_ptr<LevelDBComparator> comparator,
                                base::TaskRunner* task_runner)
      : IndexedDBBackingStore(indexed_db_factory,
                              origin_url,
                              blob_path,
                              request_context,
                              db.Pass(),
                              comparator.Pass(),
                              task_runner),
        database_id_(0) {}

  int64 database_id_;
  std::vector<Transaction::WriteDescriptor> writes_;
  std::vector<int64> removals_;

  DISALLOW_COPY_AND_ASSIGN(TestableIndexedDBBackingStore);
};

class TestIDBFactory : public IndexedDBFactory {
 public:
  explicit TestIDBFactory(IndexedDBContextImpl* idb_context)
      : IndexedDBFactory(idb_context) {}

  scoped_refptr<TestableIndexedDBBackingStore> OpenBackingStoreForTest(
      const GURL& origin,
      net::URLRequestContext* url_request_context) {
    blink::WebIDBDataLoss data_loss;
    std::string data_loss_reason;
    bool disk_full;
    scoped_refptr<IndexedDBBackingStore> backing_store =
        OpenBackingStore(origin,
                         context()->data_path(),
                         url_request_context,
                         &data_loss,
                         &data_loss_reason,
                         &disk_full);
    scoped_refptr<TestableIndexedDBBackingStore> testable_store =
        static_cast<TestableIndexedDBBackingStore*>(backing_store.get());
    return testable_store;
  }

 protected:
  virtual ~TestIDBFactory() {}

  virtual scoped_refptr<IndexedDBBackingStore> OpenBackingStoreHelper(
      const GURL& origin_url,
      const base::FilePath& data_directory,
      net::URLRequestContext* request_context,
      blink::WebIDBDataLoss* data_loss,
      std::string* data_loss_message,
      bool* disk_full,
      bool first_time) OVERRIDE {
    DefaultLevelDBFactory leveldb_factory;
    return TestableIndexedDBBackingStore::Open(this,
                                               origin_url,
                                               data_directory,
                                               request_context,
                                               &leveldb_factory,
                                               context()->TaskRunner());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestIDBFactory);
};

class IndexedDBBackingStoreTest : public testing::Test {
 public:
  IndexedDBBackingStoreTest() {}
  virtual void SetUp() {
    const GURL origin("http://localhost:81");
    task_runner_ = new base::TestSimpleTaskRunner();
    special_storage_policy_ = new MockSpecialStoragePolicy();
    special_storage_policy_->SetAllUnlimited(true);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    idb_context_ = new IndexedDBContextImpl(
        temp_dir_.path(), special_storage_policy_, NULL, task_runner_);
    idb_factory_ = new TestIDBFactory(idb_context_);
    backing_store_ =
        idb_factory_->OpenBackingStoreForTest(origin, &url_request_context_);

    // useful keys and values during tests
    m_value1 = IndexedDBValue("value1", std::vector<IndexedDBBlobInfo>());
    m_value2 = IndexedDBValue("value2", std::vector<IndexedDBBlobInfo>());

    m_blob_info.push_back(
        IndexedDBBlobInfo("uuid 3", base::UTF8ToUTF16("blob type"), 1));
    m_blob_info.push_back(
        IndexedDBBlobInfo("uuid 4",
                          base::FilePath(FILE_PATH_LITERAL("path/to/file")),
                          base::UTF8ToUTF16("file name"),
                          base::UTF8ToUTF16("file type")));
    m_value3 = IndexedDBValue("value3", m_blob_info);

    m_key1 = IndexedDBKey(99, blink::WebIDBKeyTypeNumber);
    m_key2 = IndexedDBKey(ASCIIToUTF16("key2"));
    m_key3 = IndexedDBKey(ASCIIToUTF16("key3"));
  }

  // This just checks the data that survive getting stored and recalled, e.g.
  // the file path and UUID will change and thus aren't verified.
  bool CheckBlobInfoMatches(const std::vector<IndexedDBBlobInfo>& reads) const {
    if (m_blob_info.size() != reads.size())
      return false;
    for (size_t i = 0; i < m_blob_info.size(); ++i) {
      const IndexedDBBlobInfo& a = m_blob_info[i];
      const IndexedDBBlobInfo& b = reads[i];
      if (a.is_file() != b.is_file())
        return false;
      if (a.type() != b.type())
        return false;
      if (a.is_file()) {
        if (a.file_name() != b.file_name())
          return false;
      } else {
        if (a.size() != b.size())
          return false;
      }
    }
    return true;
  }

  bool CheckBlobReadsMatchWrites(
      const std::vector<IndexedDBBlobInfo>& reads) const {
    if (backing_store_->writes().size() != reads.size())
      return false;
    std::set<int64> ids;
    for (size_t i = 0; i < backing_store_->writes().size(); ++i)
      ids.insert(backing_store_->writes()[i].key());
    if (ids.size() != backing_store_->writes().size())
      return false;
    for (size_t i = 0; i < reads.size(); ++i) {
      if (ids.count(reads[i].key()) != 1)
        return false;
    }
    return true;
  }

  bool CheckBlobWrites() const {
    if (backing_store_->writes().size() != m_blob_info.size())
      return false;
    for (size_t i = 0; i < backing_store_->writes().size(); ++i) {
      const IndexedDBBackingStore::Transaction::WriteDescriptor& desc =
          backing_store_->writes()[i];
      const IndexedDBBlobInfo& info = m_blob_info[i];
      if (desc.is_file() != info.is_file())
        return false;
      if (desc.is_file()) {
        if (desc.file_path() != info.file_path())
          return false;
      } else {
        if (desc.url() != GURL("blob:uuid/" + info.uuid()))
          return false;
      }
    }
    return true;
  }

  bool CheckBlobRemovals() const {
    if (backing_store_->removals().size() != backing_store_->writes().size())
      return false;
    for (size_t i = 0; i < backing_store_->writes().size(); ++i) {
      if (backing_store_->writes()[i].key() != backing_store_->removals()[i])
        return false;
    }
    return true;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  scoped_refptr<TestIDBFactory> idb_factory_;
  net::TestURLRequestContext url_request_context_;

  scoped_refptr<TestableIndexedDBBackingStore> backing_store_;

  // Sample keys and values that are consistent.
  IndexedDBKey m_key1;
  IndexedDBKey m_key2;
  IndexedDBKey m_key3;
  IndexedDBValue m_value1;
  IndexedDBValue m_value2;
  IndexedDBValue m_value3;
  std::vector<IndexedDBBlobInfo> m_blob_info;

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBBackingStoreTest);
};

class TestCallback : public IndexedDBBackingStore::BlobWriteCallback {
 public:
  TestCallback() : called(false), succeeded(false) {}
  virtual void Run(bool succeeded_in) OVERRIDE {
    called = true;
    succeeded = succeeded_in;
  }
  bool called;
  bool succeeded;

 protected:
  virtual ~TestCallback() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCallback);
};

TEST_F(IndexedDBBackingStoreTest, PutGetConsistency) {
  {
    IndexedDBBackingStore::Transaction transaction1(backing_store_);
    transaction1.Begin();
    ScopedVector<webkit_blob::BlobDataHandle> handles;
    IndexedDBBackingStore::RecordIdentifier record;
    leveldb::Status s = backing_store_->PutRecord(
        &transaction1, 1, 1, m_key1, m_value1, &handles, &record);
    EXPECT_TRUE(s.ok());
    scoped_refptr<TestCallback> callback(new TestCallback());
    EXPECT_TRUE(transaction1.CommitPhaseOne(callback).ok());
    EXPECT_TRUE(callback->called);
    EXPECT_TRUE(callback->succeeded);
    EXPECT_TRUE(transaction1.CommitPhaseTwo().ok());
  }

  {
    IndexedDBBackingStore::Transaction transaction2(backing_store_);
    transaction2.Begin();
    IndexedDBValue result_value;
    EXPECT_TRUE(
        backing_store_->GetRecord(&transaction2, 1, 1, m_key1, &result_value)
            .ok());
    scoped_refptr<TestCallback> callback(new TestCallback());
    EXPECT_TRUE(transaction2.CommitPhaseOne(callback).ok());
    EXPECT_TRUE(callback->called);
    EXPECT_TRUE(callback->succeeded);
    EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
    EXPECT_EQ(m_value1.bits, result_value.bits);
  }
}

TEST_F(IndexedDBBackingStoreTest, PutGetConsistencyWithBlobs) {
  {
    IndexedDBBackingStore::Transaction transaction1(backing_store_);
    transaction1.Begin();
    ScopedVector<webkit_blob::BlobDataHandle> handles;
    IndexedDBBackingStore::RecordIdentifier record;
    EXPECT_TRUE(backing_store_->PutRecord(&transaction1,
                                          1,
                                          1,
                                          m_key3,
                                          m_value3,
                                          &handles,
                                          &record).ok());
    scoped_refptr<TestCallback> callback(new TestCallback());
    EXPECT_TRUE(transaction1.CommitPhaseOne(callback).ok());
    task_runner_->RunUntilIdle();
    EXPECT_TRUE(CheckBlobWrites());
    EXPECT_TRUE(callback->called);
    EXPECT_TRUE(callback->succeeded);
    EXPECT_TRUE(transaction1.CommitPhaseTwo().ok());
  }

  {
    IndexedDBBackingStore::Transaction transaction2(backing_store_);
    transaction2.Begin();
    IndexedDBValue result_value;
    EXPECT_TRUE(
        backing_store_->GetRecord(&transaction2, 1, 1, m_key3, &result_value)
            .ok());
    scoped_refptr<TestCallback> callback(new TestCallback());
    EXPECT_TRUE(transaction2.CommitPhaseOne(callback).ok());
    EXPECT_TRUE(callback->called);
    EXPECT_TRUE(callback->succeeded);
    EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
    EXPECT_EQ(m_value3.bits, result_value.bits);
    EXPECT_TRUE(CheckBlobInfoMatches(result_value.blob_info));
    EXPECT_TRUE(CheckBlobReadsMatchWrites(result_value.blob_info));
  }

  {
    IndexedDBBackingStore::Transaction transaction3(backing_store_);
    transaction3.Begin();
    IndexedDBValue result_value;
    EXPECT_TRUE(backing_store_->DeleteRange(&transaction3,
                                            1,
                                            1,
                                            IndexedDBKeyRange(m_key3)).ok());
    scoped_refptr<TestCallback> callback(new TestCallback());
    EXPECT_TRUE(transaction3.CommitPhaseOne(callback).ok());
    task_runner_->RunUntilIdle();
    EXPECT_TRUE(callback->called);
    EXPECT_TRUE(callback->succeeded);
    EXPECT_TRUE(transaction3.CommitPhaseTwo().ok());
    EXPECT_TRUE(CheckBlobRemovals());
  }
}

TEST_F(IndexedDBBackingStoreTest, DeleteRange) {
  IndexedDBKey key0 = IndexedDBKey(ASCIIToUTF16("key0"));
  IndexedDBKey key1 = IndexedDBKey(ASCIIToUTF16("key1"));
  IndexedDBKey key2 = IndexedDBKey(ASCIIToUTF16("key2"));
  IndexedDBKey key3 = IndexedDBKey(ASCIIToUTF16("key3"));
  IndexedDBBlobInfo blob0("uuid 0", base::UTF8ToUTF16("type 0"), 1);
  IndexedDBBlobInfo blob1("uuid 1", base::UTF8ToUTF16("type 1"), 1);
  IndexedDBBlobInfo blob2("uuid 2", base::UTF8ToUTF16("type 2"), 1);
  IndexedDBBlobInfo blob3("uuid 3", base::UTF8ToUTF16("type 3"), 1);
  IndexedDBKeyRange ranges[] = {IndexedDBKeyRange(key1, key2, false, false),
                                IndexedDBKeyRange(key1, key2, false, false),
                                IndexedDBKeyRange(key0, key2, true, false),
                                IndexedDBKeyRange(key1, key3, false, true),
                                IndexedDBKeyRange(key0, key3, true, true)};

  for (unsigned i = 0; i < sizeof(ranges) / sizeof(IndexedDBKeyRange); ++i) {
    backing_store_->ClearWrites();
    backing_store_->ClearRemovals();

    {
      std::vector<IndexedDBBlobInfo> blob_info0, blob_info1, blob_info2,
          blob_info3;
      blob_info0.push_back(blob0);
      blob_info1.push_back(blob1);
      blob_info2.push_back(blob2);
      blob_info3.push_back(blob3);
      IndexedDBValue value0 = IndexedDBValue("value0", blob_info0);
      IndexedDBValue value1 = IndexedDBValue("value1", blob_info1);
      IndexedDBValue value2 = IndexedDBValue("value2", blob_info2);
      IndexedDBValue value3 = IndexedDBValue("value3", blob_info3);
      IndexedDBBackingStore::Transaction transaction1(backing_store_);
      transaction1.Begin();
      ScopedVector<webkit_blob::BlobDataHandle> handles;
      IndexedDBBackingStore::RecordIdentifier record;
      EXPECT_TRUE(backing_store_->PutRecord(&transaction1,
                                            1,
                                            i + 1,
                                            key0,
                                            value0,
                                            &handles,
                                            &record).ok());
      EXPECT_TRUE(backing_store_->PutRecord(&transaction1,
                                            1,
                                            i + 1,
                                            key1,
                                            value1,
                                            &handles,
                                            &record).ok());
      EXPECT_TRUE(backing_store_->PutRecord(&transaction1,
                                            1,
                                            i + 1,
                                            key2,
                                            value2,
                                            &handles,
                                            &record).ok());
      EXPECT_TRUE(backing_store_->PutRecord(&transaction1,
                                            1,
                                            i + 1,
                                            key3,
                                            value3,
                                            &handles,
                                            &record).ok());
      scoped_refptr<TestCallback> callback(new TestCallback());
      EXPECT_TRUE(transaction1.CommitPhaseOne(callback).ok());
      task_runner_->RunUntilIdle();
      EXPECT_TRUE(callback->called);
      EXPECT_TRUE(callback->succeeded);
      EXPECT_TRUE(transaction1.CommitPhaseTwo().ok());
    }

    {
      IndexedDBBackingStore::Transaction transaction2(backing_store_);
      transaction2.Begin();
      IndexedDBValue result_value;
      EXPECT_TRUE(
          backing_store_->DeleteRange(&transaction2, 1, i + 1, ranges[i]).ok());
      scoped_refptr<TestCallback> callback(new TestCallback());
      EXPECT_TRUE(transaction2.CommitPhaseOne(callback).ok());
      task_runner_->RunUntilIdle();
      EXPECT_TRUE(callback->called);
      EXPECT_TRUE(callback->succeeded);
      EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
      EXPECT_EQ(2UL, backing_store_->removals().size());
      EXPECT_EQ(backing_store_->writes()[1].key(),
                backing_store_->removals()[0]);
      EXPECT_EQ(backing_store_->writes()[2].key(),
                backing_store_->removals()[1]);
    }
  }
}

TEST_F(IndexedDBBackingStoreTest, DeleteRangeEmptyRange) {
  IndexedDBKey key0 = IndexedDBKey(ASCIIToUTF16("key0"));
  IndexedDBKey key1 = IndexedDBKey(ASCIIToUTF16("key1"));
  IndexedDBKey key2 = IndexedDBKey(ASCIIToUTF16("key2"));
  IndexedDBKey key3 = IndexedDBKey(ASCIIToUTF16("key3"));
  IndexedDBKey key4 = IndexedDBKey(ASCIIToUTF16("key4"));
  IndexedDBBlobInfo blob0("uuid 0", base::UTF8ToUTF16("type 0"), 1);
  IndexedDBBlobInfo blob1("uuid 1", base::UTF8ToUTF16("type 1"), 1);
  IndexedDBBlobInfo blob2("uuid 2", base::UTF8ToUTF16("type 2"), 1);
  IndexedDBBlobInfo blob3("uuid 3", base::UTF8ToUTF16("type 3"), 1);
  IndexedDBKeyRange ranges[] = {IndexedDBKeyRange(key3, key4, true, false),
                                IndexedDBKeyRange(key2, key1, false, false),
                                IndexedDBKeyRange(key2, key1, true, true)};

  for (unsigned i = 0; i < arraysize(ranges); ++i) {
    backing_store_->ClearWrites();
    backing_store_->ClearRemovals();

    {
      std::vector<IndexedDBBlobInfo> blob_info0, blob_info1, blob_info2,
          blob_info3;
      blob_info0.push_back(blob0);
      blob_info1.push_back(blob1);
      blob_info2.push_back(blob2);
      blob_info3.push_back(blob3);
      IndexedDBValue value0 = IndexedDBValue("value0", blob_info0);
      IndexedDBValue value1 = IndexedDBValue("value1", blob_info1);
      IndexedDBValue value2 = IndexedDBValue("value2", blob_info2);
      IndexedDBValue value3 = IndexedDBValue("value3", blob_info3);
      IndexedDBBackingStore::Transaction transaction1(backing_store_);
      transaction1.Begin();
      ScopedVector<webkit_blob::BlobDataHandle> handles;
      IndexedDBBackingStore::RecordIdentifier record;
      EXPECT_TRUE(backing_store_->PutRecord(&transaction1,
                                            1,
                                            i + 1,
                                            key0,
                                            value0,
                                            &handles,
                                            &record).ok());
      EXPECT_TRUE(backing_store_->PutRecord(&transaction1,
                                            1,
                                            i + 1,
                                            key1,
                                            value1,
                                            &handles,
                                            &record).ok());
      EXPECT_TRUE(backing_store_->PutRecord(&transaction1,
                                            1,
                                            i + 1,
                                            key2,
                                            value2,
                                            &handles,
                                            &record).ok());
      EXPECT_TRUE(backing_store_->PutRecord(&transaction1,
                                            1,
                                            i + 1,
                                            key3,
                                            value3,
                                            &handles,
                                            &record).ok());
      scoped_refptr<TestCallback> callback(new TestCallback());
      EXPECT_TRUE(transaction1.CommitPhaseOne(callback).ok());
      task_runner_->RunUntilIdle();
      EXPECT_TRUE(callback->called);
      EXPECT_TRUE(callback->succeeded);
      EXPECT_TRUE(transaction1.CommitPhaseTwo().ok());
    }

    {
      IndexedDBBackingStore::Transaction transaction2(backing_store_);
      transaction2.Begin();
      IndexedDBValue result_value;
      EXPECT_TRUE(
          backing_store_->DeleteRange(&transaction2, 1, i + 1, ranges[i]).ok());
      scoped_refptr<TestCallback> callback(new TestCallback());
      EXPECT_TRUE(transaction2.CommitPhaseOne(callback).ok());
      task_runner_->RunUntilIdle();
      EXPECT_TRUE(callback->called);
      EXPECT_TRUE(callback->succeeded);
      EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
      EXPECT_EQ(0UL, backing_store_->removals().size());
    }
  }
}

TEST_F(IndexedDBBackingStoreTest, LiveBlobJournal) {
  {
    IndexedDBBackingStore::Transaction transaction1(backing_store_);
    transaction1.Begin();
    ScopedVector<webkit_blob::BlobDataHandle> handles;
    IndexedDBBackingStore::RecordIdentifier record;
    EXPECT_TRUE(backing_store_->PutRecord(&transaction1,
                                          1,
                                          1,
                                          m_key3,
                                          m_value3,
                                          &handles,
                                          &record).ok());
    scoped_refptr<TestCallback> callback(new TestCallback());
    EXPECT_TRUE(transaction1.CommitPhaseOne(callback).ok());
    task_runner_->RunUntilIdle();
    EXPECT_TRUE(CheckBlobWrites());
    EXPECT_TRUE(callback->called);
    EXPECT_TRUE(callback->succeeded);
    EXPECT_TRUE(transaction1.CommitPhaseTwo().ok());
  }

  IndexedDBValue read_result_value;
  {
    IndexedDBBackingStore::Transaction transaction2(backing_store_);
    transaction2.Begin();
    EXPECT_TRUE(
        backing_store_->GetRecord(
                            &transaction2, 1, 1, m_key3, &read_result_value)
            .ok());
    scoped_refptr<TestCallback> callback(new TestCallback());
    EXPECT_TRUE(transaction2.CommitPhaseOne(callback).ok());
    EXPECT_TRUE(callback->called);
    EXPECT_TRUE(callback->succeeded);
    EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
    EXPECT_EQ(m_value3.bits, read_result_value.bits);
    EXPECT_TRUE(CheckBlobInfoMatches(read_result_value.blob_info));
    EXPECT_TRUE(CheckBlobReadsMatchWrites(read_result_value.blob_info));
    for (size_t i = 0; i < read_result_value.blob_info.size(); ++i) {
      read_result_value.blob_info[i].mark_used_callback().Run();
    }
  }

  {
    IndexedDBBackingStore::Transaction transaction3(backing_store_);
    transaction3.Begin();
    EXPECT_TRUE(backing_store_->DeleteRange(&transaction3,
                                            1,
                                            1,
                                            IndexedDBKeyRange(m_key3)).ok());
    scoped_refptr<TestCallback> callback(new TestCallback());
    EXPECT_TRUE(transaction3.CommitPhaseOne(callback).ok());
    task_runner_->RunUntilIdle();
    EXPECT_TRUE(callback->called);
    EXPECT_TRUE(callback->succeeded);
    EXPECT_TRUE(transaction3.CommitPhaseTwo().ok());
    EXPECT_EQ(0U, backing_store_->removals().size());
    for (size_t i = 0; i < read_result_value.blob_info.size(); ++i) {
      read_result_value.blob_info[i].release_callback().Run(
          read_result_value.blob_info[i].file_path());
    }
    task_runner_->RunUntilIdle();
    EXPECT_NE(0U, backing_store_->removals().size());
    EXPECT_TRUE(CheckBlobRemovals());
  }
}

// Make sure that using very high ( more than 32 bit ) values for database_id
// and object_store_id still work.
TEST_F(IndexedDBBackingStoreTest, HighIds) {
  const int64 high_database_id = 1ULL << 35;
  const int64 high_object_store_id = 1ULL << 39;
  // index_ids are capped at 32 bits for storage purposes.
  const int64 high_index_id = 1ULL << 29;

  const int64 invalid_high_index_id = 1ULL << 37;

  const IndexedDBKey& index_key = m_key2;
  std::string index_key_raw;
  EncodeIDBKey(index_key, &index_key_raw);
  {
    IndexedDBBackingStore::Transaction transaction1(backing_store_);
    transaction1.Begin();
    ScopedVector<webkit_blob::BlobDataHandle> handles;
    IndexedDBBackingStore::RecordIdentifier record;
    leveldb::Status s = backing_store_->PutRecord(&transaction1,
                                                  high_database_id,
                                                  high_object_store_id,
                                                  m_key1,
                                                  m_value1,
                                                  &handles,
                                                  &record);
    EXPECT_TRUE(s.ok());

    s = backing_store_->PutIndexDataForRecord(&transaction1,
                                              high_database_id,
                                              high_object_store_id,
                                              invalid_high_index_id,
                                              index_key,
                                              record);
    EXPECT_FALSE(s.ok());

    s = backing_store_->PutIndexDataForRecord(&transaction1,
                                              high_database_id,
                                              high_object_store_id,
                                              high_index_id,
                                              index_key,
                                              record);
    EXPECT_TRUE(s.ok());

    scoped_refptr<TestCallback> callback(new TestCallback());
    s = transaction1.CommitPhaseOne(callback);
    EXPECT_TRUE(s.ok());
    EXPECT_TRUE(callback->called);
    EXPECT_TRUE(callback->succeeded);
    s = transaction1.CommitPhaseTwo();
    EXPECT_TRUE(s.ok());
  }

  {
    IndexedDBBackingStore::Transaction transaction2(backing_store_);
    transaction2.Begin();
    IndexedDBValue result_value;
    leveldb::Status s = backing_store_->GetRecord(&transaction2,
                                                  high_database_id,
                                                  high_object_store_id,
                                                  m_key1,
                                                  &result_value);
    EXPECT_TRUE(s.ok());
    EXPECT_EQ(m_value1.bits, result_value.bits);

    scoped_ptr<IndexedDBKey> new_primary_key;
    s = backing_store_->GetPrimaryKeyViaIndex(&transaction2,
                                              high_database_id,
                                              high_object_store_id,
                                              invalid_high_index_id,
                                              index_key,
                                              &new_primary_key);
    EXPECT_FALSE(s.ok());

    s = backing_store_->GetPrimaryKeyViaIndex(&transaction2,
                                              high_database_id,
                                              high_object_store_id,
                                              high_index_id,
                                              index_key,
                                              &new_primary_key);
    EXPECT_TRUE(s.ok());
    EXPECT_TRUE(new_primary_key->Equals(m_key1));

    scoped_refptr<TestCallback> callback(new TestCallback());
    s = transaction2.CommitPhaseOne(callback);
    EXPECT_TRUE(s.ok());
    EXPECT_TRUE(callback->called);
    EXPECT_TRUE(callback->succeeded);
    s = transaction2.CommitPhaseTwo();
    EXPECT_TRUE(s.ok());
  }
}

// Make sure that other invalid ids do not crash.
TEST_F(IndexedDBBackingStoreTest, InvalidIds) {
  // valid ids for use when testing invalid ids
  const int64 database_id = 1;
  const int64 object_store_id = 1;
  const int64 index_id = kMinimumIndexId;
  const int64 invalid_low_index_id = 19;  // index_ids must be > kMinimumIndexId

  IndexedDBValue result_value;

  IndexedDBBackingStore::Transaction transaction1(backing_store_);
  transaction1.Begin();

  ScopedVector<webkit_blob::BlobDataHandle> handles;
  IndexedDBBackingStore::RecordIdentifier record;
  leveldb::Status s = backing_store_->PutRecord(&transaction1,
                                                database_id,
                                                KeyPrefix::kInvalidId,
                                                m_key1,
                                                m_value1,
                                                &handles,
                                                &record);
  EXPECT_FALSE(s.ok());
  s = backing_store_->PutRecord(
      &transaction1, database_id, 0, m_key1, m_value1, &handles, &record);
  EXPECT_FALSE(s.ok());
  s = backing_store_->PutRecord(&transaction1,
                                KeyPrefix::kInvalidId,
                                object_store_id,
                                m_key1,
                                m_value1,
                                &handles,
                                &record);
  EXPECT_FALSE(s.ok());
  s = backing_store_->PutRecord(
      &transaction1, 0, object_store_id, m_key1, m_value1, &handles, &record);
  EXPECT_FALSE(s.ok());

  s = backing_store_->GetRecord(
      &transaction1, database_id, KeyPrefix::kInvalidId, m_key1, &result_value);
  EXPECT_FALSE(s.ok());
  s = backing_store_->GetRecord(
      &transaction1, database_id, 0, m_key1, &result_value);
  EXPECT_FALSE(s.ok());
  s = backing_store_->GetRecord(&transaction1,
                                KeyPrefix::kInvalidId,
                                object_store_id,
                                m_key1,
                                &result_value);
  EXPECT_FALSE(s.ok());
  s = backing_store_->GetRecord(
      &transaction1, 0, object_store_id, m_key1, &result_value);
  EXPECT_FALSE(s.ok());

  scoped_ptr<IndexedDBKey> new_primary_key;
  s = backing_store_->GetPrimaryKeyViaIndex(&transaction1,
                                            database_id,
                                            object_store_id,
                                            KeyPrefix::kInvalidId,
                                            m_key1,
                                            &new_primary_key);
  EXPECT_FALSE(s.ok());
  s = backing_store_->GetPrimaryKeyViaIndex(&transaction1,
                                            database_id,
                                            object_store_id,
                                            invalid_low_index_id,
                                            m_key1,
                                            &new_primary_key);
  EXPECT_FALSE(s.ok());
  s = backing_store_->GetPrimaryKeyViaIndex(
      &transaction1, database_id, object_store_id, 0, m_key1, &new_primary_key);
  EXPECT_FALSE(s.ok());

  s = backing_store_->GetPrimaryKeyViaIndex(&transaction1,
                                            KeyPrefix::kInvalidId,
                                            object_store_id,
                                            index_id,
                                            m_key1,
                                            &new_primary_key);
  EXPECT_FALSE(s.ok());
  s = backing_store_->GetPrimaryKeyViaIndex(&transaction1,
                                            database_id,
                                            KeyPrefix::kInvalidId,
                                            index_id,
                                            m_key1,
                                            &new_primary_key);
  EXPECT_FALSE(s.ok());
}

TEST_F(IndexedDBBackingStoreTest, CreateDatabase) {
  const base::string16 database_name(ASCIIToUTF16("db1"));
  int64 database_id;
  const base::string16 version(ASCIIToUTF16("old_string_version"));
  const int64 int_version = 9;

  const int64 object_store_id = 99;
  const base::string16 object_store_name(ASCIIToUTF16("object_store1"));
  const bool auto_increment = true;
  const IndexedDBKeyPath object_store_key_path(
      ASCIIToUTF16("object_store_key"));

  const int64 index_id = 999;
  const base::string16 index_name(ASCIIToUTF16("index1"));
  const bool unique = true;
  const bool multi_entry = true;
  const IndexedDBKeyPath index_key_path(ASCIIToUTF16("index_key"));

  {
    leveldb::Status s = backing_store_->CreateIDBDatabaseMetaData(
        database_name, version, int_version, &database_id);
    EXPECT_TRUE(s.ok());
    EXPECT_GT(database_id, 0);

    IndexedDBBackingStore::Transaction transaction(backing_store_);
    transaction.Begin();

    s = backing_store_->CreateObjectStore(&transaction,
                                          database_id,
                                          object_store_id,
                                          object_store_name,
                                          object_store_key_path,
                                          auto_increment);
    EXPECT_TRUE(s.ok());

    s = backing_store_->CreateIndex(&transaction,
                                    database_id,
                                    object_store_id,
                                    index_id,
                                    index_name,
                                    index_key_path,
                                    unique,
                                    multi_entry);
    EXPECT_TRUE(s.ok());

    scoped_refptr<TestCallback> callback(new TestCallback());
    s = transaction.CommitPhaseOne(callback);
    EXPECT_TRUE(s.ok());
    EXPECT_TRUE(callback->called);
    EXPECT_TRUE(callback->succeeded);
    s = transaction.CommitPhaseTwo();
    EXPECT_TRUE(s.ok());
  }

  {
    IndexedDBDatabaseMetadata database;
    bool found;
    leveldb::Status s = backing_store_->GetIDBDatabaseMetaData(
        database_name, &database, &found);
    EXPECT_TRUE(s.ok());
    EXPECT_TRUE(found);

    // database.name is not filled in by the implementation.
    EXPECT_EQ(version, database.version);
    EXPECT_EQ(int_version, database.int_version);
    EXPECT_EQ(database_id, database.id);

    s = backing_store_->GetObjectStores(database.id, &database.object_stores);
    EXPECT_TRUE(s.ok());

    EXPECT_EQ(1UL, database.object_stores.size());
    IndexedDBObjectStoreMetadata object_store =
        database.object_stores[object_store_id];
    EXPECT_EQ(object_store_name, object_store.name);
    EXPECT_EQ(object_store_key_path, object_store.key_path);
    EXPECT_EQ(auto_increment, object_store.auto_increment);

    EXPECT_EQ(1UL, object_store.indexes.size());
    IndexedDBIndexMetadata index = object_store.indexes[index_id];
    EXPECT_EQ(index_name, index.name);
    EXPECT_EQ(index_key_path, index.key_path);
    EXPECT_EQ(unique, index.unique);
    EXPECT_EQ(multi_entry, index.multi_entry);
  }
}

}  // namespace

}  // namespace content
