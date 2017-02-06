// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_INDEXED_DB_DISPATCHER_H_
#define CONTENT_CHILD_INDEXED_DB_INDEXED_DB_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/nullable_string16.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/public/child/worker_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/public/platform/WebBlobInfo.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"
#include "url/origin.h"

struct IndexedDBDatabaseMetadata;
struct IndexedDBMsg_CallbacksSuccessCursorContinue_Params;
struct IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params;
struct IndexedDBMsg_CallbacksSuccessIDBCursor_Params;
struct IndexedDBMsg_CallbacksSuccessArray_Params;
struct IndexedDBMsg_CallbacksSuccessValue_Params;
struct IndexedDBMsg_CallbacksUpgradeNeeded_Params;

namespace blink {
class WebData;
}

namespace content {
class IndexedDBKey;
class IndexedDBKeyPath;
class IndexedDBKeyRange;
class WebIDBCursorImpl;
class WebIDBDatabaseImpl;
class ThreadSafeSender;

// Handle the indexed db related communication for this context thread - the
// main thread and each worker thread have their own copies.
class CONTENT_EXPORT IndexedDBDispatcher : public WorkerThread::Observer {
 public:
  // Constructor made public to allow RenderThreadImpl to own a copy without
  // failing a NOTREACHED in ThreadSpecificInstance in tests that instantiate
  // two copies of RenderThreadImpl on the same thread.  Everyone else probably
  // wants to use ThreadSpecificInstance().
  explicit IndexedDBDispatcher(ThreadSafeSender* thread_safe_sender);
  ~IndexedDBDispatcher() override;

  // |thread_safe_sender| needs to be passed in because if the call leads to
  // construction it will be needed.
  static IndexedDBDispatcher* ThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  static blink::WebIDBMetadata ConvertMetadata(
      const IndexedDBDatabaseMetadata& idb_metadata);

  void OnMessageReceived(const IPC::Message& msg);

  // This method is virtual so it can be overridden in unit tests.
  virtual bool Send(IPC::Message* msg);

  void RequestIDBFactoryGetDatabaseNames(blink::WebIDBCallbacks* callbacks,
                                         const url::Origin& origin);

  void RequestIDBFactoryOpen(const base::string16& name,
                             int64_t version,
                             int64_t transaction_id,
                             blink::WebIDBCallbacks* callbacks,
                             blink::WebIDBDatabaseCallbacks* database_callbacks,
                             const url::Origin& origin);

  void RequestIDBFactoryDeleteDatabase(const base::string16& name,
                                       blink::WebIDBCallbacks* callbacks,
                                       const url::Origin& origin);

  // This method is virtual so it can be overridden in unit tests.
  virtual void RequestIDBCursorAdvance(unsigned long count,
                                       blink::WebIDBCallbacks* callbacks_ptr,
                                       int32_t ipc_cursor_id,
                                       int64_t transaction_id);

  // This method is virtual so it can be overridden in unit tests.
  virtual void RequestIDBCursorContinue(const IndexedDBKey& key,
                                        const IndexedDBKey& primary_key,
                                        blink::WebIDBCallbacks* callbacks_ptr,
                                        int32_t ipc_cursor_id,
                                        int64_t transaction_id);

  // This method is virtual so it can be overridden in unit tests.
  virtual void RequestIDBCursorPrefetch(int n,
                                        blink::WebIDBCallbacks* callbacks_ptr,
                                        int32_t ipc_cursor_id);

  // This method is virtual so it can be overridden in unit tests.
  virtual void RequestIDBCursorPrefetchReset(int used_prefetches,
                                             int unused_prefetches,
                                             int32_t ipc_cursor_id);

  void RequestIDBDatabaseClose(int32_t ipc_database_id,
                               int32_t ipc_database_callbacks_id);

  void NotifyIDBDatabaseVersionChangeIgnored(int32_t ipc_database_id);

  void RequestIDBDatabaseCreateTransaction(
      int32_t ipc_database_id,
      int64_t transaction_id,
      blink::WebIDBDatabaseCallbacks* database_callbacks_ptr,
      blink::WebVector<long long> object_store_ids,
      blink::WebIDBTransactionMode mode);

  void RequestIDBDatabaseGet(int32_t ipc_database_id,
                             int64_t transaction_id,
                             int64_t object_store_id,
                             int64_t index_id,
                             const IndexedDBKeyRange& key_range,
                             bool key_only,
                             blink::WebIDBCallbacks* callbacks);

  void RequestIDBDatabaseGetAll(int32_t ipc_database_id,
                                int64_t transaction_id,
                                int64_t object_store_id,
                                int64_t index_id,
                                const IndexedDBKeyRange& key_range,
                                bool key_only,
                                int64_t max_count,
                                blink::WebIDBCallbacks* callbacks);

  void RequestIDBDatabasePut(
      int32_t ipc_database_id,
      int64_t transaction_id,
      int64_t object_store_id,
      const blink::WebData& value,
      const blink::WebVector<blink::WebBlobInfo>& web_blob_info,
      const IndexedDBKey& key,
      blink::WebIDBPutMode put_mode,
      blink::WebIDBCallbacks* callbacks,
      const blink::WebVector<long long>& index_ids,
      const blink::WebVector<blink::WebVector<blink::WebIDBKey>>& index_keys);

  void RequestIDBDatabaseOpenCursor(int32_t ipc_database_id,
                                    int64_t transaction_id,
                                    int64_t object_store_id,
                                    int64_t index_id,
                                    const IndexedDBKeyRange& key_range,
                                    blink::WebIDBCursorDirection direction,
                                    bool key_only,
                                    blink::WebIDBTaskType task_type,
                                    blink::WebIDBCallbacks* callbacks);

  void RequestIDBDatabaseCount(int32_t ipc_database_id,
                               int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const IndexedDBKeyRange& key_range,
                               blink::WebIDBCallbacks* callbacks);

  void RequestIDBDatabaseDeleteRange(int32_t ipc_database_id,
                                     int64_t transaction_id,
                                     int64_t object_store_id,
                                     const IndexedDBKeyRange& key_range,
                                     blink::WebIDBCallbacks* callbacks);

  void RequestIDBDatabaseClear(int32_t ipc_database_id,
                               int64_t transaction_id,
                               int64_t object_store_id,
                               blink::WebIDBCallbacks* callbacks);

  virtual void CursorDestroyed(int32_t ipc_cursor_id);
  void DatabaseDestroyed(int32_t ipc_database_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorReset);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorTransactionId);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, ValueSizeTest);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, KeyAndValueSizeTest);

  enum { kAllCursors = -1 };

  static int32_t CurrentWorkerId() { return WorkerThread::GetCurrentId(); }

  template <typename T>
  void init_params(T* params, blink::WebIDBCallbacks* callbacks_ptr) {
    std::unique_ptr<blink::WebIDBCallbacks> callbacks(callbacks_ptr);
    params->ipc_thread_id = CurrentWorkerId();
    params->ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  }

  // IDBCallback message handlers.
  void OnSuccessIDBDatabase(int32_t ipc_thread_id,
                            int32_t ipc_callbacks_id,
                            int32_t ipc_database_callbacks_id,
                            int32_t ipc_object_id,
                            const IndexedDBDatabaseMetadata& idb_metadata);
  void OnSuccessIndexedDBKey(int32_t ipc_thread_id,
                             int32_t ipc_callbacks_id,
                             const IndexedDBKey& key);

  void OnSuccessOpenCursor(
      const IndexedDBMsg_CallbacksSuccessIDBCursor_Params& p);
  void OnSuccessCursorContinue(
      const IndexedDBMsg_CallbacksSuccessCursorContinue_Params& p);
  void OnSuccessCursorPrefetch(
      const IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params& p);
  void OnSuccessStringList(int32_t ipc_thread_id,
                           int32_t ipc_callbacks_id,
                           const std::vector<base::string16>& value);
  void OnSuccessValue(const IndexedDBMsg_CallbacksSuccessValue_Params& p);
  void OnSuccessArray(const IndexedDBMsg_CallbacksSuccessArray_Params& p);
  void OnSuccessInteger(int32_t ipc_thread_id,
                        int32_t ipc_callbacks_id,
                        int64_t value);
  void OnSuccessUndefined(int32_t ipc_thread_id, int32_t ipc_callbacks_id);
  void OnError(int32_t ipc_thread_id,
               int32_t ipc_callbacks_id,
               int code,
               const base::string16& message);
  void OnIntBlocked(int32_t ipc_thread_id,
                    int32_t ipc_callbacks_id,
                    int64_t existing_version);
  void OnUpgradeNeeded(const IndexedDBMsg_CallbacksUpgradeNeeded_Params& p);
  void OnAbort(int32_t ipc_thread_id,
               int32_t ipc_database_id,
               int64_t transaction_id,
               int code,
               const base::string16& message);
  void OnComplete(int32_t ipc_thread_id,
                  int32_t ipc_database_id,
                  int64_t transaction_id);
  void OnForcedClose(int32_t ipc_thread_id, int32_t ipc_database_id);
  void OnVersionChange(int32_t ipc_thread_id,
                       int32_t ipc_database_id,
                       int64_t old_version,
                       int64_t new_version);

  // Reset cursor prefetch caches for all cursors except exception_cursor_id.
  void ResetCursorPrefetchCaches(int64_t transaction_id,
                                 int32_t ipc_exception_cursor_id);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // Maximum size (in bytes) of value/key pair allowed for put requests. Any
  // requests larger than this size will be rejected.
  // Used by unit tests to exercise behavior without allocating huge chunks
  // of memory.
  size_t max_put_value_size_ = kMaxIDBMessageSizeInBytes;

  // Careful! WebIDBCallbacks wraps non-threadsafe data types. It must be
  // destroyed and used on the same thread it was created on.
  IDMap<blink::WebIDBCallbacks, IDMapOwnPointer> pending_callbacks_;
  IDMap<blink::WebIDBDatabaseCallbacks, IDMapOwnPointer>
      pending_database_callbacks_;

  // Maps the ipc_callback_id from an open cursor request to the request's
  // transaction_id. Used to assign the transaction_id to the WebIDBCursorImpl
  // when it is created.
  std::map<int32_t, int64_t> cursor_transaction_ids_;

  // Map from cursor id to WebIDBCursorImpl.
  std::map<int32_t, WebIDBCursorImpl*> cursors_;

  std::map<int32_t, WebIDBDatabaseImpl*> databases_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_INDEXED_DB_DISPATCHER_H_
