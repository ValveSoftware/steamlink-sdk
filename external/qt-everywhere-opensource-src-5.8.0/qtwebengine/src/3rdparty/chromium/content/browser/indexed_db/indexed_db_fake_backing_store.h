// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {

class IndexedDBFactory;

class IndexedDBFakeBackingStore : public IndexedDBBackingStore {
 public:
  IndexedDBFakeBackingStore();
  IndexedDBFakeBackingStore(IndexedDBFactory* factory,
                            base::SequencedTaskRunner* task_runner);
  std::vector<base::string16> GetDatabaseNames(leveldb::Status* s) override;
  leveldb::Status GetIDBDatabaseMetaData(const base::string16& name,
                                         IndexedDBDatabaseMetadata*,
                                         bool* found) override;
  leveldb::Status CreateIDBDatabaseMetaData(const base::string16& name,
                                            int64_t version,
                                            int64_t* row_id) override;
  bool UpdateIDBDatabaseIntVersion(Transaction*,
                                   int64_t row_id,
                                   int64_t version) override;
  leveldb::Status DeleteDatabase(const base::string16& name) override;

  leveldb::Status CreateObjectStore(Transaction*,
                                    int64_t database_id,
                                    int64_t object_store_id,
                                    const base::string16& name,
                                    const IndexedDBKeyPath&,
                                    bool auto_increment) override;

  leveldb::Status DeleteObjectStore(Transaction* transaction,
                                    int64_t database_id,
                                    int64_t object_store_id) override;

  leveldb::Status PutRecord(IndexedDBBackingStore::Transaction* transaction,
                            int64_t database_id,
                            int64_t object_store_id,
                            const IndexedDBKey& key,
                            IndexedDBValue* value,
                            ScopedVector<storage::BlobDataHandle>* handles,
                            RecordIdentifier* record) override;

  leveldb::Status ClearObjectStore(Transaction*,
                                   int64_t database_id,
                                   int64_t object_store_id) override;
  leveldb::Status DeleteRecord(Transaction*,
                               int64_t database_id,
                               int64_t object_store_id,
                               const RecordIdentifier&) override;
  leveldb::Status GetKeyGeneratorCurrentNumber(
      Transaction*,
      int64_t database_id,
      int64_t object_store_id,
      int64_t* current_number) override;
  leveldb::Status MaybeUpdateKeyGeneratorCurrentNumber(
      Transaction*,
      int64_t database_id,
      int64_t object_store_id,
      int64_t new_number,
      bool check_current) override;
  leveldb::Status KeyExistsInObjectStore(
      Transaction*,
      int64_t database_id,
      int64_t object_store_id,
      const IndexedDBKey&,
      RecordIdentifier* found_record_identifier,
      bool* found) override;

  leveldb::Status CreateIndex(Transaction*,
                              int64_t database_id,
                              int64_t object_store_id,
                              int64_t index_id,
                              const base::string16& name,
                              const IndexedDBKeyPath&,
                              bool is_unique,
                              bool is_multi_entry) override;
  leveldb::Status DeleteIndex(Transaction*,
                              int64_t database_id,
                              int64_t object_store_id,
                              int64_t index_id) override;
  leveldb::Status PutIndexDataForRecord(Transaction*,
                                        int64_t database_id,
                                        int64_t object_store_id,
                                        int64_t index_id,
                                        const IndexedDBKey&,
                                        const RecordIdentifier&) override;
  void ReportBlobUnused(int64_t database_id, int64_t blob_key) override;
  std::unique_ptr<Cursor> OpenObjectStoreKeyCursor(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const IndexedDBKeyRange& key_range,
      blink::WebIDBCursorDirection,
      leveldb::Status*) override;
  std::unique_ptr<Cursor> OpenObjectStoreCursor(
      Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const IndexedDBKeyRange& key_range,
      blink::WebIDBCursorDirection,
      leveldb::Status*) override;
  std::unique_ptr<Cursor> OpenIndexKeyCursor(Transaction* transaction,
                                             int64_t database_id,
                                             int64_t object_store_id,
                                             int64_t index_id,
                                             const IndexedDBKeyRange& key_range,
                                             blink::WebIDBCursorDirection,
                                             leveldb::Status*) override;
  std::unique_ptr<Cursor> OpenIndexCursor(Transaction* transaction,
                                          int64_t database_id,
                                          int64_t object_store_id,
                                          int64_t index_id,
                                          const IndexedDBKeyRange& key_range,
                                          blink::WebIDBCursorDirection,
                                          leveldb::Status*) override;

  class FakeTransaction : public IndexedDBBackingStore::Transaction {
   public:
    explicit FakeTransaction(leveldb::Status phase_two_result);
    void Begin() override;
    leveldb::Status CommitPhaseOne(scoped_refptr<BlobWriteCallback>) override;
    leveldb::Status CommitPhaseTwo() override;
    void Rollback() override;

   private:
    leveldb::Status result_;

    DISALLOW_COPY_AND_ASSIGN(FakeTransaction);
  };

 protected:
  friend class base::RefCounted<IndexedDBFakeBackingStore>;
  ~IndexedDBFakeBackingStore() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBFakeBackingStore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_
