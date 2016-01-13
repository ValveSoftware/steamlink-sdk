// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"

namespace content {

IndexedDBFakeBackingStore::IndexedDBFakeBackingStore()
    : IndexedDBBackingStore(NULL /* indexed_db_factory */,
                            GURL("http://localhost:81"),
                            base::FilePath(),
                            NULL /* request_context */,
                            scoped_ptr<LevelDBDatabase>(),
                            scoped_ptr<LevelDBComparator>(),
                            NULL /* task_runner */) {
}
IndexedDBFakeBackingStore::IndexedDBFakeBackingStore(
    IndexedDBFactory* factory,
    base::TaskRunner* task_runner)
    : IndexedDBBackingStore(factory,
                            GURL("http://localhost:81"),
                            base::FilePath(),
                            NULL /* request_context */,
                            scoped_ptr<LevelDBDatabase>(),
                            scoped_ptr<LevelDBComparator>(),
                            task_runner) {
}
IndexedDBFakeBackingStore::~IndexedDBFakeBackingStore() {}

std::vector<base::string16> IndexedDBFakeBackingStore::GetDatabaseNames(
    leveldb::Status* s) {
  *s = leveldb::Status::OK();
  return std::vector<base::string16>();
}
leveldb::Status IndexedDBFakeBackingStore::GetIDBDatabaseMetaData(
    const base::string16& name,
    IndexedDBDatabaseMetadata*,
    bool* found) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::CreateIDBDatabaseMetaData(
    const base::string16& name,
    const base::string16& version,
    int64 int_version,
    int64* row_id) {
  return leveldb::Status::OK();
}
bool IndexedDBFakeBackingStore::UpdateIDBDatabaseIntVersion(Transaction*,
                                                            int64 row_id,
                                                            int64 version) {
  return false;
}
leveldb::Status IndexedDBFakeBackingStore::DeleteDatabase(
    const base::string16& name) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::CreateObjectStore(
    Transaction*,
    int64 database_id,
    int64 object_store_id,
    const base::string16& name,
    const IndexedDBKeyPath&,
    bool auto_increment) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::DeleteObjectStore(
    Transaction* transaction,
    int64 database_id,
    int64 object_store_id) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::PutRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKey& key,
    IndexedDBValue& value,
    ScopedVector<webkit_blob::BlobDataHandle>* handles,
    RecordIdentifier* record) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::ClearObjectStore(
    Transaction*,
    int64 database_id,
    int64 object_store_id) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::DeleteRecord(
    Transaction*,
    int64 database_id,
    int64 object_store_id,
    const RecordIdentifier&) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::GetKeyGeneratorCurrentNumber(
    Transaction*,
    int64 database_id,
    int64 object_store_id,
    int64* current_number) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::MaybeUpdateKeyGeneratorCurrentNumber(
    Transaction*,
    int64 database_id,
    int64 object_store_id,
    int64 new_number,
    bool check_current) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::KeyExistsInObjectStore(
    Transaction*,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKey&,
    RecordIdentifier* found_record_identifier,
    bool* found) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::CreateIndex(
    Transaction*,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const base::string16& name,
    const IndexedDBKeyPath&,
    bool is_unique,
    bool is_multi_entry) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::DeleteIndex(Transaction*,
                                                       int64 database_id,
                                                       int64 object_store_id,
                                                       int64 index_id) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::PutIndexDataForRecord(
    Transaction*,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKey&,
    const RecordIdentifier&) {
  return leveldb::Status::OK();
}

void IndexedDBFakeBackingStore::ReportBlobUnused(int64 database_id,
                                                 int64 blob_key) {}

scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenObjectStoreKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKeyRange& key_range,
    indexed_db::CursorDirection,
    leveldb::Status* s) {
  return scoped_ptr<IndexedDBBackingStore::Cursor>();
}
scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenObjectStoreCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKeyRange& key_range,
    indexed_db::CursorDirection,
    leveldb::Status* s) {
  return scoped_ptr<IndexedDBBackingStore::Cursor>();
}
scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenIndexKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& key_range,
    indexed_db::CursorDirection,
    leveldb::Status* s) {
  return scoped_ptr<IndexedDBBackingStore::Cursor>();
}
scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenIndexCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& key_range,
    indexed_db::CursorDirection,
    leveldb::Status* s) {
  return scoped_ptr<IndexedDBBackingStore::Cursor>();
}

IndexedDBFakeBackingStore::FakeTransaction::FakeTransaction(
    leveldb::Status result)
    : IndexedDBBackingStore::Transaction(NULL), result_(result) {
}
void IndexedDBFakeBackingStore::FakeTransaction::Begin() {}
leveldb::Status IndexedDBFakeBackingStore::FakeTransaction::CommitPhaseOne(
    scoped_refptr<BlobWriteCallback> callback) {
  callback->Run(true);
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::FakeTransaction::CommitPhaseTwo() {
  return result_;
}
void IndexedDBFakeBackingStore::FakeTransaction::Rollback() {}

}  // namespace content
