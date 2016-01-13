// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cerrno>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/leveldb/leveldb_database.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"

using base::StringPiece;
using content::IndexedDBBackingStore;
using content::LevelDBComparator;
using content::LevelDBDatabase;
using content::LevelDBFactory;
using content::LevelDBSnapshot;

namespace base {
class TaskRunner;
}

namespace content {
class IndexedDBFactory;
}

namespace net {
class URLRequestContext;
}

namespace {

class BustedLevelDBDatabase : public LevelDBDatabase {
 public:
  BustedLevelDBDatabase() {}
  static scoped_ptr<LevelDBDatabase> Open(
      const base::FilePath& file_name,
      const LevelDBComparator* /*comparator*/) {
    return scoped_ptr<LevelDBDatabase>(new BustedLevelDBDatabase);
  }
  virtual leveldb::Status Get(const base::StringPiece& key,
                              std::string* value,
                              bool* found,
                              const LevelDBSnapshot* = 0) OVERRIDE {
    return leveldb::Status::IOError("It's busted!");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BustedLevelDBDatabase);
};

class MockLevelDBFactory : public LevelDBFactory {
 public:
  MockLevelDBFactory() : destroy_called_(false) {}
  virtual leveldb::Status OpenLevelDB(
      const base::FilePath& file_name,
      const LevelDBComparator* comparator,
      scoped_ptr<LevelDBDatabase>* db,
      bool* is_disk_full = 0) OVERRIDE {
    *db = BustedLevelDBDatabase::Open(file_name, comparator);
    return leveldb::Status::OK();
  }
  virtual leveldb::Status DestroyLevelDB(const base::FilePath& file_name)
      OVERRIDE {
    EXPECT_FALSE(destroy_called_);
    destroy_called_ = true;
    return leveldb::Status::IOError("error");
  }
  virtual ~MockLevelDBFactory() { EXPECT_TRUE(destroy_called_); }

 private:
  bool destroy_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLevelDBFactory);
};

TEST(IndexedDBIOErrorTest, CleanUpTest) {
  content::IndexedDBFactory* factory = NULL;
  const GURL origin("http://localhost:81");
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath path = temp_directory.path();
  net::URLRequestContext* request_context = NULL;
  MockLevelDBFactory mock_leveldb_factory;
  blink::WebIDBDataLoss data_loss =
      blink::WebIDBDataLossNone;
  std::string data_loss_message;
  bool disk_full = false;
  base::TaskRunner* task_runner = NULL;
  bool clean_journal = false;
  scoped_refptr<IndexedDBBackingStore> backing_store =
      IndexedDBBackingStore::Open(factory,
                                  origin,
                                  path,
                                  request_context,
                                  &data_loss,
                                  &data_loss_message,
                                  &disk_full,
                                  &mock_leveldb_factory,
                                  task_runner,
                                  clean_journal);
}

// TODO(dgrogan): Remove expect_destroy if we end up not using it again. It is
// currently set to false in all 4 calls below.
template <class T>
class MockErrorLevelDBFactory : public LevelDBFactory {
 public:
  MockErrorLevelDBFactory(T error, bool expect_destroy)
      : error_(error),
        expect_destroy_(expect_destroy),
        destroy_called_(false) {}
  virtual leveldb::Status OpenLevelDB(
      const base::FilePath& file_name,
      const LevelDBComparator* comparator,
      scoped_ptr<LevelDBDatabase>* db,
      bool* is_disk_full = 0) OVERRIDE {
    return MakeIOError(
        "some filename", "some message", leveldb_env::kNewLogger, error_);
  }
  virtual leveldb::Status DestroyLevelDB(const base::FilePath& file_name)
      OVERRIDE {
    EXPECT_FALSE(destroy_called_);
    destroy_called_ = true;
    return leveldb::Status::IOError("error");
  }
  virtual ~MockErrorLevelDBFactory() {
    EXPECT_EQ(expect_destroy_, destroy_called_);
  }

 private:
  T error_;
  bool expect_destroy_;
  bool destroy_called_;

  DISALLOW_COPY_AND_ASSIGN(MockErrorLevelDBFactory);
};

TEST(IndexedDBNonRecoverableIOErrorTest, NuancedCleanupTest) {
  content::IndexedDBFactory* factory = NULL;
  const GURL origin("http://localhost:81");
  net::URLRequestContext* request_context = NULL;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath path = temp_directory.path();
  blink::WebIDBDataLoss data_loss =
      blink::WebIDBDataLossNone;
  std::string data_loss_reason;
  bool disk_full = false;
  base::TaskRunner* task_runner = NULL;
  bool clean_journal = false;

  MockErrorLevelDBFactory<int> mock_leveldb_factory(ENOSPC, false);
  scoped_refptr<IndexedDBBackingStore> backing_store =
      IndexedDBBackingStore::Open(factory,
                                  origin,
                                  path,
                                  request_context,
                                  &data_loss,
                                  &data_loss_reason,
                                  &disk_full,
                                  &mock_leveldb_factory,
                                  task_runner,
                                  clean_journal);

  MockErrorLevelDBFactory<base::File::Error> mock_leveldb_factory2(
      base::File::FILE_ERROR_NO_MEMORY, false);
  scoped_refptr<IndexedDBBackingStore> backing_store2 =
      IndexedDBBackingStore::Open(factory,
                                  origin,
                                  path,
                                  request_context,
                                  &data_loss,
                                  &data_loss_reason,
                                  &disk_full,
                                  &mock_leveldb_factory2,
                                  task_runner,
                                  clean_journal);

  MockErrorLevelDBFactory<int> mock_leveldb_factory3(EIO, false);
  scoped_refptr<IndexedDBBackingStore> backing_store3 =
      IndexedDBBackingStore::Open(factory,
                                  origin,
                                  path,
                                  request_context,
                                  &data_loss,
                                  &data_loss_reason,
                                  &disk_full,
                                  &mock_leveldb_factory3,
                                  task_runner,
                                  clean_journal);

  MockErrorLevelDBFactory<base::File::Error> mock_leveldb_factory4(
      base::File::FILE_ERROR_FAILED, false);
  scoped_refptr<IndexedDBBackingStore> backing_store4 =
      IndexedDBBackingStore::Open(factory,
                                  origin,
                                  path,
                                  request_context,
                                  &data_loss,
                                  &data_loss_reason,
                                  &disk_full,
                                  &mock_leveldb_factory4,
                                  task_runner,
                                  clean_journal);
}

}  // namespace
