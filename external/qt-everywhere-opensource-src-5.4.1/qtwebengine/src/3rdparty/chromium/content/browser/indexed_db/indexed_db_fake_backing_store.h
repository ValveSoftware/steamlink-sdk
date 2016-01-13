// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_

#include <vector>

#include "content/browser/indexed_db/indexed_db_backing_store.h"

namespace base {
class TaskRunner;
}

namespace content {

class IndexedDBFactory;

class IndexedDBFakeBackingStore : public IndexedDBBackingStore {
 public:
  IndexedDBFakeBackingStore();
  IndexedDBFakeBackingStore(IndexedDBFactory* factory,
                            base::TaskRunner* task_runner);
  virtual std::vector<base::string16> GetDatabaseNames(leveldb::Status* s)
      OVERRIDE;
  virtual leveldb::Status GetIDBDatabaseMetaData(const base::string16& name,
                                                 IndexedDBDatabaseMetadata*,
                                                 bool* found) OVERRIDE;
  virtual leveldb::Status CreateIDBDatabaseMetaData(
      const base::string16& name,
      const base::string16& version,
      int64 int_version,
      int64* row_id) OVERRIDE;
  virtual bool UpdateIDBDatabaseIntVersion(Transaction*,
                                           int64 row_id,
                                           int64 version) OVERRIDE;
  virtual leveldb::Status DeleteDatabase(const base::string16& name) OVERRIDE;

  virtual leveldb::Status CreateObjectStore(Transaction*,
                                            int64 database_id,
                                            int64 object_store_id,
                                            const base::string16& name,
                                            const IndexedDBKeyPath&,
                                            bool auto_increment) OVERRIDE;

  virtual leveldb::Status DeleteObjectStore(Transaction* transaction,
                                            int64 database_id,
                                            int64 object_store_id) OVERRIDE;

  virtual leveldb::Status PutRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKey& key,
      IndexedDBValue& value,
      ScopedVector<webkit_blob::BlobDataHandle>* handles,
      RecordIdentifier* record) OVERRIDE;

  virtual leveldb::Status ClearObjectStore(Transaction*,
                                           int64 database_id,
                                           int64 object_store_id) OVERRIDE;
  virtual leveldb::Status DeleteRecord(Transaction*,
                                       int64 database_id,
                                       int64 object_store_id,
                                       const RecordIdentifier&) OVERRIDE;
  virtual leveldb::Status GetKeyGeneratorCurrentNumber(Transaction*,
                                                       int64 database_id,
                                                       int64 object_store_id,
                                                       int64* current_number)
      OVERRIDE;
  virtual leveldb::Status MaybeUpdateKeyGeneratorCurrentNumber(
      Transaction*,
      int64 database_id,
      int64 object_store_id,
      int64 new_number,
      bool check_current) OVERRIDE;
  virtual leveldb::Status KeyExistsInObjectStore(
      Transaction*,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKey&,
      RecordIdentifier* found_record_identifier,
      bool* found) OVERRIDE;

  virtual leveldb::Status CreateIndex(Transaction*,
                                      int64 database_id,
                                      int64 object_store_id,
                                      int64 index_id,
                                      const base::string16& name,
                                      const IndexedDBKeyPath&,
                                      bool is_unique,
                                      bool is_multi_entry) OVERRIDE;
  virtual leveldb::Status DeleteIndex(Transaction*,
                                      int64 database_id,
                                      int64 object_store_id,
                                      int64 index_id) OVERRIDE;
  virtual leveldb::Status PutIndexDataForRecord(Transaction*,
                                                int64 database_id,
                                                int64 object_store_id,
                                                int64 index_id,
                                                const IndexedDBKey&,
                                                const RecordIdentifier&)
      OVERRIDE;
  virtual void ReportBlobUnused(int64 database_id, int64 blob_key) OVERRIDE;
  virtual scoped_ptr<Cursor> OpenObjectStoreKeyCursor(
      Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection,
      leveldb::Status*) OVERRIDE;
  virtual scoped_ptr<Cursor> OpenObjectStoreCursor(
      Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection,
      leveldb::Status*) OVERRIDE;
  virtual scoped_ptr<Cursor> OpenIndexKeyCursor(
      Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection,
      leveldb::Status*) OVERRIDE;
  virtual scoped_ptr<Cursor> OpenIndexCursor(Transaction* transaction,
                                             int64 database_id,
                                             int64 object_store_id,
                                             int64 index_id,
                                             const IndexedDBKeyRange& key_range,
                                             indexed_db::CursorDirection,
                                             leveldb::Status*) OVERRIDE;

  class FakeTransaction : public IndexedDBBackingStore::Transaction {
   public:
    explicit FakeTransaction(leveldb::Status phase_two_result);
    virtual void Begin() OVERRIDE;
    virtual leveldb::Status CommitPhaseOne(
        scoped_refptr<BlobWriteCallback>) OVERRIDE;
    virtual leveldb::Status CommitPhaseTwo() OVERRIDE;
    virtual void Rollback() OVERRIDE;

   private:
    leveldb::Status result_;

    DISALLOW_COPY_AND_ASSIGN(FakeTransaction);
  };

 protected:
  friend class base::RefCounted<IndexedDBFakeBackingStore>;
  virtual ~IndexedDBFakeBackingStore();

  DISALLOW_COPY_AND_ASSIGN(IndexedDBFakeBackingStore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_
