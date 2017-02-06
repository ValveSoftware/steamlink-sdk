// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/public/browser/browser_message_filter.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/quota/quota_status_code.h"
#include "url/gurl.h"

struct IndexedDBDatabaseMetadata;
struct IndexedDBHostMsg_DatabaseCount_Params;
struct IndexedDBHostMsg_DatabaseCreateIndex_Params;
struct IndexedDBHostMsg_DatabaseCreateObjectStore_Params;
struct IndexedDBHostMsg_DatabaseCreateTransaction_Params;
struct IndexedDBHostMsg_DatabaseDeleteRange_Params;
struct IndexedDBHostMsg_DatabaseGet_Params;
struct IndexedDBHostMsg_DatabaseGetAll_Params;
struct IndexedDBHostMsg_DatabaseOpenCursor_Params;
struct IndexedDBHostMsg_DatabasePut_Params;
struct IndexedDBHostMsg_DatabaseSetIndexKeys_Params;
struct IndexedDBHostMsg_FactoryDeleteDatabase_Params;
struct IndexedDBHostMsg_FactoryGetDatabaseNames_Params;
struct IndexedDBHostMsg_FactoryOpen_Params;

namespace url {
class Origin;
}

namespace content {
class IndexedDBBlobInfo;
class IndexedDBConnection;
class IndexedDBContextImpl;
class IndexedDBCursor;
class IndexedDBKey;
class IndexedDBKeyPath;
class IndexedDBKeyRange;
struct IndexedDBDatabaseMetadata;

// Handles all IndexedDB related messages from a particular renderer process.
class IndexedDBDispatcherHost : public BrowserMessageFilter {
 public:
  // Only call the constructor from the UI thread.
  IndexedDBDispatcherHost(int ipc_process_id,
                          net::URLRequestContextGetter* request_context_getter,
                          IndexedDBContextImpl* indexed_db_context,
                          ChromeBlobStorageContext* blob_storage_context);
  IndexedDBDispatcherHost(int ipc_process_id,
                          net::URLRequestContext* request_context,
                          IndexedDBContextImpl* indexed_db_context,
                          ChromeBlobStorageContext* blob_storage_context);

  static ::IndexedDBDatabaseMetadata ConvertMetadata(
      const content::IndexedDBDatabaseMetadata& metadata);

  // BrowserMessageFilter implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelClosing() override;
  void OnDestruct() const override;
  base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void FinishTransaction(int64_t host_transaction_id, bool committed);

  // A shortcut for accessing our context.
  IndexedDBContextImpl* context() const { return indexed_db_context_.get(); }
  storage::BlobStorageContext* blob_storage_context() const {
    return blob_storage_context_->context();
  }

  // IndexedDBCallbacks call these methods to add the results into the
  // applicable map.  See below for more details.
  int32_t Add(IndexedDBCursor* cursor);
  int32_t Add(IndexedDBConnection* connection,
              int32_t ipc_thread_id,
              const url::Origin& origin);

  void RegisterTransactionId(int64_t host_transaction_id,
                             const url::Origin& origin);

  IndexedDBCursor* GetCursorFromId(int32_t ipc_cursor_id);

  // These are called to map a 32-bit front-end (renderer-specific) transaction
  // id to and from a back-end ("host") transaction id that encodes the process
  // id in the high 32 bits. The mapping is host-specific and ids are validated.
  int64_t HostTransactionId(int64_t transaction_id);
  int64_t RendererTransactionId(int64_t host_transaction_id);

  // These are called to decode a host transaction ID, for diagnostic purposes.
  static uint32_t TransactionIdToRendererTransactionId(
      int64_t host_transaction_id);
  static uint32_t TransactionIdToProcessId(int64_t host_transaction_id);

  std::string HoldBlobData(const IndexedDBBlobInfo& blob_info);

 private:
  // Friends to enable OnDestruct() delegation.
  friend class BrowserThread;
  friend class base::DeleteHelper<IndexedDBDispatcherHost>;

  // Used in nested classes.
  typedef std::map<std::string, std::pair<storage::BlobDataHandle*, int>>
      BlobDataHandleMap;
  typedef std::map<int64_t, int64_t> TransactionIDToDatabaseIDMap;
  typedef std::map<int64_t, uint64_t> TransactionIDToSizeMap;
  typedef std::map<int64_t, url::Origin> TransactionIDToOriginMap;
  typedef std::map<int32_t, url::Origin> WebIDBObjectIDToOriginMap;

  // IDMap for RefCounted types
  template <typename RefCountedType>
  class RefIDMap {
   public:
    typedef int32_t KeyType;

    RefIDMap() {}
    ~RefIDMap() {}

    KeyType Add(RefCountedType* data) {
      return map_.Add(new scoped_refptr<RefCountedType>(data));
    }

    RefCountedType* Lookup(KeyType id) {
      scoped_refptr<RefCountedType>* ptr = map_.Lookup(id);
      if (ptr == NULL)
        return NULL;
      return ptr->get();
    }

    void Remove(KeyType id) { map_.Remove(id); }

    void set_check_on_null_data(bool value) {
      map_.set_check_on_null_data(value);
    }

   private:
    IDMap<scoped_refptr<RefCountedType>, IDMapOwnPointer> map_;

    DISALLOW_COPY_AND_ASSIGN(RefIDMap);
  };

  class DatabaseDispatcherHost {
   public:
    explicit DatabaseDispatcherHost(IndexedDBDispatcherHost* parent);
    ~DatabaseDispatcherHost();

    void CloseAll();
    bool OnMessageReceived(const IPC::Message& message);

    void OnCreateObjectStore(
        const IndexedDBHostMsg_DatabaseCreateObjectStore_Params& params);
    void OnDeleteObjectStore(int32_t ipc_database_id,
                             int64_t transaction_id,
                             int64_t object_store_id);
    void OnCreateTransaction(
        const IndexedDBHostMsg_DatabaseCreateTransaction_Params&);
    void OnClose(int32_t ipc_database_id);
    void OnVersionChangeIgnored(int32_t ipc_database_id);
    void OnDestroyed(int32_t ipc_database_id);

    void OnGet(const IndexedDBHostMsg_DatabaseGet_Params& params);
    void OnGetAll(const IndexedDBHostMsg_DatabaseGetAll_Params& params);
    // OnPutWrapper starts on the IO thread so that it can grab BlobDataHandles
    // before posting to the IDB TaskRunner for the rest of the job.
    void OnPutWrapper(const IndexedDBHostMsg_DatabasePut_Params& params);
    void OnPut(const IndexedDBHostMsg_DatabasePut_Params& params,
               std::vector<storage::BlobDataHandle*> handles);
    void OnSetIndexKeys(
        const IndexedDBHostMsg_DatabaseSetIndexKeys_Params& params);
    void OnSetIndexesReady(int32_t ipc_database_id,
                           int64_t transaction_id,
                           int64_t object_store_id,
                           const std::vector<int64_t>& ids);
    void OnOpenCursor(const IndexedDBHostMsg_DatabaseOpenCursor_Params& params);
    void OnCount(const IndexedDBHostMsg_DatabaseCount_Params& params);
    void OnDeleteRange(
        const IndexedDBHostMsg_DatabaseDeleteRange_Params& params);
    void OnClear(int32_t ipc_thread_id,
                 int32_t ipc_callbacks_id,
                 int32_t ipc_database_id,
                 int64_t transaction_id,
                 int64_t object_store_id);
    void OnCreateIndex(
        const IndexedDBHostMsg_DatabaseCreateIndex_Params& params);
    void OnDeleteIndex(int32_t ipc_database_id,
                       int64_t transaction_id,
                       int64_t object_store_id,
                       int64_t index_id);

    void OnAbort(int32_t ipc_database_id, int64_t transaction_id);
    void OnCommit(int32_t ipc_database_id, int64_t transaction_id);
    void OnGotUsageAndQuotaForCommit(int32_t ipc_database_id,
                                     int64_t transaction_id,
                                     storage::QuotaStatusCode status,
                                     int64_t usage,
                                     int64_t quota);

    IndexedDBDispatcherHost* parent_;
    IDMap<IndexedDBConnection, IDMapOwnPointer> map_;
    WebIDBObjectIDToOriginMap database_origin_map_;
    TransactionIDToSizeMap transaction_size_map_;
    TransactionIDToOriginMap transaction_origin_map_;
    TransactionIDToDatabaseIDMap transaction_database_map_;

    // Weak pointers are used when an asynchronous quota request is made, in
    // case the dispatcher is torn down before the response returns.
    base::WeakPtrFactory<DatabaseDispatcherHost> weak_factory_;

   private:
    DISALLOW_COPY_AND_ASSIGN(DatabaseDispatcherHost);
  };

  class CursorDispatcherHost {
   public:
    explicit CursorDispatcherHost(IndexedDBDispatcherHost* parent);
    ~CursorDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message);

    void OnAdvance(int32_t ipc_object_store_id,
                   int32_t ipc_thread_id,
                   int32_t ipc_callbacks_id,
                   uint32_t count);
    void OnContinue(int32_t ipc_object_store_id,
                    int32_t ipc_thread_id,
                    int32_t ipc_callbacks_id,
                    const IndexedDBKey& key,
                    const IndexedDBKey& primary_key);
    void OnPrefetch(int32_t ipc_cursor_id,
                    int32_t ipc_thread_id,
                    int32_t ipc_callbacks_id,
                    int n);
    void OnPrefetchReset(int32_t ipc_cursor_id,
                         int used_prefetches,
                         int unused_prefetches);
    void OnDestroyed(int32_t ipc_cursor_id);

    IndexedDBDispatcherHost* parent_;
    RefIDMap<IndexedDBCursor> map_;

   private:
    DISALLOW_COPY_AND_ASSIGN(CursorDispatcherHost);
  };

  ~IndexedDBDispatcherHost() override;

  // Helper templates.
  template <class ReturnType>
  ReturnType* GetOrTerminateProcess(IDMap<ReturnType, IDMapOwnPointer>* map,
                                    int32_t ipc_return_object_id);
  template <class ReturnType>
  ReturnType* GetOrTerminateProcess(RefIDMap<ReturnType>* map,
                                    int32_t ipc_return_object_id);

  template <typename MapType>
  void DestroyObject(MapType* map, int32_t ipc_object_id);

  // Message processing. Most of the work is delegated to the dispatcher hosts
  // below.
  void OnIDBFactoryGetDatabaseNames(
      const IndexedDBHostMsg_FactoryGetDatabaseNames_Params& p);
  void OnIDBFactoryOpen(const IndexedDBHostMsg_FactoryOpen_Params& p);

  void OnIDBFactoryDeleteDatabase(
      const IndexedDBHostMsg_FactoryDeleteDatabase_Params& p);

  void OnAckReceivedBlobs(const std::vector<std::string>& uuids);
  void OnPutHelper(const IndexedDBHostMsg_DatabasePut_Params& params,
                   std::vector<storage::BlobDataHandle*> handles);

  void ResetDispatcherHosts();
  void DropBlobData(const std::string& uuid);

  // The getter holds the context until OnChannelConnected() can be called from
  // the IO thread, which will extract the net::URLRequestContext from it.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  net::URLRequestContext* request_context_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  BlobDataHandleMap blob_data_handle_map_;

  // Only access on IndexedDB thread.
  std::unique_ptr<DatabaseDispatcherHost> database_dispatcher_host_;
  std::unique_ptr<CursorDispatcherHost> cursor_dispatcher_host_;

  // Used to set file permissions for blob storage.
  int ipc_process_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
