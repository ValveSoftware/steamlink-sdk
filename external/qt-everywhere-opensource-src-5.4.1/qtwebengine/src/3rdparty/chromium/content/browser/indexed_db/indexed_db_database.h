// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "content/browser/indexed_db/list_set.h"
#include "url/gurl.h"

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
  enum TaskType {
    NORMAL_TASK = 0,
    PREEMPTIVE_TASK
  };

  enum PutMode {
    ADD_OR_UPDATE,
    ADD_ONLY,
    CURSOR_UPDATE
  };

  // An index and corresponding set of keys
  typedef std::pair<int64, std::vector<IndexedDBKey> > IndexKeys;

  // Identifier is pair of (origin url, database name).
  typedef std::pair<GURL, base::string16> Identifier;

  static const int64 kInvalidId = 0;
  static const int64 kMinimumIndexId = 30;

  static scoped_refptr<IndexedDBDatabase> Create(
      const base::string16& name,
      IndexedDBBackingStore* backing_store,
      IndexedDBFactory* factory,
      const Identifier& unique_identifier,
      leveldb::Status* s);

  const Identifier& identifier() const { return identifier_; }
  IndexedDBBackingStore* backing_store() { return backing_store_.get(); }

  int64 id() const { return metadata_.id; }
  const base::string16& name() const { return metadata_.name; }

  void AddObjectStore(const IndexedDBObjectStoreMetadata& metadata,
                      int64 new_max_object_store_id);
  void RemoveObjectStore(int64 object_store_id);
  void AddIndex(int64 object_store_id,
                const IndexedDBIndexMetadata& metadata,
                int64 new_max_index_id);
  void RemoveIndex(int64 object_store_id, int64 index_id);

  void OpenConnection(const IndexedDBPendingConnection& connection);
  void DeleteDatabase(scoped_refptr<IndexedDBCallbacks> callbacks);
  const IndexedDBDatabaseMetadata& metadata() const { return metadata_; }

  void CreateObjectStore(int64 transaction_id,
                         int64 object_store_id,
                         const base::string16& name,
                         const IndexedDBKeyPath& key_path,
                         bool auto_increment);
  void DeleteObjectStore(int64 transaction_id, int64 object_store_id);
  void CreateTransaction(int64 transaction_id,
                         IndexedDBConnection* connection,
                         const std::vector<int64>& object_store_ids,
                         uint16 mode);
  void Close(IndexedDBConnection* connection, bool forced);
  void ForceClose();

  void Commit(int64 transaction_id);
  void Abort(int64 transaction_id);
  void Abort(int64 transaction_id, const IndexedDBDatabaseError& error);

  void CreateIndex(int64 transaction_id,
                   int64 object_store_id,
                   int64 index_id,
                   const base::string16& name,
                   const IndexedDBKeyPath& key_path,
                   bool unique,
                   bool multi_entry);
  void DeleteIndex(int64 transaction_id, int64 object_store_id, int64 index_id);

  IndexedDBTransactionCoordinator& transaction_coordinator() {
    return transaction_coordinator_;
  }
  const IndexedDBTransactionCoordinator& transaction_coordinator() const {
    return transaction_coordinator_;
  }

  void TransactionCreated(IndexedDBTransaction* transaction);
  void TransactionFinished(IndexedDBTransaction* transaction, bool committed);

  // Called by transactions to report failure committing to the backing store.
  void TransactionCommitFailed();

  void Get(int64 transaction_id,
           int64 object_store_id,
           int64 index_id,
           scoped_ptr<IndexedDBKeyRange> key_range,
           bool key_only,
           scoped_refptr<IndexedDBCallbacks> callbacks);
  void Put(int64 transaction_id,
           int64 object_store_id,
           IndexedDBValue* value,
           ScopedVector<webkit_blob::BlobDataHandle>* handles,
           scoped_ptr<IndexedDBKey> key,
           PutMode mode,
           scoped_refptr<IndexedDBCallbacks> callbacks,
           const std::vector<IndexKeys>& index_keys);
  void SetIndexKeys(int64 transaction_id,
                    int64 object_store_id,
                    scoped_ptr<IndexedDBKey> primary_key,
                    const std::vector<IndexKeys>& index_keys);
  void SetIndexesReady(int64 transaction_id,
                       int64 object_store_id,
                       const std::vector<int64>& index_ids);
  void OpenCursor(int64 transaction_id,
                  int64 object_store_id,
                  int64 index_id,
                  scoped_ptr<IndexedDBKeyRange> key_range,
                  indexed_db::CursorDirection,
                  bool key_only,
                  TaskType task_type,
                  scoped_refptr<IndexedDBCallbacks> callbacks);
  void Count(int64 transaction_id,
             int64 object_store_id,
             int64 index_id,
             scoped_ptr<IndexedDBKeyRange> key_range,
             scoped_refptr<IndexedDBCallbacks> callbacks);
  void DeleteRange(int64 transaction_id,
                   int64 object_store_id,
                   scoped_ptr<IndexedDBKeyRange> key_range,
                   scoped_refptr<IndexedDBCallbacks> callbacks);
  void Clear(int64 transaction_id,
             int64 object_store_id,
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
  void CreateObjectStoreAbortOperation(int64 object_store_id,
                                       IndexedDBTransaction* transaction);
  void DeleteObjectStoreOperation(
      int64 object_store_id,
      IndexedDBTransaction* transaction);
  void DeleteObjectStoreAbortOperation(
      const IndexedDBObjectStoreMetadata& object_store_metadata,
      IndexedDBTransaction* transaction);
  void VersionChangeOperation(int64 version,
                              scoped_refptr<IndexedDBCallbacks> callbacks,
                              scoped_ptr<IndexedDBConnection> connection,
                              IndexedDBTransaction* transaction);
  void VersionChangeAbortOperation(const base::string16& previous_version,
                                   int64 previous_int_version,
                                   IndexedDBTransaction* transaction);
  void DeleteIndexOperation(int64 object_store_id,
                            int64 index_id,
                            IndexedDBTransaction* transaction);
  void CreateIndexAbortOperation(int64 object_store_id,
                                 int64 index_id,
                                 IndexedDBTransaction* transaction);
  void DeleteIndexAbortOperation(int64 object_store_id,
                                 const IndexedDBIndexMetadata& index_metadata,
                                 IndexedDBTransaction* transaction);
  void GetOperation(int64 object_store_id,
                    int64 index_id,
                    scoped_ptr<IndexedDBKeyRange> key_range,
                    indexed_db::CursorType cursor_type,
                    scoped_refptr<IndexedDBCallbacks> callbacks,
                    IndexedDBTransaction* transaction);
  struct PutOperationParams;
  void PutOperation(scoped_ptr<PutOperationParams> params,
                    IndexedDBTransaction* transaction);
  void SetIndexesReadyOperation(size_t index_count,
                                IndexedDBTransaction* transaction);
  struct OpenCursorOperationParams;
  void OpenCursorOperation(scoped_ptr<OpenCursorOperationParams> params,
                           IndexedDBTransaction* transaction);
  void CountOperation(int64 object_store_id,
                      int64 index_id,
                      scoped_ptr<IndexedDBKeyRange> key_range,
                      scoped_refptr<IndexedDBCallbacks> callbacks,
                      IndexedDBTransaction* transaction);
  void DeleteRangeOperation(int64 object_store_id,
                            scoped_ptr<IndexedDBKeyRange> key_range,
                            scoped_refptr<IndexedDBCallbacks> callbacks,
                            IndexedDBTransaction* transaction);
  void ClearOperation(int64 object_store_id,
                      scoped_refptr<IndexedDBCallbacks> callbacks,
                      IndexedDBTransaction* transaction);

 private:
  friend class base::RefCounted<IndexedDBDatabase>;

  IndexedDBDatabase(const base::string16& name,
                    IndexedDBBackingStore* backing_store,
                    IndexedDBFactory* factory,
                    const Identifier& unique_identifier);
  ~IndexedDBDatabase();

  bool IsOpenConnectionBlocked() const;
  leveldb::Status OpenInternal();
  void RunVersionChangeTransaction(scoped_refptr<IndexedDBCallbacks> callbacks,
                                   scoped_ptr<IndexedDBConnection> connection,
                                   int64 transaction_id,
                                   int64 requested_version);
  void RunVersionChangeTransactionFinal(
      scoped_refptr<IndexedDBCallbacks> callbacks,
      scoped_ptr<IndexedDBConnection> connection,
      int64 transaction_id,
      int64 requested_version);
  void ProcessPendingCalls();

  bool IsDeleteDatabaseBlocked() const;
  void DeleteDatabaseFinal(scoped_refptr<IndexedDBCallbacks> callbacks);

  scoped_ptr<IndexedDBConnection> CreateConnection(
      scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
      int child_process_id);

  IndexedDBTransaction* GetTransaction(int64 transaction_id) const;

  bool ValidateObjectStoreId(int64 object_store_id) const;
  bool ValidateObjectStoreIdAndIndexId(int64 object_store_id,
                                       int64 index_id) const;
  bool ValidateObjectStoreIdAndOptionalIndexId(int64 object_store_id,
                                               int64 index_id) const;
  bool ValidateObjectStoreIdAndNewIndexId(int64 object_store_id,
                                          int64 index_id) const;

  scoped_refptr<IndexedDBBackingStore> backing_store_;
  IndexedDBDatabaseMetadata metadata_;

  const Identifier identifier_;
  scoped_refptr<IndexedDBFactory> factory_;

  IndexedDBTransactionCoordinator transaction_coordinator_;

  typedef std::map<int64, IndexedDBTransaction*> TransactionMap;
  TransactionMap transactions_;

  typedef std::list<IndexedDBPendingConnection> PendingOpenCallList;
  PendingOpenCallList pending_open_calls_;

  class PendingUpgradeCall;
  scoped_ptr<PendingUpgradeCall> pending_run_version_change_transaction_call_;
  class PendingSuccessCall;
  scoped_ptr<PendingSuccessCall> pending_second_half_open_;

  class PendingDeleteCall;
  typedef std::list<PendingDeleteCall*> PendingDeleteCallList;
  PendingDeleteCallList pending_delete_calls_;

  typedef list_set<IndexedDBConnection*> ConnectionSet;
  ConnectionSet connections_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
