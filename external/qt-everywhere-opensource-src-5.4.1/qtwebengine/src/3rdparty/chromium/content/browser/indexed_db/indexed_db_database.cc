// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database.h"

#include <math.h>
#include <set>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_index_writer.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseException.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "webkit/browser/blob/blob_data_handle.h"

using base::ASCIIToUTF16;
using base::Int64ToString16;
using blink::WebIDBKeyTypeNumber;

namespace content {

// PendingUpgradeCall has a scoped_ptr<IndexedDBConnection> because it owns the
// in-progress connection.
class IndexedDBDatabase::PendingUpgradeCall {
 public:
  PendingUpgradeCall(scoped_refptr<IndexedDBCallbacks> callbacks,
                     scoped_ptr<IndexedDBConnection> connection,
                     int64 transaction_id,
                     int64 version)
      : callbacks_(callbacks),
        connection_(connection.Pass()),
        version_(version),
        transaction_id_(transaction_id) {}
  scoped_refptr<IndexedDBCallbacks> callbacks() const { return callbacks_; }
  // Takes ownership of the connection object.
  scoped_ptr<IndexedDBConnection> ReleaseConnection() WARN_UNUSED_RESULT {
    return connection_.Pass();
  }
  int64 version() const { return version_; }
  int64 transaction_id() const { return transaction_id_; }

 private:
  scoped_refptr<IndexedDBCallbacks> callbacks_;
  scoped_ptr<IndexedDBConnection> connection_;
  int64 version_;
  const int64 transaction_id_;
};

// PendingSuccessCall has a IndexedDBConnection* because the connection is now
// owned elsewhere, but we need to cancel the success call if that connection
// closes before it is sent.
class IndexedDBDatabase::PendingSuccessCall {
 public:
  PendingSuccessCall(scoped_refptr<IndexedDBCallbacks> callbacks,
                     IndexedDBConnection* connection,
                     int64 version)
      : callbacks_(callbacks), connection_(connection), version_(version) {}
  scoped_refptr<IndexedDBCallbacks> callbacks() const { return callbacks_; }
  IndexedDBConnection* connection() const { return connection_; }
  int64 version() const { return version_; }

 private:
  scoped_refptr<IndexedDBCallbacks> callbacks_;
  IndexedDBConnection* connection_;
  int64 version_;
};

class IndexedDBDatabase::PendingDeleteCall {
 public:
  explicit PendingDeleteCall(scoped_refptr<IndexedDBCallbacks> callbacks)
      : callbacks_(callbacks) {}
  scoped_refptr<IndexedDBCallbacks> callbacks() const { return callbacks_; }

 private:
  scoped_refptr<IndexedDBCallbacks> callbacks_;
};

scoped_refptr<IndexedDBDatabase> IndexedDBDatabase::Create(
    const base::string16& name,
    IndexedDBBackingStore* backing_store,
    IndexedDBFactory* factory,
    const Identifier& unique_identifier,
    leveldb::Status* s) {
  scoped_refptr<IndexedDBDatabase> database =
      new IndexedDBDatabase(name, backing_store, factory, unique_identifier);
  *s = database->OpenInternal();
  if (s->ok())
    return database;
  else
    return NULL;
}

namespace {
const base::string16::value_type kNoStringVersion[] = {0};
}

IndexedDBDatabase::IndexedDBDatabase(const base::string16& name,
                                     IndexedDBBackingStore* backing_store,
                                     IndexedDBFactory* factory,
                                     const Identifier& unique_identifier)
    : backing_store_(backing_store),
      metadata_(name,
                kInvalidId,
                kNoStringVersion,
                IndexedDBDatabaseMetadata::NO_INT_VERSION,
                kInvalidId),
      identifier_(unique_identifier),
      factory_(factory) {
}

void IndexedDBDatabase::AddObjectStore(
    const IndexedDBObjectStoreMetadata& object_store,
    int64 new_max_object_store_id) {
  DCHECK(metadata_.object_stores.find(object_store.id) ==
         metadata_.object_stores.end());
  if (new_max_object_store_id != IndexedDBObjectStoreMetadata::kInvalidId) {
    DCHECK_LT(metadata_.max_object_store_id, new_max_object_store_id);
    metadata_.max_object_store_id = new_max_object_store_id;
  }
  metadata_.object_stores[object_store.id] = object_store;
}

void IndexedDBDatabase::RemoveObjectStore(int64 object_store_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  metadata_.object_stores.erase(object_store_id);
}

void IndexedDBDatabase::AddIndex(int64 object_store_id,
                                 const IndexedDBIndexMetadata& index,
                                 int64 new_max_index_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index.id) == object_store.indexes.end());
  object_store.indexes[index.id] = index;
  if (new_max_index_id != IndexedDBIndexMetadata::kInvalidId) {
    DCHECK_LT(object_store.max_index_id, new_max_index_id);
    object_store.max_index_id = new_max_index_id;
  }
  metadata_.object_stores[object_store_id] = object_store;
}

void IndexedDBDatabase::RemoveIndex(int64 object_store_id, int64 index_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index_id) != object_store.indexes.end());
  object_store.indexes.erase(index_id);
  metadata_.object_stores[object_store_id] = object_store;
}

leveldb::Status IndexedDBDatabase::OpenInternal() {
  bool success = false;
  leveldb::Status s = backing_store_->GetIDBDatabaseMetaData(
      metadata_.name, &metadata_, &success);
  DCHECK(success == (metadata_.id != kInvalidId)) << "success = " << success
                                                  << " id = " << metadata_.id;
  if (!s.ok())
    return s;
  if (success)
    return backing_store_->GetObjectStores(metadata_.id,
                                           &metadata_.object_stores);

  return backing_store_->CreateIDBDatabaseMetaData(
      metadata_.name, metadata_.version, metadata_.int_version, &metadata_.id);
}

IndexedDBDatabase::~IndexedDBDatabase() {
  DCHECK(transactions_.empty());
  DCHECK(pending_open_calls_.empty());
  DCHECK(pending_delete_calls_.empty());
}

scoped_ptr<IndexedDBConnection> IndexedDBDatabase::CreateConnection(
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    int child_process_id) {
  scoped_ptr<IndexedDBConnection> connection(
      new IndexedDBConnection(this, database_callbacks));
  connections_.insert(connection.get());
  backing_store_->GrantChildProcessPermissions(child_process_id);
  return connection.Pass();
}

IndexedDBTransaction* IndexedDBDatabase::GetTransaction(
    int64 transaction_id) const {
  TransactionMap::const_iterator trans_iterator =
      transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return NULL;
  return trans_iterator->second;
}

bool IndexedDBDatabase::ValidateObjectStoreId(int64 object_store_id) const {
  if (!ContainsKey(metadata_.object_stores, object_store_id)) {
    DLOG(ERROR) << "Invalid object_store_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::ValidateObjectStoreIdAndIndexId(int64 object_store_id,
                                                        int64 index_id) const {
  if (!ValidateObjectStoreId(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (!ContainsKey(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::ValidateObjectStoreIdAndOptionalIndexId(
    int64 object_store_id,
    int64 index_id) const {
  if (!ValidateObjectStoreId(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (index_id != IndexedDBIndexMetadata::kInvalidId &&
      !ContainsKey(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::ValidateObjectStoreIdAndNewIndexId(
    int64 object_store_id,
    int64 index_id) const {
  if (!ValidateObjectStoreId(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (ContainsKey(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

void IndexedDBDatabase::CreateObjectStore(int64 transaction_id,
                                          int64 object_store_id,
                                          const base::string16& name,
                                          const IndexedDBKeyPath& key_path,
                                          bool auto_increment) {
  IDB_TRACE1("IndexedDBDatabase::CreateObjectStore", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), indexed_db::TRANSACTION_VERSION_CHANGE);

  if (ContainsKey(metadata_.object_stores, object_store_id)) {
    DLOG(ERROR) << "Invalid object_store_id";
    return;
  }

  // Store creation is done synchronously, as it may be followed by
  // index creation (also sync) since preemptive OpenCursor/SetIndexKeys
  // may follow.
  IndexedDBObjectStoreMetadata object_store_metadata(
      name,
      object_store_id,
      key_path,
      auto_increment,
      IndexedDBDatabase::kMinimumIndexId);

  leveldb::Status s =
      backing_store_->CreateObjectStore(transaction->BackingStoreTransaction(),
                                        transaction->database()->id(),
                                        object_store_metadata.id,
                                        object_store_metadata.name,
                                        object_store_metadata.key_path,
                                        object_store_metadata.auto_increment);
  if (!s.ok()) {
    IndexedDBDatabaseError error(
        blink::WebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16("Internal error creating object store '") +
            object_store_metadata.name + ASCIIToUTF16("'."));
    transaction->Abort(error);
    if (leveldb_env::IsCorruption(s))
      factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                             error);
    return;
  }

  AddObjectStore(object_store_metadata, object_store_id);
  transaction->ScheduleAbortTask(
      base::Bind(&IndexedDBDatabase::CreateObjectStoreAbortOperation,
                 this,
                 object_store_id));
}

void IndexedDBDatabase::DeleteObjectStore(int64 transaction_id,
                                          int64 object_store_id) {
  IDB_TRACE1("IndexedDBDatabase::DeleteObjectStore", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), indexed_db::TRANSACTION_VERSION_CHANGE);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  transaction->ScheduleTask(
      base::Bind(&IndexedDBDatabase::DeleteObjectStoreOperation,
                 this,
                 object_store_id));
}

void IndexedDBDatabase::CreateIndex(int64 transaction_id,
                                    int64 object_store_id,
                                    int64 index_id,
                                    const base::string16& name,
                                    const IndexedDBKeyPath& key_path,
                                    bool unique,
                                    bool multi_entry) {
  IDB_TRACE1("IndexedDBDatabase::CreateIndex", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), indexed_db::TRANSACTION_VERSION_CHANGE);

  if (!ValidateObjectStoreIdAndNewIndexId(object_store_id, index_id))
    return;

  // Index creation is done synchronously since preemptive
  // OpenCursor/SetIndexKeys may follow.
  const IndexedDBIndexMetadata index_metadata(
      name, index_id, key_path, unique, multi_entry);

  if (!backing_store_->CreateIndex(transaction->BackingStoreTransaction(),
                                   transaction->database()->id(),
                                   object_store_id,
                                   index_metadata.id,
                                   index_metadata.name,
                                   index_metadata.key_path,
                                   index_metadata.unique,
                                   index_metadata.multi_entry).ok()) {
    base::string16 error_string =
        ASCIIToUTF16("Internal error creating index '") +
        index_metadata.name + ASCIIToUTF16("'.");
    transaction->Abort(IndexedDBDatabaseError(
        blink::WebIDBDatabaseExceptionUnknownError, error_string));
    return;
  }

  AddIndex(object_store_id, index_metadata, index_id);
  transaction->ScheduleAbortTask(
      base::Bind(&IndexedDBDatabase::CreateIndexAbortOperation,
                 this,
                 object_store_id,
                 index_id));
}

void IndexedDBDatabase::CreateIndexAbortOperation(
    int64 object_store_id,
    int64 index_id,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::CreateIndexAbortOperation",
             "txn.id",
             transaction->id());
  DCHECK(!transaction);
  RemoveIndex(object_store_id, index_id);
}

void IndexedDBDatabase::DeleteIndex(int64 transaction_id,
                                    int64 object_store_id,
                                    int64 index_id) {
  IDB_TRACE1("IndexedDBDatabase::DeleteIndex", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), indexed_db::TRANSACTION_VERSION_CHANGE);

  if (!ValidateObjectStoreIdAndIndexId(object_store_id, index_id))
    return;

  transaction->ScheduleTask(
      base::Bind(&IndexedDBDatabase::DeleteIndexOperation,
                 this,
                 object_store_id,
                 index_id));
}

void IndexedDBDatabase::DeleteIndexOperation(
    int64 object_store_id,
    int64 index_id,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1(
      "IndexedDBDatabase::DeleteIndexOperation", "txn.id", transaction->id());

  const IndexedDBIndexMetadata index_metadata =
      metadata_.object_stores[object_store_id].indexes[index_id];

  leveldb::Status s =
      backing_store_->DeleteIndex(transaction->BackingStoreTransaction(),
                                  transaction->database()->id(),
                                  object_store_id,
                                  index_id);
  if (!s.ok()) {
    base::string16 error_string =
        ASCIIToUTF16("Internal error deleting index '") +
        index_metadata.name + ASCIIToUTF16("'.");
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 error_string);
    transaction->Abort(error);
    if (leveldb_env::IsCorruption(s))
      factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                             error);
    return;
  }

  RemoveIndex(object_store_id, index_id);
  transaction->ScheduleAbortTask(
      base::Bind(&IndexedDBDatabase::DeleteIndexAbortOperation,
                 this,
                 object_store_id,
                 index_metadata));
}

void IndexedDBDatabase::DeleteIndexAbortOperation(
    int64 object_store_id,
    const IndexedDBIndexMetadata& index_metadata,
    IndexedDBTransaction* transaction) {
  DCHECK(!transaction);
  IDB_TRACE1("IndexedDBDatabase::DeleteIndexAbortOperation",
             "txn.id",
             transaction->id());
  AddIndex(object_store_id, index_metadata, IndexedDBIndexMetadata::kInvalidId);
}

void IndexedDBDatabase::Commit(int64 transaction_id) {
  // The frontend suggests that we commit, but we may have previously initiated
  // an abort, and so have disposed of the transaction. on_abort has already
  // been dispatched to the frontend, so it will find out about that
  // asynchronously.
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (transaction)
    transaction->Commit();
}

void IndexedDBDatabase::Abort(int64 transaction_id) {
  // If the transaction is unknown, then it has already been aborted by the
  // backend before this call so it is safe to ignore it.
  IDB_TRACE1("IndexedDBDatabase::Abort", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (transaction)
    transaction->Abort();
}

void IndexedDBDatabase::Abort(int64 transaction_id,
                              const IndexedDBDatabaseError& error) {
  IDB_TRACE1("IndexedDBDatabase::Abort(error)", "txn.id", transaction_id);
  // If the transaction is unknown, then it has already been aborted by the
  // backend before this call so it is safe to ignore it.
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (transaction)
    transaction->Abort(error);
}

void IndexedDBDatabase::Get(int64 transaction_id,
                            int64 object_store_id,
                            int64 index_id,
                            scoped_ptr<IndexedDBKeyRange> key_range,
                            bool key_only,
                            scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE1("IndexedDBDatabase::Get", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (!ValidateObjectStoreIdAndOptionalIndexId(object_store_id, index_id))
    return;

  transaction->ScheduleTask(base::Bind(
      &IndexedDBDatabase::GetOperation,
      this,
      object_store_id,
      index_id,
      Passed(&key_range),
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      callbacks));
}

void IndexedDBDatabase::GetOperation(
    int64 object_store_id,
    int64 index_id,
    scoped_ptr<IndexedDBKeyRange> key_range,
    indexed_db::CursorType cursor_type,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::GetOperation", "txn.id", transaction->id());

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  const IndexedDBKey* key;

  leveldb::Status s;
  scoped_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;
  if (key_range->IsOnlyKey()) {
    key = &key_range->lower();
  } else {
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      DCHECK_NE(cursor_type, indexed_db::CURSOR_KEY_ONLY);
      // ObjectStore Retrieval Operation
      backing_store_cursor = backing_store_->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(),
          id(),
          object_store_id,
          *key_range,
          indexed_db::CURSOR_NEXT,
          &s);
    } else if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      // Index Value Retrieval Operation
      backing_store_cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(),
          id(),
          object_store_id,
          index_id,
          *key_range,
          indexed_db::CURSOR_NEXT,
          &s);
    } else {
      // Index Referenced Value Retrieval Operation
      backing_store_cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(),
          id(),
          object_store_id,
          index_id,
          *key_range,
          indexed_db::CURSOR_NEXT,
          &s);
    }

    if (!s.ok()) {
      DLOG(ERROR) << "Unable to open cursor operation: " << s.ToString();
      IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                   "Internal error deleting data in range");
      if (leveldb_env::IsCorruption(s)) {
        factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                               error);
      }
    }

    if (!backing_store_cursor) {
      callbacks->OnSuccess();
      return;
    }

    key = &backing_store_cursor->key();
  }

  scoped_ptr<IndexedDBKey> primary_key;
  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    // Object Store Retrieval Operation
    IndexedDBValue value;
    s = backing_store_->GetRecord(transaction->BackingStoreTransaction(),
                                  id(),
                                  object_store_id,
                                  *key,
                                  &value);
    if (!s.ok()) {
      IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                   "Internal error in GetRecord.");
      callbacks->OnError(error);

      if (leveldb_env::IsCorruption(s))
        factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                               error);
      return;
    }

    if (value.empty()) {
      callbacks->OnSuccess();
      return;
    }

    if (object_store_metadata.auto_increment &&
        !object_store_metadata.key_path.IsNull()) {
      callbacks->OnSuccess(&value, *key, object_store_metadata.key_path);
      return;
    }

    callbacks->OnSuccess(&value);
    return;
  }

  // From here we are dealing only with indexes.
  s = backing_store_->GetPrimaryKeyViaIndex(
      transaction->BackingStoreTransaction(),
      id(),
      object_store_id,
      index_id,
      *key,
      &primary_key);
  if (!s.ok()) {
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error in GetPrimaryKeyViaIndex.");
    callbacks->OnError(error);
    if (leveldb_env::IsCorruption(s))
      factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                             error);
    return;
  }
  if (!primary_key) {
    callbacks->OnSuccess();
    return;
  }
  if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
    // Index Value Retrieval Operation
    callbacks->OnSuccess(*primary_key);
    return;
  }

  // Index Referenced Value Retrieval Operation
  IndexedDBValue value;
  s = backing_store_->GetRecord(transaction->BackingStoreTransaction(),
                                id(),
                                object_store_id,
                                *primary_key,
                                &value);
  if (!s.ok()) {
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error in GetRecord.");
    callbacks->OnError(error);
    if (leveldb_env::IsCorruption(s))
      factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                             error);
    return;
  }

  if (value.empty()) {
    callbacks->OnSuccess();
    return;
  }
  if (object_store_metadata.auto_increment &&
      !object_store_metadata.key_path.IsNull()) {
    callbacks->OnSuccess(&value, *primary_key, object_store_metadata.key_path);
    return;
  }
  callbacks->OnSuccess(&value);
}

static scoped_ptr<IndexedDBKey> GenerateKey(
    IndexedDBBackingStore* backing_store,
    IndexedDBTransaction* transaction,
    int64 database_id,
    int64 object_store_id) {
  const int64 max_generator_value =
      9007199254740992LL;  // Maximum integer storable as ECMAScript number.
  int64 current_number;
  leveldb::Status s = backing_store->GetKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(),
      database_id,
      object_store_id,
      &current_number);
  if (!s.ok()) {
    LOG(ERROR) << "Failed to GetKeyGeneratorCurrentNumber";
    return make_scoped_ptr(new IndexedDBKey());
  }
  if (current_number < 0 || current_number > max_generator_value)
    return make_scoped_ptr(new IndexedDBKey());

  return make_scoped_ptr(new IndexedDBKey(current_number, WebIDBKeyTypeNumber));
}

static leveldb::Status UpdateKeyGenerator(IndexedDBBackingStore* backing_store,
                                          IndexedDBTransaction* transaction,
                                          int64 database_id,
                                          int64 object_store_id,
                                          const IndexedDBKey& key,
                                          bool check_current) {
  DCHECK_EQ(WebIDBKeyTypeNumber, key.type());
  return backing_store->MaybeUpdateKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(),
      database_id,
      object_store_id,
      static_cast<int64>(floor(key.number())) + 1,
      check_current);
}

struct IndexedDBDatabase::PutOperationParams {
  PutOperationParams() {}
  int64 object_store_id;
  IndexedDBValue value;
  ScopedVector<webkit_blob::BlobDataHandle> handles;
  scoped_ptr<IndexedDBKey> key;
  IndexedDBDatabase::PutMode put_mode;
  scoped_refptr<IndexedDBCallbacks> callbacks;
  std::vector<IndexKeys> index_keys;

 private:
  DISALLOW_COPY_AND_ASSIGN(PutOperationParams);
};

void IndexedDBDatabase::Put(int64 transaction_id,
                            int64 object_store_id,
                            IndexedDBValue* value,
                            ScopedVector<webkit_blob::BlobDataHandle>* handles,
                            scoped_ptr<IndexedDBKey> key,
                            PutMode put_mode,
                            scoped_refptr<IndexedDBCallbacks> callbacks,
                            const std::vector<IndexKeys>& index_keys) {
  IDB_TRACE1("IndexedDBDatabase::Put", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_NE(transaction->mode(), indexed_db::TRANSACTION_READ_ONLY);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  DCHECK(key);
  DCHECK(value);
  scoped_ptr<PutOperationParams> params(new PutOperationParams());
  params->object_store_id = object_store_id;
  params->value.swap(*value);
  params->handles.swap(*handles);
  params->key = key.Pass();
  params->put_mode = put_mode;
  params->callbacks = callbacks;
  params->index_keys = index_keys;
  transaction->ScheduleTask(base::Bind(
      &IndexedDBDatabase::PutOperation, this, base::Passed(&params)));
}

void IndexedDBDatabase::PutOperation(scoped_ptr<PutOperationParams> params,
                                     IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::PutOperation", "txn.id", transaction->id());
  DCHECK_NE(transaction->mode(), indexed_db::TRANSACTION_READ_ONLY);
  bool key_was_generated = false;

  DCHECK(metadata_.object_stores.find(params->object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[params->object_store_id];
  DCHECK(object_store.auto_increment || params->key->IsValid());

  scoped_ptr<IndexedDBKey> key;
  if (params->put_mode != IndexedDBDatabase::CURSOR_UPDATE &&
      object_store.auto_increment && !params->key->IsValid()) {
    scoped_ptr<IndexedDBKey> auto_inc_key = GenerateKey(
        backing_store_.get(), transaction, id(), params->object_store_id);
    key_was_generated = true;
    if (!auto_inc_key->IsValid()) {
      params->callbacks->OnError(
          IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionConstraintError,
                                 "Maximum key generator value reached."));
      return;
    }
    key = auto_inc_key.Pass();
  } else {
    key = params->key.Pass();
  }

  DCHECK(key->IsValid());

  IndexedDBBackingStore::RecordIdentifier record_identifier;
  if (params->put_mode == IndexedDBDatabase::ADD_ONLY) {
    bool found = false;
    leveldb::Status s = backing_store_->KeyExistsInObjectStore(
        transaction->BackingStoreTransaction(),
        id(),
        params->object_store_id,
        *key,
        &record_identifier,
        &found);
    if (!s.ok()) {
      IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                   "Internal error checking key existence.");
      params->callbacks->OnError(error);
      if (leveldb_env::IsCorruption(s))
        factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                               error);
      return;
    }
    if (found) {
      params->callbacks->OnError(
          IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionConstraintError,
                                 "Key already exists in the object store."));
      return;
    }
  }

  ScopedVector<IndexWriter> index_writers;
  base::string16 error_message;
  bool obeys_constraints = false;
  bool backing_store_success = MakeIndexWriters(transaction,
                                                backing_store_.get(),
                                                id(),
                                                object_store,
                                                *key,
                                                key_was_generated,
                                                params->index_keys,
                                                &index_writers,
                                                &error_message,
                                                &obeys_constraints);
  if (!backing_store_success) {
    params->callbacks->OnError(IndexedDBDatabaseError(
        blink::WebIDBDatabaseExceptionUnknownError,
        "Internal error: backing store error updating index keys."));
    return;
  }
  if (!obeys_constraints) {
    params->callbacks->OnError(IndexedDBDatabaseError(
        blink::WebIDBDatabaseExceptionConstraintError, error_message));
    return;
  }

  // Before this point, don't do any mutation. After this point, rollback the
  // transaction in case of error.
  leveldb::Status s =
      backing_store_->PutRecord(transaction->BackingStoreTransaction(),
                                id(),
                                params->object_store_id,
                                *key,
                                params->value,
                                &params->handles,
                                &record_identifier);
  if (!s.ok()) {
    IndexedDBDatabaseError error(
        blink::WebIDBDatabaseExceptionUnknownError,
        "Internal error: backing store error performing put/add.");
    params->callbacks->OnError(error);
    if (leveldb_env::IsCorruption(s))
      factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                             error);
    return;
  }

  for (size_t i = 0; i < index_writers.size(); ++i) {
    IndexWriter* index_writer = index_writers[i];
    index_writer->WriteIndexKeys(record_identifier,
                                 backing_store_.get(),
                                 transaction->BackingStoreTransaction(),
                                 id(),
                                 params->object_store_id);
  }

  if (object_store.auto_increment &&
      params->put_mode != IndexedDBDatabase::CURSOR_UPDATE &&
      key->type() == WebIDBKeyTypeNumber) {
    leveldb::Status s = UpdateKeyGenerator(backing_store_.get(),
                                           transaction,
                                           id(),
                                           params->object_store_id,
                                           *key,
                                           !key_was_generated);
    if (!s.ok()) {
      IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                   "Internal error updating key generator.");
      params->callbacks->OnError(error);
      if (leveldb_env::IsCorruption(s))
        factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                               error);
      return;
    }
  }

  params->callbacks->OnSuccess(*key);
}

void IndexedDBDatabase::SetIndexKeys(int64 transaction_id,
                                     int64 object_store_id,
                                     scoped_ptr<IndexedDBKey> primary_key,
                                     const std::vector<IndexKeys>& index_keys) {
  IDB_TRACE1("IndexedDBDatabase::SetIndexKeys", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), indexed_db::TRANSACTION_VERSION_CHANGE);

  // TODO(alecflett): This method could be asynchronous, but we need to
  // evaluate if it's worth the extra complexity.
  IndexedDBBackingStore::RecordIdentifier record_identifier;
  bool found = false;
  leveldb::Status s = backing_store_->KeyExistsInObjectStore(
      transaction->BackingStoreTransaction(),
      metadata_.id,
      object_store_id,
      *primary_key,
      &record_identifier,
      &found);
  if (!s.ok()) {
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error setting index keys.");
    transaction->Abort(error);
    if (leveldb_env::IsCorruption(s))
      factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                             error);
    return;
  }
  if (!found) {
    transaction->Abort(IndexedDBDatabaseError(
        blink::WebIDBDatabaseExceptionUnknownError,
        "Internal error setting index keys for object store."));
    return;
  }

  ScopedVector<IndexWriter> index_writers;
  base::string16 error_message;
  bool obeys_constraints = false;
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];
  bool backing_store_success = MakeIndexWriters(transaction,
                                                backing_store_,
                                                id(),
                                                object_store_metadata,
                                                *primary_key,
                                                false,
                                                index_keys,
                                                &index_writers,
                                                &error_message,
                                                &obeys_constraints);
  if (!backing_store_success) {
    transaction->Abort(IndexedDBDatabaseError(
        blink::WebIDBDatabaseExceptionUnknownError,
        "Internal error: backing store error updating index keys."));
    return;
  }
  if (!obeys_constraints) {
    transaction->Abort(IndexedDBDatabaseError(
        blink::WebIDBDatabaseExceptionConstraintError, error_message));
    return;
  }

  for (size_t i = 0; i < index_writers.size(); ++i) {
    IndexWriter* index_writer = index_writers[i];
    index_writer->WriteIndexKeys(record_identifier,
                                 backing_store_,
                                 transaction->BackingStoreTransaction(),
                                 id(),
                                 object_store_id);
  }
}

void IndexedDBDatabase::SetIndexesReady(int64 transaction_id,
                                        int64,
                                        const std::vector<int64>& index_ids) {
  IDB_TRACE1("IndexedDBDatabase::SetIndexesReady", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), indexed_db::TRANSACTION_VERSION_CHANGE);

  transaction->ScheduleTask(
      IndexedDBDatabase::PREEMPTIVE_TASK,
      base::Bind(&IndexedDBDatabase::SetIndexesReadyOperation,
                 this,
                 index_ids.size()));
}

void IndexedDBDatabase::SetIndexesReadyOperation(
    size_t index_count,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::SetIndexesReadyOperation",
             "txn.id",
             transaction->id());
  for (size_t i = 0; i < index_count; ++i)
    transaction->DidCompletePreemptiveEvent();
}

struct IndexedDBDatabase::OpenCursorOperationParams {
  OpenCursorOperationParams() {}
  int64 object_store_id;
  int64 index_id;
  scoped_ptr<IndexedDBKeyRange> key_range;
  indexed_db::CursorDirection direction;
  indexed_db::CursorType cursor_type;
  IndexedDBDatabase::TaskType task_type;
  scoped_refptr<IndexedDBCallbacks> callbacks;

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenCursorOperationParams);
};

void IndexedDBDatabase::OpenCursor(
    int64 transaction_id,
    int64 object_store_id,
    int64 index_id,
    scoped_ptr<IndexedDBKeyRange> key_range,
    indexed_db::CursorDirection direction,
    bool key_only,
    TaskType task_type,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE1("IndexedDBDatabase::OpenCursor", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (!ValidateObjectStoreIdAndOptionalIndexId(object_store_id, index_id))
    return;

  scoped_ptr<OpenCursorOperationParams> params(new OpenCursorOperationParams());
  params->object_store_id = object_store_id;
  params->index_id = index_id;
  params->key_range = key_range.Pass();
  params->direction = direction;
  params->cursor_type =
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE;
  params->task_type = task_type;
  params->callbacks = callbacks;
  transaction->ScheduleTask(base::Bind(
      &IndexedDBDatabase::OpenCursorOperation, this, base::Passed(&params)));
}

void IndexedDBDatabase::OpenCursorOperation(
    scoped_ptr<OpenCursorOperationParams> params,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1(
      "IndexedDBDatabase::OpenCursorOperation", "txn.id", transaction->id());

  // The frontend has begun indexing, so this pauses the transaction
  // until the indexing is complete. This can't happen any earlier
  // because we don't want to switch to early mode in case multiple
  // indexes are being created in a row, with Put()'s in between.
  if (params->task_type == IndexedDBDatabase::PREEMPTIVE_TASK)
    transaction->AddPreemptiveEvent();

  leveldb::Status s;
  scoped_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;
  if (params->index_id == IndexedDBIndexMetadata::kInvalidId) {
    if (params->cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      DCHECK_EQ(params->task_type, IndexedDBDatabase::NORMAL_TASK);
      backing_store_cursor = backing_store_->OpenObjectStoreKeyCursor(
          transaction->BackingStoreTransaction(),
          id(),
          params->object_store_id,
          *params->key_range,
          params->direction,
          &s);
    } else {
      backing_store_cursor = backing_store_->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(),
          id(),
          params->object_store_id,
          *params->key_range,
          params->direction,
          &s);
    }
  } else {
    DCHECK_EQ(params->task_type, IndexedDBDatabase::NORMAL_TASK);
    if (params->cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      backing_store_cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(),
          id(),
          params->object_store_id,
          params->index_id,
          *params->key_range,
          params->direction,
          &s);
    } else {
      backing_store_cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(),
          id(),
          params->object_store_id,
          params->index_id,
          *params->key_range,
          params->direction,
          &s);
    }
  }

  if (!s.ok()) {
    DLOG(ERROR) << "Unable to open cursor operation: " << s.ToString();
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error opening cursor operation");
    if (leveldb_env::IsCorruption(s)) {
      factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                             error);
    }
  }

  if (!backing_store_cursor) {
    // Why is Success being called?
    params->callbacks->OnSuccess(static_cast<IndexedDBValue*>(NULL));
    return;
  }

  scoped_refptr<IndexedDBCursor> cursor =
      new IndexedDBCursor(backing_store_cursor.Pass(),
                          params->cursor_type,
                          params->task_type,
                          transaction);
  params->callbacks->OnSuccess(
      cursor, cursor->key(), cursor->primary_key(), cursor->Value());
}

void IndexedDBDatabase::Count(int64 transaction_id,
                              int64 object_store_id,
                              int64 index_id,
                              scoped_ptr<IndexedDBKeyRange> key_range,
                              scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE1("IndexedDBDatabase::Count", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (!ValidateObjectStoreIdAndOptionalIndexId(object_store_id, index_id))
    return;

  transaction->ScheduleTask(base::Bind(&IndexedDBDatabase::CountOperation,
                                       this,
                                       object_store_id,
                                       index_id,
                                       base::Passed(&key_range),
                                       callbacks));
}

void IndexedDBDatabase::CountOperation(
    int64 object_store_id,
    int64 index_id,
    scoped_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::CountOperation", "txn.id", transaction->id());
  uint32 count = 0;
  scoped_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;

  leveldb::Status s;
  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    backing_store_cursor = backing_store_->OpenObjectStoreKeyCursor(
        transaction->BackingStoreTransaction(),
        id(),
        object_store_id,
        *key_range,
        indexed_db::CURSOR_NEXT,
        &s);
  } else {
    backing_store_cursor = backing_store_->OpenIndexKeyCursor(
        transaction->BackingStoreTransaction(),
        id(),
        object_store_id,
        index_id,
        *key_range,
        indexed_db::CURSOR_NEXT,
        &s);
  }
  if (!s.ok()) {
    DLOG(ERROR) << "Unable perform count operation: " << s.ToString();
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error performing count operation");
    if (leveldb_env::IsCorruption(s)) {
      factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                             error);
    }
  }
  if (!backing_store_cursor) {
    callbacks->OnSuccess(count);
    return;
  }

  do {
    ++count;
  } while (backing_store_cursor->Continue(&s));

  // TODO(cmumford): Check for database corruption.

  callbacks->OnSuccess(count);
}

void IndexedDBDatabase::DeleteRange(
    int64 transaction_id,
    int64 object_store_id,
    scoped_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE1("IndexedDBDatabase::DeleteRange", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_NE(transaction->mode(), indexed_db::TRANSACTION_READ_ONLY);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  transaction->ScheduleTask(base::Bind(&IndexedDBDatabase::DeleteRangeOperation,
                                       this,
                                       object_store_id,
                                       base::Passed(&key_range),
                                       callbacks));
}

void IndexedDBDatabase::DeleteRangeOperation(
    int64 object_store_id,
    scoped_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1(
      "IndexedDBDatabase::DeleteRangeOperation", "txn.id", transaction->id());
  leveldb::Status s =
      backing_store_->DeleteRange(transaction->BackingStoreTransaction(),
                                  id(),
                                  object_store_id,
                                  *key_range);
  if (!s.ok()) {
    base::string16 error_string =
        ASCIIToUTF16("Internal error deleting data in range");
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 error_string);
    transaction->Abort(error);
    if (leveldb_env::IsCorruption(s)) {
      factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                             error);
    }
    return;
  }
  callbacks->OnSuccess();
}

void IndexedDBDatabase::Clear(int64 transaction_id,
                              int64 object_store_id,
                              scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE1("IndexedDBDatabase::Clear", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_NE(transaction->mode(), indexed_db::TRANSACTION_READ_ONLY);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  transaction->ScheduleTask(base::Bind(
      &IndexedDBDatabase::ClearOperation, this, object_store_id, callbacks));
}

void IndexedDBDatabase::ClearOperation(
    int64 object_store_id,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::ClearOperation", "txn.id", transaction->id());
  leveldb::Status s = backing_store_->ClearObjectStore(
      transaction->BackingStoreTransaction(), id(), object_store_id);
  if (!s.ok()) {
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error clearing object store");
    callbacks->OnError(error);
    if (leveldb_env::IsCorruption(s)) {
      factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                             error);
    }
    return;
  }
  callbacks->OnSuccess();
}

void IndexedDBDatabase::DeleteObjectStoreOperation(
    int64 object_store_id,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::DeleteObjectStoreOperation",
             "txn.id",
             transaction->id());

  const IndexedDBObjectStoreMetadata object_store_metadata =
      metadata_.object_stores[object_store_id];
  leveldb::Status s =
      backing_store_->DeleteObjectStore(transaction->BackingStoreTransaction(),
                                        transaction->database()->id(),
                                        object_store_id);
  if (!s.ok()) {
    base::string16 error_string =
        ASCIIToUTF16("Internal error deleting object store '") +
        object_store_metadata.name + ASCIIToUTF16("'.");
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 error_string);
    transaction->Abort(error);
    if (leveldb_env::IsCorruption(s))
      factory_->HandleBackingStoreCorruption(backing_store_->origin_url(),
                                             error);
    return;
  }

  RemoveObjectStore(object_store_id);
  transaction->ScheduleAbortTask(
      base::Bind(&IndexedDBDatabase::DeleteObjectStoreAbortOperation,
                 this,
                 object_store_metadata));
}

void IndexedDBDatabase::VersionChangeOperation(
    int64 version,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    scoped_ptr<IndexedDBConnection> connection,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1(
      "IndexedDBDatabase::VersionChangeOperation", "txn.id", transaction->id());
  int64 old_version = metadata_.int_version;
  DCHECK_GT(version, old_version);

  if (!backing_store_->UpdateIDBDatabaseIntVersion(
          transaction->BackingStoreTransaction(), id(), version)) {
    IndexedDBDatabaseError error(
        blink::WebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16(
            "Internal error writing data to stable storage when "
            "updating version."));
    callbacks->OnError(error);
    transaction->Abort(error);
    return;
  }

  transaction->ScheduleAbortTask(
      base::Bind(&IndexedDBDatabase::VersionChangeAbortOperation,
                 this,
                 metadata_.version,
                 metadata_.int_version));
  metadata_.int_version = version;
  metadata_.version = kNoStringVersion;

  DCHECK(!pending_second_half_open_);
  pending_second_half_open_.reset(
      new PendingSuccessCall(callbacks, connection.get(), version));
  callbacks->OnUpgradeNeeded(old_version, connection.Pass(), metadata());
}

void IndexedDBDatabase::TransactionFinished(IndexedDBTransaction* transaction,
                                            bool committed) {
  DCHECK(transactions_.find(transaction->id()) != transactions_.end());
  DCHECK_EQ(transactions_[transaction->id()], transaction);
  transactions_.erase(transaction->id());

  if (transaction->mode() == indexed_db::TRANSACTION_VERSION_CHANGE) {
    if (pending_second_half_open_) {
      if (committed) {
        DCHECK_EQ(pending_second_half_open_->version(), metadata_.int_version);
        DCHECK(metadata_.id != kInvalidId);

        // Connection was already minted for OnUpgradeNeeded callback.
        scoped_ptr<IndexedDBConnection> connection;
        pending_second_half_open_->callbacks()->OnSuccess(connection.Pass(),
                                                          this->metadata());
      } else {
        pending_second_half_open_->callbacks()->OnError(
            IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionAbortError,
                                   "Version change transaction was aborted in "
                                   "upgradeneeded event handler."));
      }
      pending_second_half_open_.reset();
    }

    // Connection queue is now unblocked.
    ProcessPendingCalls();
  }
}

void IndexedDBDatabase::TransactionCommitFailed() {
  // Factory may be null in unit tests.
  if (!factory_)
    return;
  factory_->HandleBackingStoreFailure(backing_store_->origin_url());
}

size_t IndexedDBDatabase::ConnectionCount() const {
  // This does not include pending open calls, as those should not block version
  // changes and deletes.
  return connections_.size();
}

size_t IndexedDBDatabase::PendingOpenCount() const {
  return pending_open_calls_.size();
}

size_t IndexedDBDatabase::PendingUpgradeCount() const {
  return pending_run_version_change_transaction_call_ ? 1 : 0;
}

size_t IndexedDBDatabase::RunningUpgradeCount() const {
  return pending_second_half_open_ ? 1 : 0;
}

size_t IndexedDBDatabase::PendingDeleteCount() const {
  return pending_delete_calls_.size();
}

void IndexedDBDatabase::ProcessPendingCalls() {
  if (pending_run_version_change_transaction_call_ && ConnectionCount() == 1) {
    DCHECK(pending_run_version_change_transaction_call_->version() >
           metadata_.int_version);
    scoped_ptr<PendingUpgradeCall> pending_call =
        pending_run_version_change_transaction_call_.Pass();
    RunVersionChangeTransactionFinal(pending_call->callbacks(),
                                     pending_call->ReleaseConnection(),
                                     pending_call->transaction_id(),
                                     pending_call->version());
    DCHECK_EQ(1u, ConnectionCount());
    // Fall through would be a no-op, since transaction must complete
    // asynchronously.
    DCHECK(IsDeleteDatabaseBlocked());
    DCHECK(IsOpenConnectionBlocked());
    return;
  }

  if (!IsDeleteDatabaseBlocked()) {
    PendingDeleteCallList pending_delete_calls;
    pending_delete_calls_.swap(pending_delete_calls);
    while (!pending_delete_calls.empty()) {
      // Only the first delete call will delete the database, but each must fire
      // callbacks.
      scoped_ptr<PendingDeleteCall> pending_delete_call(
          pending_delete_calls.front());
      pending_delete_calls.pop_front();
      DeleteDatabaseFinal(pending_delete_call->callbacks());
    }
    // delete_database_final should never re-queue calls.
    DCHECK(pending_delete_calls_.empty());
    // Fall through when complete, as pending opens may be unblocked.
  }

  if (!IsOpenConnectionBlocked()) {
    PendingOpenCallList pending_open_calls;
    pending_open_calls_.swap(pending_open_calls);
    while (!pending_open_calls.empty()) {
      OpenConnection(pending_open_calls.front());
      pending_open_calls.pop_front();
    }
  }
}

void IndexedDBDatabase::CreateTransaction(
    int64 transaction_id,
    IndexedDBConnection* connection,
    const std::vector<int64>& object_store_ids,
    uint16 mode) {
  IDB_TRACE1("IndexedDBDatabase::CreateTransaction", "txn.id", transaction_id);
  DCHECK(connections_.count(connection));
  DCHECK(transactions_.find(transaction_id) == transactions_.end());
  if (transactions_.find(transaction_id) != transactions_.end())
    return;

  // The transaction will add itself to this database's coordinator, which
  // manages the lifetime of the object.
  TransactionCreated(new IndexedDBTransaction(
      transaction_id,
      connection->callbacks(),
      std::set<int64>(object_store_ids.begin(), object_store_ids.end()),
      static_cast<indexed_db::TransactionMode>(mode),
      this,
      new IndexedDBBackingStore::Transaction(backing_store_)));
}

void IndexedDBDatabase::TransactionCreated(IndexedDBTransaction* transaction) {
  transactions_[transaction->id()] = transaction;
}

bool IndexedDBDatabase::IsOpenConnectionBlocked() const {
  return !pending_delete_calls_.empty() ||
         transaction_coordinator_.IsRunningVersionChangeTransaction() ||
         pending_run_version_change_transaction_call_;
}

void IndexedDBDatabase::OpenConnection(
    const IndexedDBPendingConnection& connection) {
  DCHECK(backing_store_);

  // TODO(jsbell): Should have a priority queue so that higher version
  // requests are processed first. http://crbug.com/225850
  if (IsOpenConnectionBlocked()) {
    // The backing store only detects data loss when it is first opened. The
    // presence of existing connections means we didn't even check for data loss
    // so there'd better not be any.
    DCHECK_NE(blink::WebIDBDataLossTotal, connection.callbacks->data_loss());
    pending_open_calls_.push_back(connection);
    return;
  }

  if (metadata_.id == kInvalidId) {
    // The database was deleted then immediately re-opened; OpenInternal()
    // recreates it in the backing store.
    if (OpenInternal().ok()) {
      DCHECK_EQ(IndexedDBDatabaseMetadata::NO_INT_VERSION,
                metadata_.int_version);
    } else {
      base::string16 message;
      if (connection.version == IndexedDBDatabaseMetadata::NO_INT_VERSION) {
        message = ASCIIToUTF16(
            "Internal error opening database with no version specified.");
      } else {
        message =
            ASCIIToUTF16("Internal error opening database with version ") +
            Int64ToString16(connection.version);
      }
      connection.callbacks->OnError(IndexedDBDatabaseError(
          blink::WebIDBDatabaseExceptionUnknownError, message));
      return;
    }
  }

  // We infer that the database didn't exist from its lack of either type of
  // version.
  bool is_new_database =
      metadata_.version == kNoStringVersion &&
      metadata_.int_version == IndexedDBDatabaseMetadata::NO_INT_VERSION;

  if (connection.version == IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION) {
    // For unit tests only - skip upgrade steps. Calling from script with
    // DEFAULT_INT_VERSION throws exception.
    // TODO(jsbell): DCHECK that not in unit tests.
    DCHECK(is_new_database);
    connection.callbacks->OnSuccess(
        CreateConnection(connection.database_callbacks,
                         connection.child_process_id),
        this->metadata());
    return;
  }

  // We may need to change the version.
  int64 local_version = connection.version;
  if (local_version == IndexedDBDatabaseMetadata::NO_INT_VERSION) {
    if (!is_new_database) {
      connection.callbacks->OnSuccess(
          CreateConnection(connection.database_callbacks,
                           connection.child_process_id),
          this->metadata());
      return;
    }
    // Spec says: If no version is specified and no database exists, set
    // database version to 1.
    local_version = 1;
  }

  if (local_version > metadata_.int_version) {
    RunVersionChangeTransaction(connection.callbacks,
                                CreateConnection(connection.database_callbacks,
                                                 connection.child_process_id),
                                connection.transaction_id,
                                local_version);
    return;
  }
  if (local_version < metadata_.int_version) {
    connection.callbacks->OnError(IndexedDBDatabaseError(
        blink::WebIDBDatabaseExceptionVersionError,
        ASCIIToUTF16("The requested version (") +
            Int64ToString16(local_version) +
            ASCIIToUTF16(") is less than the existing version (") +
            Int64ToString16(metadata_.int_version) + ASCIIToUTF16(").")));
    return;
  }
  DCHECK_EQ(local_version, metadata_.int_version);
  connection.callbacks->OnSuccess(
      CreateConnection(connection.database_callbacks,
                       connection.child_process_id),
      this->metadata());
}

void IndexedDBDatabase::RunVersionChangeTransaction(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    scoped_ptr<IndexedDBConnection> connection,
    int64 transaction_id,
    int64 requested_version) {

  DCHECK(callbacks);
  DCHECK(connections_.count(connection.get()));
  if (ConnectionCount() > 1) {
    DCHECK_NE(blink::WebIDBDataLossTotal, callbacks->data_loss());
    // Front end ensures the event is not fired at connections that have
    // close_pending set.
    for (ConnectionSet::const_iterator it = connections_.begin();
         it != connections_.end();
         ++it) {
      if (*it != connection.get()) {
        (*it)->callbacks()->OnVersionChange(metadata_.int_version,
                                            requested_version);
      }
    }
    // TODO(jsbell): Remove the call to OnBlocked and instead wait
    // until the frontend tells us that all the "versionchange" events
    // have been delivered.  http://crbug.com/100123
    callbacks->OnBlocked(metadata_.int_version);

    DCHECK(!pending_run_version_change_transaction_call_);
    pending_run_version_change_transaction_call_.reset(new PendingUpgradeCall(
        callbacks, connection.Pass(), transaction_id, requested_version));
    return;
  }
  RunVersionChangeTransactionFinal(
      callbacks, connection.Pass(), transaction_id, requested_version);
}

void IndexedDBDatabase::RunVersionChangeTransactionFinal(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    scoped_ptr<IndexedDBConnection> connection,
    int64 transaction_id,
    int64 requested_version) {

  std::vector<int64> object_store_ids;
  CreateTransaction(transaction_id,
                    connection.get(),
                    object_store_ids,
                    indexed_db::TRANSACTION_VERSION_CHANGE);

  transactions_[transaction_id]->ScheduleTask(
      base::Bind(&IndexedDBDatabase::VersionChangeOperation,
                 this,
                 requested_version,
                 callbacks,
                 base::Passed(&connection)));
  DCHECK(!pending_second_half_open_);
}

void IndexedDBDatabase::DeleteDatabase(
    scoped_refptr<IndexedDBCallbacks> callbacks) {

  if (IsDeleteDatabaseBlocked()) {
    for (ConnectionSet::const_iterator it = connections_.begin();
         it != connections_.end();
         ++it) {
      // Front end ensures the event is not fired at connections that have
      // close_pending set.
      (*it)->callbacks()->OnVersionChange(
          metadata_.int_version, IndexedDBDatabaseMetadata::NO_INT_VERSION);
    }
    // TODO(jsbell): Only fire OnBlocked if there are open
    // connections after the VersionChangeEvents are received, not
    // just set up to fire.  http://crbug.com/100123
    callbacks->OnBlocked(metadata_.int_version);
    pending_delete_calls_.push_back(new PendingDeleteCall(callbacks));
    return;
  }
  DeleteDatabaseFinal(callbacks);
}

bool IndexedDBDatabase::IsDeleteDatabaseBlocked() const {
  return !!ConnectionCount();
}

void IndexedDBDatabase::DeleteDatabaseFinal(
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  DCHECK(!IsDeleteDatabaseBlocked());
  DCHECK(backing_store_);
  if (!backing_store_->DeleteDatabase(metadata_.name).ok()) {
    callbacks->OnError(
        IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                               "Internal error deleting database."));
    return;
  }
  int64 old_version = metadata_.int_version;
  metadata_.version = kNoStringVersion;
  metadata_.id = kInvalidId;
  metadata_.int_version = IndexedDBDatabaseMetadata::NO_INT_VERSION;
  metadata_.object_stores.clear();
  callbacks->OnSuccess(old_version);
  if (factory_)
    factory_->DatabaseDeleted(identifier_);
}

void IndexedDBDatabase::ForceClose() {
  // IndexedDBConnection::ForceClose() may delete this database, so hold ref.
  scoped_refptr<IndexedDBDatabase> protect(this);
  ConnectionSet::const_iterator it = connections_.begin();
  while (it != connections_.end()) {
    IndexedDBConnection* connection = *it++;
    connection->ForceClose();
  }
  DCHECK(connections_.empty());
}

void IndexedDBDatabase::Close(IndexedDBConnection* connection, bool forced) {
  DCHECK(connections_.count(connection));
  DCHECK(connection->IsConnected());
  DCHECK(connection->database() == this);

  IDB_TRACE("IndexedDBDatabase::Close");
  // Abort outstanding transactions from the closing connection. This
  // can not happen if the close is requested by the connection itself
  // as the front-end defers the close until all transactions are
  // complete, but can occur on process termination or forced close.
  {
    TransactionMap transactions(transactions_);
    for (TransactionMap::const_iterator it = transactions.begin(),
                                        end = transactions.end();
         it != end;
         ++it) {
      if (it->second->connection() == connection->callbacks())
        it->second->Abort(
            IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                                   "Connection is closing."));
    }
  }

  connections_.erase(connection);
  if (pending_second_half_open_ &&
      pending_second_half_open_->connection() == connection) {
    pending_second_half_open_->callbacks()->OnError(
        IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionAbortError,
                               "The connection was closed."));
    pending_second_half_open_.reset();
  }

  ProcessPendingCalls();

  // TODO(jsbell): Add a test for the pending_open_calls_ cases below.
  if (!ConnectionCount() && !pending_open_calls_.size() &&
      !pending_delete_calls_.size()) {
    DCHECK(transactions_.empty());

    const GURL origin_url = backing_store_->origin_url();
    backing_store_ = NULL;

    // factory_ should only be null in unit tests.
    // TODO(jsbell): DCHECK(factory_ || !in_unit_tests) - somehow.
    if (factory_) {
      factory_->ReleaseDatabase(identifier_, forced);
      factory_ = NULL;
    }
  }
}

void IndexedDBDatabase::CreateObjectStoreAbortOperation(
    int64 object_store_id,
    IndexedDBTransaction* transaction) {
  DCHECK(!transaction);
  IDB_TRACE1("IndexedDBDatabase::CreateObjectStoreAbortOperation",
             "txn.id",
             transaction->id());
  RemoveObjectStore(object_store_id);
}

void IndexedDBDatabase::DeleteObjectStoreAbortOperation(
    const IndexedDBObjectStoreMetadata& object_store_metadata,
    IndexedDBTransaction* transaction) {
  DCHECK(!transaction);
  IDB_TRACE1("IndexedDBDatabase::DeleteObjectStoreAbortOperation",
             "txn.id",
             transaction->id());
  AddObjectStore(object_store_metadata,
                 IndexedDBObjectStoreMetadata::kInvalidId);
}

void IndexedDBDatabase::VersionChangeAbortOperation(
    const base::string16& previous_version,
    int64 previous_int_version,
    IndexedDBTransaction* transaction) {
  DCHECK(!transaction);
  IDB_TRACE1("IndexedDBDatabase::VersionChangeAbortOperation",
             "txn.id",
             transaction->id());
  metadata_.version = previous_version;
  metadata_.int_version = previous_int_version;
}

}  // namespace content
