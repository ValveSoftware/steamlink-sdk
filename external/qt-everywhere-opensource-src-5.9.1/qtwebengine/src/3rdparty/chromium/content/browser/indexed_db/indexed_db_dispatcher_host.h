// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/common/indexed_db/indexed_db.mojom.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "url/gurl.h"

struct IndexedDBHostMsg_DatabaseObserve_Params;
struct IndexedDBMsg_Observation;
struct IndexedDBMsg_ObserverChanges;

namespace url {
class Origin;
}

namespace content {
class IndexedDBBlobInfo;
class IndexedDBCallbacks;
class IndexedDBConnection;
class IndexedDBContextImpl;
class IndexedDBCursor;
class IndexedDBDatabaseCallbacks;
class IndexedDBKey;
class IndexedDBObservation;
class IndexedDBObserverChanges;

// Handles all IndexedDB related messages from a particular renderer process.
class IndexedDBDispatcherHost
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<::indexed_db::mojom::Factory>,
      public ::indexed_db::mojom::Factory {
 public:
  // Only call the constructor from the UI thread.
  IndexedDBDispatcherHost(int ipc_process_id,
                          net::URLRequestContextGetter* request_context_getter,
                          IndexedDBContextImpl* indexed_db_context,
                          ChromeBlobStorageContext* blob_storage_context);

  static IndexedDBMsg_ObserverChanges ConvertObserverChanges(
      std::unique_ptr<IndexedDBObserverChanges> changes);
  static IndexedDBMsg_Observation ConvertObservation(
      const IndexedDBObservation* observation);

  // BrowserMessageFilter implementation.
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
  int ipc_process_id() const { return ipc_process_id_; }

  // IndexedDBCallbacks call these methods to add the results into the
  // applicable map.  See below for more details.
  int32_t Add(IndexedDBCursor* cursor);

  bool RegisterTransactionId(int64_t host_transaction_id,
                             const url::Origin& origin);
  bool GetTransactionSize(int64_t host_transaction_id,
                          int64_t* transaction_size);
  void AddToTransaction(int64_t host_transaction_id, int64_t value_length);

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
  void DropBlobData(const std::string& uuid);

  // True if the channel is closing/closed and outstanding requests
  // can be abandoned. Only access on IndexedDB thread.
  bool IsOpen() const;

 private:
  // Friends to enable OnDestruct() delegation.
  friend class BrowserThread;
  friend class base::DeleteHelper<IndexedDBDispatcherHost>;

  // Used in nested classes.
  typedef std::map<int64_t, int64_t> TransactionIDToDatabaseIDMap;
  typedef std::map<int64_t, int64_t> TransactionIDToSizeMap;
  typedef std::map<int64_t, url::Origin> TransactionIDToOriginMap;

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
  ReturnType* GetOrTerminateProcess(RefIDMap<ReturnType>* map,
                                    int32_t ipc_return_object_id);

  template <typename MapType>
  void DestroyObject(MapType* map, int32_t ipc_object_id);

  // indexed_db::mojom::Factory implementation:
  void GetDatabaseNames(
      ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info,
      const url::Origin& origin) override;
  void Open(int32_t worker_thread,
            ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info,
            ::indexed_db::mojom::DatabaseCallbacksAssociatedPtrInfo
                database_callbacks_info,
            const url::Origin& origin,
            const base::string16& name,
            int64_t version,
            int64_t transaction_id) override;
  void DeleteDatabase(
      ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info,
      const url::Origin& origin,
      const base::string16& name) override;

  void GetDatabaseNamesOnIDBThread(scoped_refptr<IndexedDBCallbacks> callbacks,
                                   const url::Origin& origin);
  void OpenOnIDBThread(
      scoped_refptr<IndexedDBCallbacks> callbacks,
      scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
      const url::Origin& origin,
      const base::string16& name,
      int64_t version,
      int64_t transaction_id);
  void DeleteDatabaseOnIDBThread(scoped_refptr<IndexedDBCallbacks> callbacks,
                                 const url::Origin& origin,
                                 const base::string16& name);

  // Message processing. Most of the work is delegated to the dispatcher hosts
  // below.
  void OnAckReceivedBlobs(const std::vector<std::string>& uuids);

  void ResetDispatcherHosts();

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Maps blob uuid to a pair (handle, ref count). Entry is added and/or count
  // is incremented in HoldBlobData(), and count is decremented and/or entry
  // removed in DropBlobData().
  std::map<std::string,
           std::pair<std::unique_ptr<storage::BlobDataHandle>, int>>
      blob_data_handle_map_;

  // Only access on IndexedDB thread.
  bool is_open_ = true;
  TransactionIDToSizeMap transaction_size_map_;
  TransactionIDToOriginMap transaction_origin_map_;
  std::unique_ptr<CursorDispatcherHost> cursor_dispatcher_host_;

  // Used to set file permissions for blob storage.
  int ipc_process_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
