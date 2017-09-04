// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_observer.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "content/browser/indexed_db/list_set.h"
#include "content/common/indexed_db/indexed_db_metadata.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"

namespace url {
class Origin;
}

namespace content {

class IndexedDBConnection;
class IndexedDBDatabaseCallbacks;
class IndexedDBFactory;
class IndexedDBKey;
class IndexedDBKeyPath;
class IndexedDBKeyRange;
class IndexedDBObserverChanges;
class IndexedDBTransaction;
struct IndexedDBValue;

class CONTENT_EXPORT IndexedDBDatabase
    : NON_EXPORTED_BASE(public base::RefCounted<IndexedDBDatabase>) {
 public:
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
  void SetObjectStoreName(int64_t object_store_id, const base::string16& name);
  void AddIndex(int64_t object_store_id,
                const IndexedDBIndexMetadata& metadata,
                int64_t new_max_index_id);
  void RemoveIndex(int64_t object_store_id, int64_t index_id);
  void SetIndexName(int64_t object_store_id,
                    int64_t index_id,
                    const base::string16& name);

  void OpenConnection(std::unique_ptr<IndexedDBPendingConnection> connection);
  void DeleteDatabase(scoped_refptr<IndexedDBCallbacks> callbacks);
  const IndexedDBDatabaseMetadata& metadata() const { return metadata_; }

  void CreateObjectStore(int64_t transaction_id,
                         int64_t object_store_id,
                         const base::string16& name,
                         const IndexedDBKeyPath& key_path,
                         bool auto_increment);
  void DeleteObjectStore(int64_t transaction_id, int64_t object_store_id);
  void RenameObjectStore(int64_t transaction_id,
                         int64_t object_store_id,
                         const base::string16& new_name);

  // Returns a pointer to a newly created transaction. The object is owned
  // by |transaction_coordinator_|.
  IndexedDBTransaction* CreateTransaction(
      int64_t transaction_id,
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
  void RenameIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   const base::string16& new_name);

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

  void AddPendingObserver(int64_t transaction_id,
                          int32_t observer_id,
                          const IndexedDBObserver::Options& options);
  void RemovePendingObservers(IndexedDBConnection* connection,
                              const std::vector<int32_t>& pending_observer_ids);

  void FilterObservation(IndexedDBTransaction*,
                         int64_t object_store_id,
                         blink::WebIDBOperationType type,
                         const IndexedDBKeyRange& key_range);
  void SendObservations(
      std::map<int32_t, std::unique_ptr<IndexedDBObserverChanges>> change_map);

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
           std::vector<std::unique_ptr<storage::BlobDataHandle>>* handles,
           std::unique_ptr<IndexedDBKey> key,
           blink::WebIDBPutMode mode,
           scoped_refptr<IndexedDBCallbacks> callbacks,
           const std::vector<IndexedDBIndexKeys>& index_keys);
  void SetIndexKeys(int64_t transaction_id,
                    int64_t object_store_id,
                    std::unique_ptr<IndexedDBKey> primary_key,
                    const std::vector<IndexedDBIndexKeys>& index_keys);
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
  size_t ConnectionCount() const { return connections_.size(); }

  // Number of active open/delete calls (running or blocked on other
  // connections).
  size_t ActiveOpenDeleteCount() const { return active_request_ ? 1 : 0; }

  // Number of open/delete calls that are waiting their turn.
  size_t PendingOpenDeleteCount() const { return pending_requests_.size(); }

  // Asynchronous tasks scheduled within transactions:
  void CreateObjectStoreAbortOperation(int64_t object_store_id,
                                       IndexedDBTransaction* transaction);
  void DeleteObjectStoreOperation(int64_t object_store_id,
                                  IndexedDBTransaction* transaction);
  void DeleteObjectStoreAbortOperation(
      const IndexedDBObjectStoreMetadata& object_store_metadata,
      IndexedDBTransaction* transaction);
  void RenameObjectStoreAbortOperation(int64_t object_store_id,
                                       const base::string16& old_name,
                                       IndexedDBTransaction* transaction);
  void VersionChangeOperation(int64_t version,
                              scoped_refptr<IndexedDBCallbacks> callbacks,
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
  void RenameIndexAbortOperation(int64_t object_store_id,
                                 int64_t index_id,
                                 const base::string16& old_name,
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

  class ConnectionRequest;
  class OpenRequest;
  class DeleteRequest;

  leveldb::Status OpenInternal();

  // Called internally when an open or delete request comes in. Processes
  // the queue immediately if there are no other requests.
  void AppendRequest(std::unique_ptr<ConnectionRequest> request);

  // Called by requests when complete. The request will be freed, so the
  // request must do no other work after calling this. If there are pending
  // requests, the queue will be synchronously processed.
  void RequestComplete(ConnectionRequest* request);

  // Pop the first request from the queue and start it.
  void ProcessRequestQueue();

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

  std::map<int64_t, IndexedDBTransaction*> transactions_;

  list_set<IndexedDBConnection*> connections_;

  // This holds the first open or delete request that is currently being
  // processed. The request has already broadcast OnVersionChange if
  // necessary.
  std::unique_ptr<ConnectionRequest> active_request_;

  // This holds open or delete requests that are waiting for the active
  // request to be completed. The requests have not yet broadcast
  // OnVersionChange (if necessary).
  std::queue<std::unique_ptr<ConnectionRequest>> pending_requests_;

  // The |processing_pending_requests_| flag is set while ProcessRequestQueue()
  // is executing. It prevents rentrant calls if the active request completes
  // synchronously.
  bool processing_pending_requests_ = false;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
