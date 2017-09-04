// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database.h"

#include <math.h>

#include <limits>
#include <set>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_index_writer.h"
#include "content/browser/indexed_db/indexed_db_observation.h"
#include "content/browser/indexed_db/indexed_db_observer_changes.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_return_value.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "content/public/common/content_switches.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseException.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "url/origin.h"

using base::ASCIIToUTF16;
using base::Int64ToString16;
using blink::WebIDBKeyTypeNumber;

namespace content {

namespace {

// Used for WebCore.IndexedDB.Schema.ObjectStore.KeyPathType and
// WebCore.IndexedDB.Schema.Index.KeyPathType histograms. Do not
// modify (delete, re-order, renumber) these values other than
// the _MAX value.
enum HistogramIDBKeyPathType {
  KEY_PATH_TYPE_NONE = 0,
  KEY_PATH_TYPE_STRING = 1,
  KEY_PATH_TYPE_ARRAY = 2,
  KEY_PATH_TYPE_MAX = 3,  // Keep as last/max entry, for histogram range.
};

HistogramIDBKeyPathType HistogramKeyPathType(const IndexedDBKeyPath& key_path) {
  switch (key_path.type()) {
    case blink::WebIDBKeyPathTypeNull:
      return KEY_PATH_TYPE_NONE;
    case blink::WebIDBKeyPathTypeString:
      return KEY_PATH_TYPE_STRING;
    case blink::WebIDBKeyPathTypeArray:
      return KEY_PATH_TYPE_ARRAY;
  }
  NOTREACHED();
  return KEY_PATH_TYPE_NONE;
}

}  // namespace

// This represents what script calls an 'IDBOpenDBRequest' - either a database
// open or delete call. These may be blocked on other connections. After every
// callback, the request must call IndexedDBDatabase::RequestComplete() or be
// expecting a further callback.
class IndexedDBDatabase::ConnectionRequest {
 public:
  explicit ConnectionRequest(scoped_refptr<IndexedDBDatabase> db) : db_(db) {}

  virtual ~ConnectionRequest() {}

  // Called when the request makes it to the front of the queue.
  virtual void Perform() = 0;

  // Called if a front-end signals that it is ignoring a "versionchange"
  // event. This should result in firing a "blocked" event at the request.
  virtual void OnVersionChangeIgnored() const = 0;

  // Called when a connection is closed; if it corresponds to this connection,
  // need to do cleanup. Otherwise, it may unblock further steps.
  virtual void OnConnectionClosed(IndexedDBConnection* connection) = 0;

  // Called when the upgrade transaction has started executing.
  virtual void UpgradeTransactionStarted(int64_t old_version) = 0;

  // Called when the upgrade transaction has finished.
  virtual void UpgradeTransactionFinished(bool committed) = 0;

 protected:
  scoped_refptr<IndexedDBDatabase> db_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionRequest);
};

class IndexedDBDatabase::OpenRequest
    : public IndexedDBDatabase::ConnectionRequest {
 public:
  OpenRequest(scoped_refptr<IndexedDBDatabase> db,
              std::unique_ptr<IndexedDBPendingConnection> pending_connection)
      : ConnectionRequest(db), pending_(std::move(pending_connection)) {}

  void Perform() override {
    if (!pending_->callbacks->IsValid()) {
      db_->RequestComplete(this);
      return;
    }

    if (db_->metadata_.id == kInvalidId) {
      // The database was deleted then immediately re-opened; OpenInternal()
      // recreates it in the backing store.
      if (!db_->OpenInternal().ok()) {
        // TODO(jsbell): Consider including sanitized leveldb status message.
        base::string16 message;
        if (pending_->version == IndexedDBDatabaseMetadata::NO_VERSION) {
          message = ASCIIToUTF16(
              "Internal error opening database with no version specified.");
        } else {
          message =
              ASCIIToUTF16("Internal error opening database with version ") +
              Int64ToString16(pending_->version);
        }
        pending_->callbacks->OnError(IndexedDBDatabaseError(
            blink::WebIDBDatabaseExceptionUnknownError, message));
        db_->RequestComplete(this);
        return;
      }

      DCHECK_EQ(IndexedDBDatabaseMetadata::NO_VERSION, db_->metadata_.version);
    }

    const int64_t old_version = db_->metadata_.version;
    int64_t& new_version = pending_->version;

    bool is_new_database = old_version == IndexedDBDatabaseMetadata::NO_VERSION;

    if (new_version == IndexedDBDatabaseMetadata::DEFAULT_VERSION) {
      // For unit tests only - skip upgrade steps. (Calling from script with
      // DEFAULT_VERSION throws exception.)
      DCHECK(is_new_database);
      pending_->callbacks->OnSuccess(
          db_->CreateConnection(pending_->database_callbacks,
                                pending_->child_process_id),
          db_->metadata_);
      db_->RequestComplete(this);
      return;
    }

    if (!is_new_database &&
        (new_version == old_version ||
         new_version == IndexedDBDatabaseMetadata::NO_VERSION)) {
      pending_->callbacks->OnSuccess(
          db_->CreateConnection(pending_->database_callbacks,
                                pending_->child_process_id),
          db_->metadata_);
      db_->RequestComplete(this);
      return;
    }

    if (new_version == IndexedDBDatabaseMetadata::NO_VERSION) {
      // If no version is specified and no database exists, upgrade the
      // database version to 1.
      DCHECK(is_new_database);
      new_version = 1;
    } else if (new_version < old_version) {
      // Requested version is lower than current version - fail the request.
      DCHECK(!is_new_database);
      pending_->callbacks->OnError(IndexedDBDatabaseError(
          blink::WebIDBDatabaseExceptionVersionError,
          ASCIIToUTF16("The requested version (") +
              Int64ToString16(pending_->version) +
              ASCIIToUTF16(") is less than the existing version (") +
              Int64ToString16(db_->metadata_.version) + ASCIIToUTF16(").")));
      db_->RequestComplete(this);
      return;
    }

    // Requested version is higher than current version - upgrade needed.
    DCHECK_GT(new_version, old_version);

    if (db_->connections_.empty()) {
      StartUpgrade();
      return;
    }

    // There are outstanding connections - fire "versionchange" events and
    // wait for the connections to close. Front end ensures the event is not
    // fired at connections that have close_pending set. A "blocked" event
    // will be fired at the request when one of the connections acks that the
    // "versionchange" event was ignored.
    DCHECK_NE(pending_->data_loss_info.status, blink::WebIDBDataLossTotal);
    for (const auto* connection : db_->connections_)
      connection->callbacks()->OnVersionChange(old_version, new_version);

    // When all connections have closed the upgrade can proceed.
  }

  void OnVersionChangeIgnored() const override {
    pending_->callbacks->OnBlocked(db_->metadata_.version);
  }

  void OnConnectionClosed(IndexedDBConnection* connection) override {
    // This connection closed prematurely; signal an error and complete.
    if (connection && connection->callbacks() == pending_->database_callbacks) {
      pending_->callbacks->OnError(
          IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionAbortError,
                                 "The connection was closed."));
      db_->RequestComplete(this);
      return;
    }

    if (!db_->connections_.empty())
      return;

    StartUpgrade();
  }

  // Initiate the upgrade. The bulk of the work actually happens in
  // IndexedDBDatabase::VersionChangeOperation in order to kick the
  // transaction into the correct state.
  void StartUpgrade() {
    connection_ = db_->CreateConnection(pending_->database_callbacks,
                                        pending_->child_process_id);
    DCHECK_EQ(db_->connections_.count(connection_.get()), 1UL);

    std::vector<int64_t> object_store_ids;
    IndexedDBTransaction* transaction = db_->CreateTransaction(
        pending_->transaction_id, connection_.get(), object_store_ids,
        blink::WebIDBTransactionModeVersionChange);

    DCHECK(db_->transaction_coordinator_.IsRunningVersionChangeTransaction());
    transaction->ScheduleTask(
        base::Bind(&IndexedDBDatabase::VersionChangeOperation, db_,
                   pending_->version, pending_->callbacks));
  }

  // Called when the upgrade transaction has started executing.
  void UpgradeTransactionStarted(int64_t old_version) override {
    DCHECK(connection_);
    pending_->callbacks->OnUpgradeNeeded(old_version, std::move(connection_),
                                         db_->metadata_,
                                         pending_->data_loss_info);
  }

  void UpgradeTransactionFinished(bool committed) override {
    // Ownership of connection was already passed along in OnUpgradeNeeded.
    DCHECK(!connection_);

    if (committed) {
      DCHECK_EQ(pending_->version, db_->metadata_.version);
      pending_->callbacks->OnSuccess(std::unique_ptr<IndexedDBConnection>(),
                                     db_->metadata());
    } else {
      DCHECK_NE(pending_->version, db_->metadata_.version);
      pending_->callbacks->OnError(
          IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionAbortError,
                                 "Version change transaction was aborted in "
                                 "upgradeneeded event handler."));
    }
    db_->RequestComplete(this);
  }

 private:
  std::unique_ptr<IndexedDBPendingConnection> pending_;

  // If an upgrade is needed, holds the pending connection until ownership is
  // transferred to the IndexedDBDispatcherHost via OnUpgradeNeeded.
  std::unique_ptr<IndexedDBConnection> connection_;

  DISALLOW_COPY_AND_ASSIGN(OpenRequest);
};

class IndexedDBDatabase::DeleteRequest
    : public IndexedDBDatabase::ConnectionRequest {
 public:
  DeleteRequest(scoped_refptr<IndexedDBDatabase> db,
                scoped_refptr<IndexedDBCallbacks> callbacks)
      : ConnectionRequest(db), callbacks_(callbacks) {}

  void Perform() override {
    if (db_->connections_.empty()) {
      // No connections, so delete immediately.
      DoDelete();
      return;
    }

    // Front end ensures the event is not fired at connections that have
    // close_pending set.
    const int64_t old_version = db_->metadata_.version;
    const int64_t new_version = IndexedDBDatabaseMetadata::NO_VERSION;
    for (const auto* connection : db_->connections_)
      connection->callbacks()->OnVersionChange(old_version, new_version);
  }

  void OnVersionChangeIgnored() const override {
    callbacks_->OnBlocked(db_->metadata_.version);
  }

  void OnConnectionClosed(IndexedDBConnection* connection) override {
    if (!db_->connections_.empty())
      return;
    DoDelete();
  }

  void DoDelete() {
    leveldb::Status s =
        db_->backing_store_->DeleteDatabase(db_->metadata_.name);
    if (!s.ok()) {
      // TODO(jsbell): Consider including sanitized leveldb status message.
      IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                   "Internal error deleting database.");
      callbacks_->OnError(error);
      if (s.IsCorruption()) {
        url::Origin origin = db_->backing_store_->origin();
        db_->backing_store_ = nullptr;
        db_->factory_->HandleBackingStoreCorruption(origin, error);
      }
      db_->RequestComplete(this);
      return;
    }

    int64_t old_version = db_->metadata_.version;
    db_->metadata_.id = kInvalidId;
    db_->metadata_.version = IndexedDBDatabaseMetadata::NO_VERSION;
    db_->metadata_.max_object_store_id = kInvalidId;
    db_->metadata_.object_stores.clear();
    callbacks_->OnSuccess(old_version);
    db_->factory_->DatabaseDeleted(db_->identifier_);

    db_->RequestComplete(this);
  }

  void UpgradeTransactionStarted(int64_t old_version) override { NOTREACHED(); }

  void UpgradeTransactionFinished(bool committed) override { NOTREACHED(); }

 private:
  scoped_refptr<IndexedDBCallbacks> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(DeleteRequest);
};

scoped_refptr<IndexedDBDatabase> IndexedDBDatabase::Create(
    const base::string16& name,
    IndexedDBBackingStore* backing_store,
    IndexedDBFactory* factory,
    const Identifier& unique_identifier,
    leveldb::Status* s) {
  scoped_refptr<IndexedDBDatabase> database =
      IndexedDBClassFactory::Get()->CreateIndexedDBDatabase(
          name, backing_store, factory, unique_identifier);
  *s = database->OpenInternal();
  if (s->ok())
    return database;
  else
    return NULL;
}

IndexedDBDatabase::IndexedDBDatabase(const base::string16& name,
                                     IndexedDBBackingStore* backing_store,
                                     IndexedDBFactory* factory,
                                     const Identifier& unique_identifier)
    : backing_store_(backing_store),
      metadata_(name,
                kInvalidId,
                IndexedDBDatabaseMetadata::NO_VERSION,
                kInvalidId),
      identifier_(unique_identifier),
      factory_(factory) {
  DCHECK(factory != NULL);
}

void IndexedDBDatabase::AddObjectStore(
    const IndexedDBObjectStoreMetadata& object_store,
    int64_t new_max_object_store_id) {
  DCHECK(metadata_.object_stores.find(object_store.id) ==
         metadata_.object_stores.end());
  if (new_max_object_store_id != IndexedDBObjectStoreMetadata::kInvalidId) {
    DCHECK_LT(metadata_.max_object_store_id, new_max_object_store_id);
    metadata_.max_object_store_id = new_max_object_store_id;
  }
  metadata_.object_stores[object_store.id] = object_store;
}

void IndexedDBDatabase::RemoveObjectStore(int64_t object_store_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  metadata_.object_stores.erase(object_store_id);
}

void IndexedDBDatabase::SetObjectStoreName(
    int64_t object_store_id, const base::string16& name) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  metadata_.object_stores[object_store_id].name = name;
}

void IndexedDBDatabase::AddIndex(int64_t object_store_id,
                                 const IndexedDBIndexMetadata& index,
                                 int64_t new_max_index_id) {
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

void IndexedDBDatabase::RemoveIndex(int64_t object_store_id, int64_t index_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index_id) != object_store.indexes.end());
  object_store.indexes.erase(index_id);
  metadata_.object_stores[object_store_id] = object_store;
}

void IndexedDBDatabase::SetIndexName(
    int64_t object_store_id, int64_t index_id, const base::string16& name) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index_id) != object_store.indexes.end());
  object_store.indexes[index_id].name = name;
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
      metadata_.name, metadata_.version, &metadata_.id);
}

IndexedDBDatabase::~IndexedDBDatabase() {
  DCHECK(transactions_.empty());
  DCHECK(!active_request_);
  DCHECK(pending_requests_.empty());
}

size_t IndexedDBDatabase::GetMaxMessageSizeInBytes() const {
  return kMaxIDBMessageSizeInBytes;
}

std::unique_ptr<IndexedDBConnection> IndexedDBDatabase::CreateConnection(
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    int child_process_id) {
  std::unique_ptr<IndexedDBConnection> connection(
      base::MakeUnique<IndexedDBConnection>(this, database_callbacks));
  connections_.insert(connection.get());
  backing_store_->GrantChildProcessPermissions(child_process_id);
  return connection;
}

IndexedDBTransaction* IndexedDBDatabase::GetTransaction(
    int64_t transaction_id) const {
  const auto& trans_iterator = transactions_.find(transaction_id);
  if (trans_iterator == transactions_.end())
    return NULL;
  return trans_iterator->second;
}

bool IndexedDBDatabase::ValidateObjectStoreId(int64_t object_store_id) const {
  if (!base::ContainsKey(metadata_.object_stores, object_store_id)) {
    DLOG(ERROR) << "Invalid object_store_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::ValidateObjectStoreIdAndIndexId(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!ValidateObjectStoreId(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (!base::ContainsKey(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::ValidateObjectStoreIdAndOptionalIndexId(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!ValidateObjectStoreId(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (index_id != IndexedDBIndexMetadata::kInvalidId &&
      !base::ContainsKey(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::ValidateObjectStoreIdAndNewIndexId(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!ValidateObjectStoreId(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (base::ContainsKey(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

void IndexedDBDatabase::CreateObjectStore(int64_t transaction_id,
                                          int64_t object_store_id,
                                          const base::string16& name,
                                          const IndexedDBKeyPath& key_path,
                                          bool auto_increment) {
  IDB_TRACE1("IndexedDBDatabase::CreateObjectStore", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), blink::WebIDBTransactionModeVersionChange);

  if (base::ContainsKey(metadata_.object_stores, object_store_id)) {
    DLOG(ERROR) << "Invalid object_store_id";
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("WebCore.IndexedDB.Schema.ObjectStore.KeyPathType",
                            HistogramKeyPathType(key_path), KEY_PATH_TYPE_MAX);
  UMA_HISTOGRAM_BOOLEAN("WebCore.IndexedDB.Schema.ObjectStore.AutoIncrement",
                        auto_increment);

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
    if (s.IsCorruption())
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
    return;
  }

  AddObjectStore(object_store_metadata, object_store_id);
  transaction->ScheduleAbortTask(
      base::Bind(&IndexedDBDatabase::CreateObjectStoreAbortOperation,
                 this,
                 object_store_id));
}

void IndexedDBDatabase::DeleteObjectStore(int64_t transaction_id,
                                          int64_t object_store_id) {
  IDB_TRACE1("IndexedDBDatabase::DeleteObjectStore", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), blink::WebIDBTransactionModeVersionChange);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  transaction->ScheduleTask(
      base::Bind(&IndexedDBDatabase::DeleteObjectStoreOperation,
                 this,
                 object_store_id));
}

void IndexedDBDatabase::RenameObjectStore(int64_t transaction_id,
                                          int64_t object_store_id,
                                          const base::string16& new_name) {
  IDB_TRACE1("IndexedDBDatabase::RenameObjectStore", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), blink::WebIDBTransactionModeVersionChange);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  // Store renaming is done synchronously, as it may be followed by
  // index creation (also sync) since preemptive OpenCursor/SetIndexKeys
  // may follow.
  const IndexedDBObjectStoreMetadata object_store_metadata =
      metadata_.object_stores[object_store_id];

  leveldb::Status s =
      backing_store_->RenameObjectStore(transaction->BackingStoreTransaction(),
                                        transaction->database()->id(),
                                        object_store_metadata.id, new_name);
  if (!s.ok()) {
    IndexedDBDatabaseError error(
        blink::WebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16("Internal error renaming object store '") +
            object_store_metadata.name + ASCIIToUTF16("' to '") + new_name +
            ASCIIToUTF16("'."));
    transaction->Abort(error);
    if (s.IsCorruption())
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
    return;
  }

  transaction->ScheduleAbortTask(
      base::Bind(&IndexedDBDatabase::RenameObjectStoreAbortOperation,
                 this,
                 object_store_id,
                 object_store_metadata.name));
  SetObjectStoreName(object_store_id, new_name);
}

void IndexedDBDatabase::CreateIndex(int64_t transaction_id,
                                    int64_t object_store_id,
                                    int64_t index_id,
                                    const base::string16& name,
                                    const IndexedDBKeyPath& key_path,
                                    bool unique,
                                    bool multi_entry) {
  IDB_TRACE1("IndexedDBDatabase::CreateIndex", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), blink::WebIDBTransactionModeVersionChange);

  if (!ValidateObjectStoreIdAndNewIndexId(object_store_id, index_id))
    return;

  UMA_HISTOGRAM_ENUMERATION("WebCore.IndexedDB.Schema.Index.KeyPathType",
                            HistogramKeyPathType(key_path), KEY_PATH_TYPE_MAX);
  UMA_HISTOGRAM_BOOLEAN("WebCore.IndexedDB.Schema.Index.Unique", unique);
  UMA_HISTOGRAM_BOOLEAN("WebCore.IndexedDB.Schema.Index.MultiEntry",
                        multi_entry);

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
    int64_t object_store_id,
    int64_t index_id,
    IndexedDBTransaction* transaction) {
  DCHECK(!transaction);
  IDB_TRACE("IndexedDBDatabase::CreateIndexAbortOperation");
  RemoveIndex(object_store_id, index_id);
}

void IndexedDBDatabase::DeleteIndex(int64_t transaction_id,
                                    int64_t object_store_id,
                                    int64_t index_id) {
  IDB_TRACE1("IndexedDBDatabase::DeleteIndex", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), blink::WebIDBTransactionModeVersionChange);

  if (!ValidateObjectStoreIdAndIndexId(object_store_id, index_id))
    return;

  transaction->ScheduleTask(
      base::Bind(&IndexedDBDatabase::DeleteIndexOperation,
                 this,
                 object_store_id,
                 index_id));
}

void IndexedDBDatabase::DeleteIndexOperation(
    int64_t object_store_id,
    int64_t index_id,
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
    if (s.IsCorruption())
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
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
    int64_t object_store_id,
    const IndexedDBIndexMetadata& index_metadata,
    IndexedDBTransaction* transaction) {
  DCHECK(!transaction);
  IDB_TRACE("IndexedDBDatabase::DeleteIndexAbortOperation");
  AddIndex(object_store_id, index_metadata, IndexedDBIndexMetadata::kInvalidId);
}

void IndexedDBDatabase::RenameIndex(int64_t transaction_id,
                                    int64_t object_store_id,
                                    int64_t index_id,
                                    const base::string16& new_name) {
  IDB_TRACE1("IndexedDBDatabase::RenameIndex", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), blink::WebIDBTransactionModeVersionChange);

  if (!ValidateObjectStoreIdAndIndexId(object_store_id, index_id))
    return;

  // Index renaming is done synchronously since preemptive
  // OpenCursor/SetIndexKeys may follow.

  const IndexedDBIndexMetadata index_metadata =
      metadata_.object_stores[object_store_id].indexes[index_id];

  leveldb::Status s =
      backing_store_->RenameIndex(transaction->BackingStoreTransaction(),
                                  transaction->database()->id(),
                                  object_store_id,
                                  index_id,
                                  new_name);
  if (!s.ok()) {
    base::string16 error_string =
        ASCIIToUTF16("Internal error renaming index '") +
        index_metadata.name + ASCIIToUTF16("' to '") + new_name +
        ASCIIToUTF16("'.");
    transaction->Abort(IndexedDBDatabaseError(
        blink::WebIDBDatabaseExceptionUnknownError, error_string));
    return;
  }

  transaction->ScheduleAbortTask(
      base::Bind(&IndexedDBDatabase::RenameIndexAbortOperation,
                 this,
                 object_store_id,
                 index_id,
                 index_metadata.name));
  SetIndexName(object_store_id, index_id, new_name);
}

void IndexedDBDatabase::RenameIndexAbortOperation(
    int64_t object_store_id,
    int64_t index_id,
    const base::string16& old_name,
    IndexedDBTransaction* transaction) {
  DCHECK(!transaction);
  IDB_TRACE("IndexedDBDatabase::RenameIndexAbortOperation");
  SetIndexName(object_store_id, index_id, old_name);
}

void IndexedDBDatabase::Commit(int64_t transaction_id) {
  // The frontend suggests that we commit, but we may have previously initiated
  // an abort, and so have disposed of the transaction. on_abort has already
  // been dispatched to the frontend, so it will find out about that
  // asynchronously.
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (transaction) {
    scoped_refptr<IndexedDBFactory> factory = factory_;
    leveldb::Status s = transaction->Commit();
    if (s.IsCorruption()) {
      IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                   "Internal error committing transaction.");
      factory->HandleBackingStoreCorruption(identifier_.first, error);
    }
  }
}

void IndexedDBDatabase::Abort(int64_t transaction_id) {
  // If the transaction is unknown, then it has already been aborted by the
  // backend before this call so it is safe to ignore it.
  IDB_TRACE1("IndexedDBDatabase::Abort", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (transaction)
    transaction->Abort();
}

void IndexedDBDatabase::Abort(int64_t transaction_id,
                              const IndexedDBDatabaseError& error) {
  IDB_TRACE1("IndexedDBDatabase::Abort(error)", "txn.id", transaction_id);
  // If the transaction is unknown, then it has already been aborted by the
  // backend before this call so it is safe to ignore it.
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (transaction)
    transaction->Abort(error);
}

void IndexedDBDatabase::AddPendingObserver(
    int64_t transaction_id,
    int32_t observer_id,
    const IndexedDBObserver::Options& options) {
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  transaction->AddPendingObserver(observer_id, options);
}

void IndexedDBDatabase::RemovePendingObservers(
    IndexedDBConnection* connection,
    const std::vector<int32_t>& pending_observer_ids) {
  for (const auto& it : transactions_) {
    // Avoid call to RemovePendingObservers for transactions on other
    // connections.
    if (it.second->connection() == connection)
      it.second->RemovePendingObservers(pending_observer_ids);
  }
}

// TODO(palakj): Augment the function with IDBValue later. Issue
// crbug.com/609934.
void IndexedDBDatabase::FilterObservation(IndexedDBTransaction* transaction,
                                          int64_t object_store_id,
                                          blink::WebIDBOperationType type,
                                          const IndexedDBKeyRange& key_range) {
  for (auto* connection : connections_) {
    bool recorded = false;
    for (const auto& observer : connection->active_observers()) {
      if (!observer->IsRecordingType(type) ||
          !observer->IsRecordingObjectStore(object_store_id))
        continue;
      if (!recorded) {
        if (type == blink::WebIDBClear) {
          transaction->AddObservation(
              connection->id(),
              base::MakeUnique<IndexedDBObservation>(object_store_id, type));
        } else {
          transaction->AddObservation(connection->id(),
                                      base::MakeUnique<IndexedDBObservation>(
                                          object_store_id, type, key_range));
        }
        recorded = true;
      }
      transaction->RecordObserverForLastObservation(connection->id(),
                                                    observer->id());
    }
  }
}

void IndexedDBDatabase::SendObservations(
    std::map<int32_t, std::unique_ptr<IndexedDBObserverChanges>> changes_map) {
  for (auto* conn : connections_) {
    auto it = changes_map.find(conn->id());
    if (it != changes_map.end())
      conn->callbacks()->OnDatabaseChange(std::move(it->second));
  }
}

void IndexedDBDatabase::GetAll(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               std::unique_ptr<IndexedDBKeyRange> key_range,
                               bool key_only,
                               int64_t max_count,
                               scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE1("IndexedDBDatabase::GetAll", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (!ValidateObjectStoreId(object_store_id))
    return;

  transaction->ScheduleTask(base::Bind(
      &IndexedDBDatabase::GetAllOperation, this, object_store_id, index_id,
      base::Passed(&key_range),
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      max_count, callbacks));
}

void IndexedDBDatabase::Get(int64_t transaction_id,
                            int64_t object_store_id,
                            int64_t index_id,
                            std::unique_ptr<IndexedDBKeyRange> key_range,
                            bool key_only,
                            scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE1("IndexedDBDatabase::Get", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (!ValidateObjectStoreIdAndOptionalIndexId(object_store_id, index_id))
    return;

  transaction->ScheduleTask(base::Bind(
      &IndexedDBDatabase::GetOperation, this, object_store_id, index_id,
      base::Passed(&key_range),
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      callbacks));
}

void IndexedDBDatabase::GetOperation(
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
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
  std::unique_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;
  if (key_range->IsOnlyKey()) {
    key = &key_range->lower();
  } else {
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // ObjectStore Retrieval Operation
      if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
        backing_store_cursor = backing_store_->OpenObjectStoreKeyCursor(
            transaction->BackingStoreTransaction(), id(), object_store_id,
            *key_range, blink::WebIDBCursorDirectionNext, &s);
      } else {
        backing_store_cursor = backing_store_->OpenObjectStoreCursor(
            transaction->BackingStoreTransaction(), id(), object_store_id,
            *key_range, blink::WebIDBCursorDirectionNext, &s);
      }
    } else if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      // Index Value Retrieval Operation
      backing_store_cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(),
          id(),
          object_store_id,
          index_id,
          *key_range,
          blink::WebIDBCursorDirectionNext,
          &s);
    } else {
      // Index Referenced Value Retrieval Operation
      backing_store_cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(),
          id(),
          object_store_id,
          index_id,
          *key_range,
          blink::WebIDBCursorDirectionNext,
          &s);
    }

    if (!s.ok()) {
      DLOG(ERROR) << "Unable to open cursor operation: " << s.ToString();
      IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                   "Internal error deleting data in range");
      if (s.IsCorruption()) {
        factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
      }
    }

    if (!backing_store_cursor) {
      callbacks->OnSuccess();
      return;
    }

    key = &backing_store_cursor->key();
  }

  std::unique_ptr<IndexedDBKey> primary_key;
  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    // Object Store Retrieval Operation
    IndexedDBReturnValue value;
    s = backing_store_->GetRecord(transaction->BackingStoreTransaction(),
                                  id(),
                                  object_store_id,
                                  *key,
                                  &value);
    if (!s.ok()) {
      IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                   "Internal error in GetRecord.");
      callbacks->OnError(error);

      if (s.IsCorruption())
        factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
      return;
    }

    if (value.empty()) {
      callbacks->OnSuccess();
      return;
    }

    if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      callbacks->OnSuccess(*key);
      return;
    }

    if (object_store_metadata.auto_increment &&
        !object_store_metadata.key_path.IsNull()) {
      value.primary_key = *key;
      value.key_path = object_store_metadata.key_path;
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
    if (s.IsCorruption())
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
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
  IndexedDBReturnValue value;
  s = backing_store_->GetRecord(transaction->BackingStoreTransaction(),
                                id(),
                                object_store_id,
                                *primary_key,
                                &value);
  if (!s.ok()) {
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error in GetRecord.");
    callbacks->OnError(error);
    if (s.IsCorruption())
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
    return;
  }

  if (value.empty()) {
    callbacks->OnSuccess();
    return;
  }
  if (object_store_metadata.auto_increment &&
      !object_store_metadata.key_path.IsNull()) {
    value.primary_key = *primary_key;
    value.key_path = object_store_metadata.key_path;
  }
  callbacks->OnSuccess(&value);
}

void IndexedDBDatabase::GetAllOperation(
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    indexed_db::CursorType cursor_type,
    int64_t max_count,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::GetAllOperation", "txn.id", transaction->id());

  DCHECK_GT(max_count, 0);

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  leveldb::Status s;

  std::unique_ptr<IndexedDBBackingStore::Cursor> cursor;

  if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
    // Retrieving keys
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // Object Store: Key Retrieval Operation
      cursor = backing_store_->OpenObjectStoreKeyCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          *key_range, blink::WebIDBCursorDirectionNext, &s);
    } else {
      // Index Value: (Primary Key) Retrieval Operation
      cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::WebIDBCursorDirectionNext, &s);
    }
  } else {
    // Retrieving values
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // Object Store: Value Retrieval Operation
      cursor = backing_store_->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          *key_range, blink::WebIDBCursorDirectionNext, &s);
    } else {
      // Object Store: Referenced Value Retrieval Operation
      cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::WebIDBCursorDirectionNext, &s);
    }
  }

  if (!s.ok()) {
    DLOG(ERROR) << "Unable to open cursor operation: " << s.ToString();
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error in GetAllOperation");
    callbacks->OnError(error);
    if (s.IsCorruption()) {
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
    }
    return;
  }

  std::vector<IndexedDBKey> found_keys;
  std::vector<IndexedDBReturnValue> found_values;
  if (!cursor) {
    // Doesn't matter if key or value array here - will be empty array when it
    // hits JavaScript.
    callbacks->OnSuccessArray(&found_values);
    return;
  }

  bool did_first_seek = false;
  bool generated_key = object_store_metadata.auto_increment &&
                       !object_store_metadata.key_path.IsNull();

  size_t response_size = kMaxIDBMessageOverhead;
  int64_t num_found_items = 0;
  while (num_found_items++ < max_count) {
    bool cursor_valid;
    if (did_first_seek) {
      cursor_valid = cursor->Continue(&s);
    } else {
      cursor_valid = cursor->FirstSeek(&s);
      did_first_seek = true;
    }
    if (!s.ok()) {
      IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                   "Internal error in GetAllOperation.");
      callbacks->OnError(error);
      if (s.IsCorruption())
        factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
      return;
    }

    if (!cursor_valid)
      break;

    IndexedDBReturnValue return_value;
    IndexedDBKey return_key;

    if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      return_key = cursor->primary_key();
    } else {
      // Retrieving values
      return_value.swap(*cursor->value());
      if (!return_value.empty() && generated_key) {
        return_value.primary_key = cursor->primary_key();
        return_value.key_path = object_store_metadata.key_path;
      }
    }

    if (cursor_type == indexed_db::CURSOR_KEY_ONLY)
      response_size += return_key.size_estimate();
    else
      response_size += return_value.SizeEstimate();
    if (response_size > GetMaxMessageSizeInBytes()) {
      callbacks->OnError(
          IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Maximum IPC message size exceeded."));
      return;
    }

    if (cursor_type == indexed_db::CURSOR_KEY_ONLY)
      found_keys.push_back(return_key);
    else
      found_values.push_back(return_value);
  }

  if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
    // IndexedDBKey already supports an array of values so we can leverage  this
    // to return an array of keys - no need to create our own array of keys.
    callbacks->OnSuccess(IndexedDBKey(found_keys));
  } else {
    callbacks->OnSuccessArray(&found_values);
  }
}

static std::unique_ptr<IndexedDBKey> GenerateKey(
    IndexedDBBackingStore* backing_store,
    IndexedDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id) {
  const int64_t max_generator_value =
      9007199254740992LL;  // Maximum integer storable as ECMAScript number.
  int64_t current_number;
  leveldb::Status s = backing_store->GetKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(),
      database_id,
      object_store_id,
      &current_number);
  if (!s.ok()) {
    LOG(ERROR) << "Failed to GetKeyGeneratorCurrentNumber";
    return base::MakeUnique<IndexedDBKey>();
  }
  if (current_number < 0 || current_number > max_generator_value)
    return base::MakeUnique<IndexedDBKey>();

  return base::MakeUnique<IndexedDBKey>(current_number, WebIDBKeyTypeNumber);
}

static leveldb::Status UpdateKeyGenerator(IndexedDBBackingStore* backing_store,
                                          IndexedDBTransaction* transaction,
                                          int64_t database_id,
                                          int64_t object_store_id,
                                          const IndexedDBKey& key,
                                          bool check_current) {
  DCHECK_EQ(WebIDBKeyTypeNumber, key.type());
  return backing_store->MaybeUpdateKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(), database_id, object_store_id,
      static_cast<int64_t>(floor(key.number())) + 1, check_current);
}

struct IndexedDBDatabase::PutOperationParams {
  PutOperationParams() {}
  int64_t object_store_id;
  IndexedDBValue value;
  std::vector<std::unique_ptr<storage::BlobDataHandle>> handles;
  std::unique_ptr<IndexedDBKey> key;
  blink::WebIDBPutMode put_mode;
  scoped_refptr<IndexedDBCallbacks> callbacks;
  std::vector<IndexedDBIndexKeys> index_keys;

 private:
  DISALLOW_COPY_AND_ASSIGN(PutOperationParams);
};

void IndexedDBDatabase::Put(
    int64_t transaction_id,
    int64_t object_store_id,
    IndexedDBValue* value,
    std::vector<std::unique_ptr<storage::BlobDataHandle>>* handles,
    std::unique_ptr<IndexedDBKey> key,
    blink::WebIDBPutMode put_mode,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const std::vector<IndexedDBIndexKeys>& index_keys) {
  IDB_TRACE1("IndexedDBDatabase::Put", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_NE(transaction->mode(), blink::WebIDBTransactionModeReadOnly);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  DCHECK(key);
  DCHECK(value);
  std::unique_ptr<PutOperationParams> params(
      base::MakeUnique<PutOperationParams>());
  params->object_store_id = object_store_id;
  params->value.swap(*value);
  params->handles.swap(*handles);
  params->key = std::move(key);
  params->put_mode = put_mode;
  params->callbacks = callbacks;
  params->index_keys = index_keys;
  transaction->ScheduleTask(base::Bind(
      &IndexedDBDatabase::PutOperation, this, base::Passed(&params)));
}

void IndexedDBDatabase::PutOperation(std::unique_ptr<PutOperationParams> params,
                                     IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::PutOperation", "txn.id", transaction->id());
  DCHECK_NE(transaction->mode(), blink::WebIDBTransactionModeReadOnly);
  bool key_was_generated = false;

  DCHECK(metadata_.object_stores.find(params->object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[params->object_store_id];
  DCHECK(object_store.auto_increment || params->key->IsValid());

  std::unique_ptr<IndexedDBKey> key;
  if (params->put_mode != blink::WebIDBPutModeCursorUpdate &&
      object_store.auto_increment && !params->key->IsValid()) {
    std::unique_ptr<IndexedDBKey> auto_inc_key = GenerateKey(
        backing_store_.get(), transaction, id(), params->object_store_id);
    key_was_generated = true;
    if (!auto_inc_key->IsValid()) {
      params->callbacks->OnError(
          IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionConstraintError,
                                 "Maximum key generator value reached."));
      return;
    }
    key = std::move(auto_inc_key);
  } else {
    key = std::move(params->key);
  }

  DCHECK(key->IsValid());

  IndexedDBBackingStore::RecordIdentifier record_identifier;
  if (params->put_mode == blink::WebIDBPutModeAddOnly) {
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
      if (s.IsCorruption())
        factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
      return;
    }
    if (found) {
      params->callbacks->OnError(
          IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionConstraintError,
                                 "Key already exists in the object store."));
      return;
    }
  }

  std::vector<std::unique_ptr<IndexWriter>> index_writers;
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
                                &params->value,
                                &params->handles,
                                &record_identifier);
  if (!s.ok()) {
    IndexedDBDatabaseError error(
        blink::WebIDBDatabaseExceptionUnknownError,
        "Internal error: backing store error performing put/add.");
    params->callbacks->OnError(error);
    if (s.IsCorruption())
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
    return;
  }
  {
    IDB_TRACE1("IndexedDBDatabase::PutOperation.UpdateIndexes", "txn.id",
               transaction->id());
    for (const auto& writer : index_writers) {
      writer->WriteIndexKeys(record_identifier, backing_store_.get(),
                             transaction->BackingStoreTransaction(), id(),
                             params->object_store_id);
    }
  }

  if (object_store.auto_increment &&
      params->put_mode != blink::WebIDBPutModeCursorUpdate &&
      key->type() == WebIDBKeyTypeNumber) {
    IDB_TRACE1("IndexedDBDatabase::PutOperation.AutoIncrement", "txn.id",
               transaction->id());
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
      if (s.IsCorruption())
        factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
      return;
    }
  }
  {
    IDB_TRACE1("IndexedDBDatabase::PutOperation.Callbacks", "txn.id",
               transaction->id());
    params->callbacks->OnSuccess(*key);
  }
  FilterObservation(transaction, params->object_store_id,
                    params->put_mode == blink::WebIDBPutModeAddOnly
                        ? blink::WebIDBAdd
                        : blink::WebIDBPut,
                    IndexedDBKeyRange(*key));
}

void IndexedDBDatabase::SetIndexKeys(
    int64_t transaction_id,
    int64_t object_store_id,
    std::unique_ptr<IndexedDBKey> primary_key,
    const std::vector<IndexedDBIndexKeys>& index_keys) {
  IDB_TRACE1("IndexedDBDatabase::SetIndexKeys", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), blink::WebIDBTransactionModeVersionChange);

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
    if (s.IsCorruption())
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
    return;
  }
  if (!found) {
    transaction->Abort(IndexedDBDatabaseError(
        blink::WebIDBDatabaseExceptionUnknownError,
        "Internal error setting index keys for object store."));
    return;
  }

  std::vector<std::unique_ptr<IndexWriter>> index_writers;
  base::string16 error_message;
  bool obeys_constraints = false;
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];
  bool backing_store_success = MakeIndexWriters(transaction,
                                                backing_store_.get(),
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

  for (const auto& writer : index_writers) {
    writer->WriteIndexKeys(record_identifier, backing_store_.get(),
                           transaction->BackingStoreTransaction(), id(),
                           object_store_id);
  }
}

void IndexedDBDatabase::SetIndexesReady(int64_t transaction_id,
                                        int64_t,
                                        const std::vector<int64_t>& index_ids) {
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_EQ(transaction->mode(), blink::WebIDBTransactionModeVersionChange);

  transaction->ScheduleTask(
      blink::WebIDBTaskTypePreemptive,
      base::Bind(&IndexedDBDatabase::SetIndexesReadyOperation,
                 this,
                 index_ids.size()));
}

void IndexedDBDatabase::SetIndexesReadyOperation(
    size_t index_count,
    IndexedDBTransaction* transaction) {
  for (size_t i = 0; i < index_count; ++i)
    transaction->DidCompletePreemptiveEvent();
}

struct IndexedDBDatabase::OpenCursorOperationParams {
  OpenCursorOperationParams() {}
  int64_t object_store_id;
  int64_t index_id;
  std::unique_ptr<IndexedDBKeyRange> key_range;
  blink::WebIDBCursorDirection direction;
  indexed_db::CursorType cursor_type;
  blink::WebIDBTaskType task_type;
  scoped_refptr<IndexedDBCallbacks> callbacks;

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenCursorOperationParams);
};

void IndexedDBDatabase::OpenCursor(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    blink::WebIDBCursorDirection direction,
    bool key_only,
    blink::WebIDBTaskType task_type,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE1("IndexedDBDatabase::OpenCursor", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (!ValidateObjectStoreIdAndOptionalIndexId(object_store_id, index_id))
    return;

  std::unique_ptr<OpenCursorOperationParams> params(
      base::MakeUnique<OpenCursorOperationParams>());
  params->object_store_id = object_store_id;
  params->index_id = index_id;
  params->key_range = std::move(key_range);
  params->direction = direction;
  params->cursor_type =
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE;
  params->task_type = task_type;
  params->callbacks = callbacks;
  transaction->ScheduleTask(base::Bind(
      &IndexedDBDatabase::OpenCursorOperation, this, base::Passed(&params)));
}

void IndexedDBDatabase::OpenCursorOperation(
    std::unique_ptr<OpenCursorOperationParams> params,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1(
      "IndexedDBDatabase::OpenCursorOperation", "txn.id", transaction->id());

  // The frontend has begun indexing, so this pauses the transaction
  // until the indexing is complete. This can't happen any earlier
  // because we don't want to switch to early mode in case multiple
  // indexes are being created in a row, with Put()'s in between.
  if (params->task_type == blink::WebIDBTaskTypePreemptive)
    transaction->AddPreemptiveEvent();

  leveldb::Status s;
  std::unique_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;
  if (params->index_id == IndexedDBIndexMetadata::kInvalidId) {
    if (params->cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      DCHECK_EQ(params->task_type, blink::WebIDBTaskTypeNormal);
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
    DCHECK_EQ(params->task_type, blink::WebIDBTaskTypeNormal);
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
    if (s.IsCorruption()) {
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
    }
  }

  if (!backing_store_cursor) {
    // Why is Success being called?
    params->callbacks->OnSuccess(nullptr);
    return;
  }

  scoped_refptr<IndexedDBCursor> cursor =
      new IndexedDBCursor(std::move(backing_store_cursor), params->cursor_type,
                          params->task_type, transaction);
  params->callbacks->OnSuccess(
      cursor, cursor->key(), cursor->primary_key(), cursor->Value());
}

void IndexedDBDatabase::Count(int64_t transaction_id,
                              int64_t object_store_id,
                              int64_t index_id,
                              std::unique_ptr<IndexedDBKeyRange> key_range,
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
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::CountOperation", "txn.id", transaction->id());
  uint32_t count = 0;
  std::unique_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;

  leveldb::Status s;
  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    backing_store_cursor = backing_store_->OpenObjectStoreKeyCursor(
        transaction->BackingStoreTransaction(),
        id(),
        object_store_id,
        *key_range,
        blink::WebIDBCursorDirectionNext,
        &s);
  } else {
    backing_store_cursor = backing_store_->OpenIndexKeyCursor(
        transaction->BackingStoreTransaction(),
        id(),
        object_store_id,
        index_id,
        *key_range,
        blink::WebIDBCursorDirectionNext,
        &s);
  }
  if (!s.ok()) {
    DLOG(ERROR) << "Unable perform count operation: " << s.ToString();
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error performing count operation");
    if (s.IsCorruption()) {
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
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
    int64_t transaction_id,
    int64_t object_store_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE1("IndexedDBDatabase::DeleteRange", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_NE(transaction->mode(), blink::WebIDBTransactionModeReadOnly);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  transaction->ScheduleTask(base::Bind(&IndexedDBDatabase::DeleteRangeOperation,
                                       this,
                                       object_store_id,
                                       base::Passed(&key_range),
                                       callbacks));
}

void IndexedDBDatabase::DeleteRangeOperation(
    int64_t object_store_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::DeleteRangeOperation", "txn.id",
             transaction->id());
  size_t delete_count = 0;
  leveldb::Status s =
      backing_store_->DeleteRange(transaction->BackingStoreTransaction(), id(),
                                  object_store_id, *key_range, &delete_count);
  if (!s.ok()) {
    base::string16 error_string =
        ASCIIToUTF16("Internal error deleting data in range");
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 error_string);
    transaction->Abort(error);
    if (s.IsCorruption()) {
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
    }
    return;
  }
  callbacks->OnSuccess();
  FilterObservation(transaction, object_store_id, blink::WebIDBDelete,
                    *key_range);
}

void IndexedDBDatabase::Clear(int64_t transaction_id,
                              int64_t object_store_id,
                              scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE1("IndexedDBDatabase::Clear", "txn.id", transaction_id);
  IndexedDBTransaction* transaction = GetTransaction(transaction_id);
  if (!transaction)
    return;
  DCHECK_NE(transaction->mode(), blink::WebIDBTransactionModeReadOnly);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  transaction->ScheduleTask(base::Bind(
      &IndexedDBDatabase::ClearOperation, this, object_store_id, callbacks));
}

void IndexedDBDatabase::ClearOperation(
    int64_t object_store_id,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::ClearOperation", "txn.id", transaction->id());
  leveldb::Status s = backing_store_->ClearObjectStore(
      transaction->BackingStoreTransaction(), id(), object_store_id);
  if (!s.ok()) {
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error clearing object store");
    callbacks->OnError(error);
    if (s.IsCorruption()) {
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
    }
    return;
  }
  callbacks->OnSuccess();

  FilterObservation(transaction, object_store_id, blink::WebIDBClear,
                    IndexedDBKeyRange());
}

void IndexedDBDatabase::DeleteObjectStoreOperation(
    int64_t object_store_id,
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
    if (s.IsCorruption())
      factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
    return;
  }

  RemoveObjectStore(object_store_id);
  transaction->ScheduleAbortTask(
      base::Bind(&IndexedDBDatabase::DeleteObjectStoreAbortOperation,
                 this,
                 object_store_metadata));
}

void IndexedDBDatabase::VersionChangeOperation(
    int64_t version,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1(
      "IndexedDBDatabase::VersionChangeOperation", "txn.id", transaction->id());
  int64_t old_version = metadata_.version;
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
      base::Bind(&IndexedDBDatabase::VersionChangeAbortOperation, this,
                 metadata_.version));
  metadata_.version = version;

  active_request_->UpgradeTransactionStarted(old_version);
}

void IndexedDBDatabase::TransactionFinished(IndexedDBTransaction* transaction,
                                            bool committed) {
  IDB_TRACE1("IndexedDBTransaction::TransactionFinished", "txn.id", id());
  DCHECK(transactions_.find(transaction->id()) != transactions_.end());
  DCHECK_EQ(transactions_[transaction->id()], transaction);
  transactions_.erase(transaction->id());

  // This may be an unrelated transaction finishing while waiting for
  // connections to close, or the actual upgrade transaction from an active
  // request. Notify the active request if it's the latter.
  if (active_request_ &&
      transaction->mode() == blink::WebIDBTransactionModeVersionChange) {
    active_request_->UpgradeTransactionFinished(committed);
  }
}

void IndexedDBDatabase::TransactionCommitFailed(const leveldb::Status& status) {
  if (status.IsCorruption()) {
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Error committing transaction");
    factory_->HandleBackingStoreCorruption(backing_store_->origin(), error);
  } else {
    factory_->HandleBackingStoreFailure(backing_store_->origin());
  }
}

void IndexedDBDatabase::AppendRequest(
    std::unique_ptr<ConnectionRequest> request) {
  pending_requests_.push(std::move(request));

  if (!active_request_)
    ProcessRequestQueue();
}

void IndexedDBDatabase::RequestComplete(ConnectionRequest* request) {
  DCHECK_EQ(request, active_request_.get());
  active_request_.reset();

  if (!pending_requests_.empty())
    ProcessRequestQueue();
}

void IndexedDBDatabase::ProcessRequestQueue() {
  // Don't run re-entrantly to avoid exploding call stacks for requests that
  // complete synchronously. The loop below will process requests until one is
  // blocked.
  if (processing_pending_requests_)
    return;

  DCHECK(!active_request_);
  DCHECK(!pending_requests_.empty());

  base::AutoReset<bool> processing(&processing_pending_requests_, true);
  do {
    active_request_ = std::move(pending_requests_.front());
    pending_requests_.pop();
    active_request_->Perform();
    // If the active request completed synchronously, keep going.
  } while (!active_request_ && !pending_requests_.empty());
}

IndexedDBTransaction* IndexedDBDatabase::CreateTransaction(
    int64_t transaction_id,
    IndexedDBConnection* connection,
    const std::vector<int64_t>& object_store_ids,
    blink::WebIDBTransactionMode mode) {
  IDB_TRACE1("IndexedDBDatabase::CreateTransaction", "txn.id", transaction_id);
  DCHECK(connections_.count(connection));
  DCHECK(transactions_.find(transaction_id) == transactions_.end());
  if (transactions_.find(transaction_id) != transactions_.end())
    return nullptr;

  UMA_HISTOGRAM_COUNTS_1000(
      "WebCore.IndexedDB.Database.OutstandingTransactionCount",
      transactions_.size());

  // The transaction will add itself to this database's coordinator, which
  // manages the lifetime of the object.
  IndexedDBTransaction* transaction =
      IndexedDBClassFactory::Get()->CreateIndexedDBTransaction(
          transaction_id, connection->GetWeakPtr(),
          std::set<int64_t>(object_store_ids.begin(), object_store_ids.end()),
          mode, new IndexedDBBackingStore::Transaction(backing_store_.get()));
  TransactionCreated(transaction);
  return transaction;
}

void IndexedDBDatabase::TransactionCreated(IndexedDBTransaction* transaction) {
  transactions_[transaction->id()] = transaction;
}

void IndexedDBDatabase::OpenConnection(
    std::unique_ptr<IndexedDBPendingConnection> connection) {
  AppendRequest(base::MakeUnique<OpenRequest>(this, std::move(connection)));
}

void IndexedDBDatabase::DeleteDatabase(
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  AppendRequest(base::MakeUnique<DeleteRequest>(this, callbacks));
}

void IndexedDBDatabase::ForceClose() {
  // IndexedDBConnection::ForceClose() may delete this database, so hold ref.
  scoped_refptr<IndexedDBDatabase> protect(this);
  auto it = connections_.begin();
  while (it != connections_.end()) {
    IndexedDBConnection* connection = *it++;
    connection->ForceClose();
  }
  DCHECK(connections_.empty());
}

void IndexedDBDatabase::VersionChangeIgnored() {
  if (active_request_)
    active_request_->OnVersionChangeIgnored();
}

void IndexedDBDatabase::Close(IndexedDBConnection* connection, bool forced) {
  DCHECK(connections_.count(connection));
  DCHECK(connection->IsConnected());
  DCHECK(connection->database() == this);

  IDB_TRACE("IndexedDBDatabase::Close");

  // Abort outstanding transactions from the closing connection. This can not
  // happen if the close is requested by the connection itself as the
  // front-end defers the close until all transactions are complete, but can
  // occur on process termination or forced close.
  {
    auto transactions(transactions_);
    for (const auto& it : transactions) {
      if (it.second->callbacks() == connection->callbacks())
        it.second->Abort(
            IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                                   "Connection is closing."));
    }
  }

  // Abort transactions before removing the connection; aborting may complete
  // an upgrade, and thus allow the next open/delete requests to proceed. The
  // new active_request_ should see the old connection count until explicitly
  // notified below.
  connections_.erase(connection);

  // Notify the active request, which may need to do cleanup or proceed with
  // the operation. This may trigger other work, such as more connections or
  // deletions, so |active_request_| itself may change.
  if (active_request_)
    active_request_->OnConnectionClosed(connection);

  // If there are no more connections (current, active, or pending), tell the
  // factory to clean us up.
  if (connections_.empty() && !active_request_ && pending_requests_.empty()) {
    DCHECK(transactions_.empty());
    backing_store_ = nullptr;
    factory_->ReleaseDatabase(identifier_, forced);
  }
}

void IndexedDBDatabase::CreateObjectStoreAbortOperation(
    int64_t object_store_id,
    IndexedDBTransaction* transaction) {
  DCHECK(!transaction);
  IDB_TRACE("IndexedDBDatabase::CreateObjectStoreAbortOperation");
  RemoveObjectStore(object_store_id);
}

void IndexedDBDatabase::DeleteObjectStoreAbortOperation(
    const IndexedDBObjectStoreMetadata& object_store_metadata,
    IndexedDBTransaction* transaction) {
  DCHECK(!transaction);
  IDB_TRACE("IndexedDBDatabase::DeleteObjectStoreAbortOperation");
  AddObjectStore(object_store_metadata,
                 IndexedDBObjectStoreMetadata::kInvalidId);
}

void IndexedDBDatabase::RenameObjectStoreAbortOperation(
    int64_t object_store_id,
    const base::string16& old_name,
    IndexedDBTransaction* transaction) {
  DCHECK(!transaction);
  IDB_TRACE("IndexedDBDatabase::RenameObjectStoreAbortOperation");
  SetObjectStoreName(object_store_id, old_name);
}

void IndexedDBDatabase::VersionChangeAbortOperation(
    int64_t previous_version,
    IndexedDBTransaction* transaction) {
  DCHECK(!transaction);
  IDB_TRACE("IndexedDBDatabase::VersionChangeAbortOperation");
  metadata_.version = previous_version;
}

}  // namespace content
