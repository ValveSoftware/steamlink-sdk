// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "url/origin.h"

namespace content {
class IndexedDBBlobInfo;
class IndexedDBConnection;
class IndexedDBCursor;
class IndexedDBDatabase;
class IndexedDBDatabaseCallbacks;
struct IndexedDBDatabaseMetadata;
struct IndexedDBReturnValue;
struct IndexedDBValue;

class CONTENT_EXPORT IndexedDBCallbacks
    : public base::RefCounted<IndexedDBCallbacks> {
 public:
  // Simple payload responses
  IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                     int32_t ipc_thread_id,
                     int32_t ipc_callbacks_id);

  // IndexedDBCursor responses
  IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                     int32_t ipc_thread_id,
                     int32_t ipc_callbacks_id,
                     int32_t ipc_cursor_id);

  // IndexedDBDatabase responses
  IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                     int32_t ipc_thread_id,
                     int32_t ipc_callbacks_id,
                     int32_t ipc_database_callbacks_id,
                     int64_t host_transaction_id,
                     const url::Origin& origin);

  virtual void OnError(const IndexedDBDatabaseError& error);

  // IndexedDBFactory::GetDatabaseNames
  virtual void OnSuccess(const std::vector<base::string16>& string);

  // IndexedDBFactory::Open / DeleteDatabase
  virtual void OnBlocked(int64_t existing_version);

  // IndexedDBFactory::Open
  virtual void OnDataLoss(blink::WebIDBDataLoss data_loss,
                          std::string data_loss_message);
  virtual void OnUpgradeNeeded(
      int64_t old_version,
      std::unique_ptr<IndexedDBConnection> connection,
      const content::IndexedDBDatabaseMetadata& metadata);
  virtual void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                         const content::IndexedDBDatabaseMetadata& metadata);

  // IndexedDBDatabase::OpenCursor
  virtual void OnSuccess(scoped_refptr<IndexedDBCursor> cursor,
                         const IndexedDBKey& key,
                         const IndexedDBKey& primary_key,
                         IndexedDBValue* value);

  // IndexedDBCursor::Continue / Advance
  virtual void OnSuccess(const IndexedDBKey& key,
                         const IndexedDBKey& primary_key,
                         IndexedDBValue* value);

  // IndexedDBCursor::PrefetchContinue
  virtual void OnSuccessWithPrefetch(
      const std::vector<IndexedDBKey>& keys,
      const std::vector<IndexedDBKey>& primary_keys,
      std::vector<IndexedDBValue>* values);

  // IndexedDBDatabase::Get
  // IndexedDBCursor::Advance
  virtual void OnSuccess(IndexedDBReturnValue* value);

  // IndexedDBDatabase::GetAll
  virtual void OnSuccessArray(std::vector<IndexedDBReturnValue>* values,
                              const IndexedDBKeyPath& key_path);

  // IndexedDBDatabase::Put / IndexedDBCursor::Update
  virtual void OnSuccess(const IndexedDBKey& key);

  // IndexedDBDatabase::Count
  // IndexedDBFactory::DeleteDatabase
  // IndexedDBDatabase::DeleteRange
  virtual void OnSuccess(int64_t value);

  // IndexedDBCursor::Continue / Advance (when complete)
  virtual void OnSuccess();

  blink::WebIDBDataLoss data_loss() const { return data_loss_; }

  void SetConnectionOpenStartTime(const base::TimeTicks& start_time);

 protected:
  virtual ~IndexedDBCallbacks();

 private:
  void RegisterBlobsAndSend(const std::vector<IndexedDBBlobInfo>& blob_info,
                            const base::Closure& callback);

  friend class base::RefCounted<IndexedDBCallbacks>;

  // Originally from IndexedDBCallbacks:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  int32_t ipc_callbacks_id_;
  int32_t ipc_thread_id_;

  // IndexedDBCursor callbacks ------------------------
  int32_t ipc_cursor_id_;

  // IndexedDBDatabase callbacks ------------------------
  int64_t host_transaction_id_;
  url::Origin origin_;
  int32_t ipc_database_id_;
  int32_t ipc_database_callbacks_id_;

  // Stored in OnDataLoss, merged with OnUpgradeNeeded response.
  blink::WebIDBDataLoss data_loss_;
  std::string data_loss_message_;

  // The "blocked" event should be sent at most once per request.
  bool sent_blocked_;
  base::TimeTicks connection_open_start_time_;
  DISALLOW_COPY_AND_ASSIGN(IndexedDBCallbacks);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_
