// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "content/browser/indexed_db/list_set.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"

namespace url {
class Origin;
}

namespace content {

class IndexedDBBlobInfo;
class IndexedDBConnection;
class IndexedDBDatabaseCallbacks;
class IndexedDBFactory;
class IndexedDBKey;
class IndexedDBKeyPath;
class IndexedDBKeyRange;
class IndexedDBTransaction;
struct IndexedDBValue;

class CONTENT_EXPORT IndexedDBDatabase
    : NON_EXPORTED_BASE(public base::RefCounted<IndexedDBDatabase>) {
 public:
  // An index and corresponding set of keys
  using IndexKeys = std::pair<int64_t, std::vector<IndexedDBKey>>;

  // Identifier is pair of (origin, database name).
  using Identifier = std::pair<url::Origin, base::string16>;

  static const int64_t kInvalidId = 0;
  static const int64_t kMinimumIndexId = 30;

  static scoped_refptr<IndexedDBDatabase> Create(
      const base::string16& name,
      IndexedDBBackingStore* backing_store,
      IndexedDBFactory* factory,
      const Identifier& unique_identifier,
      leveldb::Status* s);

  const Identifier& identifier() const { return identifier_; }
  IndexedDBBackingStore* backing_store() { return backing_store_.get(); }

  int64_t id() const { return metadata_.id; }
  const base::string16& name() const { return metadata_.name; }

  void AddObjectStore(const IndexedDBObjectStoreMetadata& metadata,
                      int64_t new_max_object_store_id);
  void RemoveObjectStore(int64_t object_store_id);
  void AddIndex(int64_t object_store_id,
                const IndexedDBIndexMetadata& metadata,
                int64_t new_max_index_id);
  void RemoveIndex(int64_t object_store_id, int64_t index_id);

  void OpenConnection(const IndexedDBPendingConnection& connection);
  void DeleteDatabase(scoped_refptr<IndexedDBCallbacks> callbacks);
  const IndexedDBDatabaseMetadata& metadata() const { return metadata_; }

  void CreateObjectStore(int64_t transaction_id,
                         int64_t object_store_id,
                         const base::string16& name,
                         const IndexedDBKeyPath& key_path,
                         bool auto_increment);
  void DeleteObjectStore(int64_t transaction_id, int64_t object_store_id);
  void CreateTransaction(int64_t transaction_id,
                         IndexedDBConnection* connection,
                         const std::vector<int64_t>& object_store_ids,
                         blink::WebIDBTransactionMode mode);
  void Close(IndexedDBConnection* connection, bool forced);
  void ForceClose();

  // Ack that one of the connections notified with a "versionchange" event did
  // not promptly close. Therefore a "blocked" event should be fired at the
  // pending connection.
  void VersionChangeIgnored();

  void Commit(int64_t transaction_id);
  void Abort(int64_t transaction_id);
  void Abort(int64_t transaction_id, const IndexedDBDatabaseError& error);

  void CreateIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   const base::string16& name,
                   const IndexedDBKeyPath& key_path,
                   bool unique,
                   bool multi_entry);
  void DeleteIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id);

  IndexedDBTransactionCoordinator& transaction_coordinator() {
    return transaction_coordinator_;
  }
  const IndexedDBTransactionCoordinator& transaction_coordinator() const {
    return transaction_coordinator_;
  }

  void TransactionCreated(IndexedDBTransaction* transaction);
  void TransactionFinished(IndexedDBTransaction* transaction, bool committed);

  // Called by transactions to report failure committing to the backing store.
  void TransactionCommitFailed(const leveldb::Status& status);

  void Get(int64_t transaction_id,
           int64_t object_store_id,
           int64_t index_id,
           std::unique_ptr<IndexedDBKeyRange> key_range,
           bool key_only,
           scoped_refptr<IndexedDBCallbacks> callbacks);
  void GetAll(int64_t transaction_id,
              int64_t object_store_id,
              int64_t index_id,
              std::unique_ptr<IndexedDBKeyRange> key_range,
              bool key_only,
              int64_t max_count,
              scoped_refptr<IndexedDBCallbacks> callbacks);
  void Put(int64_t transaction_id,
           int64_t object_store_id,
           IndexedDBValue* value,
           ScopedVector<storage::BlobDataHandle>* handles,
           std::unique_ptr<IndexedDBKey> key,
           blink::WebIDBPutMode mode,
           scoped_refptr<IndexedDBCallbacks> callbacks,
           const std::vector<IndexKeys>& index_keys);
  void SetIndexKeys(int64_t transaction_id,
                    int64_t object_store_id,
                    std::unique_ptr<IndexedDBKey> primary_key,
                    const std::vector<IndexKeys>& index_keys);
  void SetIndexesReady(int64_t transaction_id,
                       int64_t object_store_id,
                       const std::vector<int64_t>& index_ids);
  void OpenCursor(int64_t transaction_id,
                  int64_t object_store_id,
                  int64_t index_id,
                  std::unique_ptr<IndexedDBKeyRange> key_range,
                  blink::WebIDBCursorDirection,
                  bool key_only,
                  blink::WebIDBTaskType task_type,
                  scoped_refptr<IndexedDBCallbacks> callbacks);
  void Count(int64_t transaction_id,
             int64_t object_store_id,
             int64_t index_id,
             std::unique_ptr<IndexedDBKeyRange> key_range,
             scoped_refptr<IndexedDBCallbacks> callbacks);
  void DeleteRange(int64_t transaction_id,
                   int64_t object_store_id,
                   std::unique_ptr<IndexedDBKeyRange> key_range,
                   scoped_refptr<IndexedDBCallbacks> callbacks);
  void Clear(int64_t transaction_id,
             int64_t object_store_id,
             scoped_refptr<IndexedDBCallbacks> callbacks);

  // Number of connections that have progressed passed initial open call.
  size_t ConnectionCount() const;
  // Number of open calls that are blocked on other connections.
  size_t PendingOpenCount() const;
  // Number of pending upgrades (0 or 1). Also included in ConnectionCount().
  size_t PendingUpgradeCount() const;
  // Number of running upgrades (0 or 1). Also included in ConnectionCount().
  size_t RunningUpgradeCount() const;
  // Number of pending deletes, blocked on other connections.
  size_t PendingDeleteCount() const;

  // Asynchronous tasks scheduled within transactions:
  void CreateObjectStoreAbortOperation(int64_t object_store_id,
                                       IndexedDBTransaction* transaction);
  void DeleteObjectStoreOperation(int64_t object_store_id,
                                  IndexedDBTransaction* transaction);
  void DeleteObjectStoreAbortOperation(
      const IndexedDBObjectStoreMetadata& object_store_metadata,
      IndexedDBTransaction* transaction);
  void VersionChangeOperation(int64_t version,
                              scoped_refptr<IndexedDBCallbacks> callbacks,
                              std::unique_ptr<IndexedDBConnection> connection,
                              IndexedDBTransaction* transaction);
  void VersionChangeAbortOperation(int64_t previous_version,
                                   IndexedDBTransaction* transaction);
  void DeleteIndexOperation(int64_t object_store_id,
                            int64_t index_id,
                            IndexedDBTransaction* transaction);
  void CreateIndexAbortOperation(int64_t object_store_id,
                                 int64_t index_id,
                                 IndexedDBTransaction* transaction);
  void DeleteIndexAbortOperation(int64_t object_store_id,
                                 const IndexedDBIndexMetadata& index_metadata,
                                 IndexedDBTransaction* transaction);
  void GetOperation(int64_t object_store_id,
                    int64_t index_id,
                    std::unique_ptr<IndexedDBKeyRange> key_range,
                    indexed_db::CursorType cursor_type,
                    scoped_refptr<IndexedDBCallbacks> callbacks,
                    IndexedDBTransaction* transaction);
  void GetAllOperation(int64_t object_store_id,
                       int64_t index_id,
                       std::unique_ptr<IndexedDBKeyRange> key_range,
                       indexed_db::CursorType cursor_type,
                       int64_t max_count,
                       scoped_refptr<IndexedDBCallbacks> callbacks,
                       IndexedDBTransaction* transaction);
  struct PutOperationParams;
  void PutOperation(std::unique_ptr<PutOperationParams> params,
                    IndexedDBTransaction* transaction);
  void SetIndexesReadyOperation(size_t index_count,
                                IndexedDBTransaction* transaction);
  struct OpenCursorOperationParams;
  void OpenCursorOperation(std::unique_ptr<OpenCursorOperationParams> params,
                           IndexedDBTransaction* transaction);
  void CountOperation(int64_t object_store_id,
                      int64_t index_id,
                      std::unique_ptr<IndexedDBKeyRange> key_range,
                      scoped_refptr<IndexedDBCallbacks> callbacks,
                      IndexedDBTransaction* transaction);
  void DeleteRangeOperation(int64_t object_store_id,
                            std::unique_ptr<IndexedDBKeyRange> key_range,
                            scoped_refptr<IndexedDBCallbacks> callbacks,
                            IndexedDBTransaction* transaction);
  void ClearOperation(int64_t object_store_id,
                      scoped_refptr<IndexedDBCallbacks> callbacks,
                      IndexedDBTransaction* transaction);

 protected:
  IndexedDBDatabase(const base::string16& name,
                    IndexedDBBackingStore* backing_store,
                    IndexedDBFactory* factory,
                    const Identifier& unique_identifier);
  virtual ~IndexedDBDatabase();

  // May be overridden in tests.
  virtual size_t GetMaxMessageSizeInBytes() const;

 private:
  friend class base::RefCounted<IndexedDBDatabase>;
  friend class IndexedDBClassFactory;

  class PendingDeleteCall;
  class PendingSuccessCall;
  class PendingUpgradeCall;

  typedef std::map<int64_t, IndexedDBTransaction*> TransactionMap;
  typedef list_set<IndexedDBConnection*> ConnectionSet;

  bool IsOpenConnectionBlocked() const;
  leveldb::Status OpenInternal();
  void RunVersionChangeTransaction(
      scoped_refptr<IndexedDBCallbacks> callbacks,
      std::unique_ptr<IndexedDBConnection> connection,
      int64_t transaction_id,
      int64_t requested_version);
  void RunVersionChangeTransactionFinal(
      scoped_refptr<IndexedDBCallbacks> callbacks,
      std::unique_ptr<IndexedDBConnection> connection,
      int64_t transaction_id,
      int64_t requested_version);
  void ProcessPendingCalls();

  bool IsDeleteDatabaseBlocked() const;
  void DeleteDatabaseFinal(scoped_refptr<IndexedDBCallbacks> callbacks);

  std::unique_ptr<IndexedDBConnection> CreateConnection(
      scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
      int child_process_id);

  IndexedDBTransaction* GetTransaction(int64_t transaction_id) const;

  bool ValidateObjectStoreId(int64_t object_store_id) const;
  bool ValidateObjectStoreIdAndIndexId(int64_t object_store_id,
                                       int64_t index_id) const;
  bool ValidateObjectStoreIdAndOptionalIndexId(int64_t object_store_id,
                                               int64_t index_id) const;
  bool ValidateObjectStoreIdAndNewIndexId(int64_t object_store_id,
                                          int64_t index_id) const;

  scoped_refptr<IndexedDBBackingStore> backing_store_;
  IndexedDBDatabaseMetadata metadata_;

  const Identifier identifier_;
  scoped_refptr<IndexedDBFactory> factory_;

  IndexedDBTransactionCoordinator transaction_coordinator_;

  TransactionMap transactions_;
  std::queue<IndexedDBPendingConnection> pending_open_calls_;
  std::unique_ptr<PendingUpgradeCall>
      pending_run_version_change_transaction_call_;
  std::unique_ptr<PendingSuccessCall> pending_second_half_open_;
  std::list<PendingDeleteCall*> pending_delete_calls_;

  ConnectionSet connections_;
  bool experimental_web_platform_features_enabled_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
