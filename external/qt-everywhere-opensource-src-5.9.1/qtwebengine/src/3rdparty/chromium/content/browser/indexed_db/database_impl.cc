// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/database_impl.h"

#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseException.h"

using std::swap;

namespace content {

namespace {
const char kInvalidBlobUuid[] = "Blob UUID is invalid";
}  // namespace

class DatabaseImpl::IDBThreadHelper {
 public:
  IDBThreadHelper(std::unique_ptr<IndexedDBConnection> connection,
                  const url::Origin& origin,
                  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host);
  ~IDBThreadHelper();

  void CreateObjectStore(int64_t transaction_id,
                         int64_t object_store_id,
                         const base::string16& name,
                         const IndexedDBKeyPath& key_path,
                         bool auto_increment);
  void DeleteObjectStore(int64_t transaction_id, int64_t object_store_id);
  void RenameObjectStore(int64_t transaction_id,
                         int64_t object_store_id,
                         const base::string16& new_name);
  void CreateTransaction(int64_t transaction_id,
                         const std::vector<int64_t>& object_store_ids,
                         blink::WebIDBTransactionMode mode);
  void Close();
  void VersionChangeIgnored();
  void AddObserver(int64_t transaction_id,
                   int32_t observer_id,
                   bool include_transaction,
                   bool no_records,
                   bool values,
                   uint16_t operation_types);
  void RemoveObservers(const std::vector<int32_t>& observers);
  void Get(int64_t transaction_id,
           int64_t object_store_id,
           int64_t index_id,
           const IndexedDBKeyRange& key_range,
           bool key_only,
           scoped_refptr<IndexedDBCallbacks> callbacks);
  void GetAll(int64_t transaction_id,
              int64_t object_store_id,
              int64_t index_id,
              const IndexedDBKeyRange& key_range,
              bool key_only,
              int64_t max_count,
              scoped_refptr<IndexedDBCallbacks> callbacks);
  void Put(int64_t transaction_id,
           int64_t object_store_id,
           ::indexed_db::mojom::ValuePtr value,
           std::vector<std::unique_ptr<storage::BlobDataHandle>> handles,
           const IndexedDBKey& key,
           blink::WebIDBPutMode mode,
           const std::vector<IndexedDBIndexKeys>& index_keys,
           scoped_refptr<IndexedDBCallbacks> callbacks);
  void SetIndexKeys(int64_t transaction_id,
                    int64_t object_store_id,
                    const IndexedDBKey& primary_key,
                    const std::vector<IndexedDBIndexKeys>& index_keys);
  void SetIndexesReady(int64_t transaction_id,
                       int64_t object_store_id,
                       const std::vector<int64_t>& index_ids);
  void OpenCursor(int64_t transaction_id,
                  int64_t object_store_id,
                  int64_t index_id,
                  const IndexedDBKeyRange& key_range,
                  blink::WebIDBCursorDirection direction,
                  bool key_only,
                  blink::WebIDBTaskType task_type,
                  scoped_refptr<IndexedDBCallbacks> callbacks);
  void Count(int64_t transaction_id,
             int64_t object_store_id,
             int64_t index_id,
             const IndexedDBKeyRange& key_range,
             scoped_refptr<IndexedDBCallbacks> callbacks);
  void DeleteRange(int64_t transaction_id,
                   int64_t object_store_id,
                   const IndexedDBKeyRange& key_range,
                   scoped_refptr<IndexedDBCallbacks> callbacks);
  void Clear(int64_t transaction_id,
             int64_t object_store_id,
             scoped_refptr<IndexedDBCallbacks> callbacks);
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
  void Abort(int64_t transaction_id);
  void Commit(int64_t transaction_id);
  void OnGotUsageAndQuotaForCommit(int64_t transaction_id,
                                   storage::QuotaStatusCode status,
                                   int64_t usage,
                                   int64_t quota);
  void AckReceivedBlobs(const std::vector<std::string>& uuids);

 private:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  std::unique_ptr<IndexedDBConnection> connection_;
  const url::Origin origin_;
  base::WeakPtrFactory<IDBThreadHelper> weak_factory_;
};

DatabaseImpl::DatabaseImpl(
    std::unique_ptr<IndexedDBConnection> connection,
    const url::Origin& origin,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host)
    : dispatcher_host_(dispatcher_host),
      origin_(origin),
      idb_runner_(base::ThreadTaskRunnerHandle::Get()) {
  helper_ = new IDBThreadHelper(std::move(connection), origin,
                                std::move(dispatcher_host));
}

DatabaseImpl::~DatabaseImpl() {
  idb_runner_->DeleteSoon(FROM_HERE, helper_);
}

void DatabaseImpl::CreateObjectStore(int64_t transaction_id,
                                     int64_t object_store_id,
                                     const base::string16& name,
                                     const IndexedDBKeyPath& key_path,
                                     bool auto_increment) {
  idb_runner_->PostTask(
      FROM_HERE, base::Bind(&IDBThreadHelper::CreateObjectStore,
                            base::Unretained(helper_), transaction_id,
                            object_store_id, name, key_path, auto_increment));
}

void DatabaseImpl::DeleteObjectStore(int64_t transaction_id,
                                     int64_t object_store_id) {
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::DeleteObjectStore, base::Unretained(helper_),
                 transaction_id, object_store_id));
}

void DatabaseImpl::RenameObjectStore(int64_t transaction_id,
                                     int64_t object_store_id,
                                     const base::string16& new_name) {
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::RenameObjectStore, base::Unretained(helper_),
                 transaction_id, object_store_id, new_name));
}

void DatabaseImpl::CreateTransaction(
    int64_t transaction_id,
    const std::vector<int64_t>& object_store_ids,
    blink::WebIDBTransactionMode mode) {
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::CreateTransaction, base::Unretained(helper_),
                 transaction_id, object_store_ids, mode));
}

void DatabaseImpl::Close() {
  idb_runner_->PostTask(FROM_HERE, base::Bind(&IDBThreadHelper::Close,
                                              base::Unretained(helper_)));
}

void DatabaseImpl::VersionChangeIgnored() {
  idb_runner_->PostTask(FROM_HERE,
                        base::Bind(&IDBThreadHelper::VersionChangeIgnored,
                                   base::Unretained(helper_)));
}

void DatabaseImpl::AddObserver(int64_t transaction_id,
                               int32_t observer_id,
                               bool include_transaction,
                               bool no_records,
                               bool values,
                               uint16_t operation_types) {
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::AddObserver, base::Unretained(helper_),
                 transaction_id, observer_id, include_transaction, no_records,
                 values, operation_types));
}

void DatabaseImpl::RemoveObservers(const std::vector<int32_t>& observers) {
  idb_runner_->PostTask(FROM_HERE,
                        base::Bind(&IDBThreadHelper::RemoveObservers,
                                   base::Unretained(helper_), observers));
}

void DatabaseImpl::Get(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    bool key_only,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info) {
  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      dispatcher_host_.get(), origin_, std::move(callbacks_info)));
  idb_runner_->PostTask(
      FROM_HERE, base::Bind(&IDBThreadHelper::Get, base::Unretained(helper_),
                            transaction_id, object_store_id, index_id,
                            key_range, key_only, base::Passed(&callbacks)));
}

void DatabaseImpl::GetAll(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    bool key_only,
    int64_t max_count,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info) {
  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      dispatcher_host_.get(), origin_, std::move(callbacks_info)));
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::GetAll, base::Unretained(helper_),
                 transaction_id, object_store_id, index_id, key_range, key_only,
                 max_count, base::Passed(&callbacks)));
}

void DatabaseImpl::Put(
    int64_t transaction_id,
    int64_t object_store_id,
    ::indexed_db::mojom::ValuePtr value,
    const IndexedDBKey& key,
    blink::WebIDBPutMode mode,
    const std::vector<IndexedDBIndexKeys>& index_keys,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info) {
  std::vector<std::unique_ptr<storage::BlobDataHandle>> handles;
  for (const auto& info : value->blob_or_file_info) {
    std::unique_ptr<storage::BlobDataHandle> handle =
        dispatcher_host_->blob_storage_context()->GetBlobDataFromUUID(
            info->uuid);
    if (!handle) {
      mojo::ReportBadMessage(kInvalidBlobUuid);
      return;
    }
    handles.push_back(std::move(handle));
  }

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      dispatcher_host_.get(), origin_, std::move(callbacks_info)));

  idb_runner_->PostTask(
      FROM_HERE, base::Bind(&IDBThreadHelper::Put, base::Unretained(helper_),
                            transaction_id, object_store_id,
                            base::Passed(&value), base::Passed(&handles), key,
                            mode, index_keys, base::Passed(&callbacks)));
}

void DatabaseImpl::SetIndexKeys(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKey& primary_key,
    const std::vector<IndexedDBIndexKeys>& index_keys) {
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::SetIndexKeys, base::Unretained(helper_),
                 transaction_id, object_store_id, primary_key, index_keys));
}

void DatabaseImpl::SetIndexesReady(int64_t transaction_id,
                                   int64_t object_store_id,
                                   const std::vector<int64_t>& index_ids) {
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::SetIndexesReady, base::Unretained(helper_),
                 transaction_id, object_store_id, index_ids));
}

void DatabaseImpl::OpenCursor(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::WebIDBCursorDirection direction,
    bool key_only,
    blink::WebIDBTaskType task_type,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info) {
  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      dispatcher_host_.get(), origin_, std::move(callbacks_info)));
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::OpenCursor, base::Unretained(helper_),
                 transaction_id, object_store_id, index_id, key_range,
                 direction, key_only, task_type, base::Passed(&callbacks)));
}

void DatabaseImpl::Count(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info) {
  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      dispatcher_host_.get(), origin_, std::move(callbacks_info)));
  idb_runner_->PostTask(
      FROM_HERE, base::Bind(&IDBThreadHelper::Count, base::Unretained(helper_),
                            transaction_id, object_store_id, index_id,
                            key_range, base::Passed(&callbacks)));
}

void DatabaseImpl::DeleteRange(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info) {
  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      dispatcher_host_.get(), origin_, std::move(callbacks_info)));
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::DeleteRange, base::Unretained(helper_),
                 transaction_id, object_store_id, key_range,
                 base::Passed(&callbacks)));
}

void DatabaseImpl::Clear(
    int64_t transaction_id,
    int64_t object_store_id,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info) {
  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      dispatcher_host_.get(), origin_, std::move(callbacks_info)));
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::Clear, base::Unretained(helper_),
                 transaction_id, object_store_id, base::Passed(&callbacks)));
}

void DatabaseImpl::CreateIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const base::string16& name,
                               const IndexedDBKeyPath& key_path,
                               bool unique,
                               bool multi_entry) {
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::CreateIndex, base::Unretained(helper_),
                 transaction_id, object_store_id, index_id, name, key_path,
                 unique, multi_entry));
}

void DatabaseImpl::DeleteIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id) {
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::DeleteIndex, base::Unretained(helper_),
                 transaction_id, object_store_id, index_id));
}

void DatabaseImpl::RenameIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const base::string16& new_name) {
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::RenameIndex, base::Unretained(helper_),
                 transaction_id, object_store_id, index_id, new_name));
}

void DatabaseImpl::Abort(int64_t transaction_id) {
  idb_runner_->PostTask(
      FROM_HERE, base::Bind(&IDBThreadHelper::Abort, base::Unretained(helper_),
                            transaction_id));
}

void DatabaseImpl::Commit(int64_t transaction_id) {
  idb_runner_->PostTask(
      FROM_HERE, base::Bind(&IDBThreadHelper::Commit, base::Unretained(helper_),
                            transaction_id));
}

void DatabaseImpl::AckReceivedBlobs(const std::vector<std::string>& uuids) {
  for (const auto& uuid : uuids)
    dispatcher_host_->DropBlobData(uuid);
}

DatabaseImpl::IDBThreadHelper::IDBThreadHelper(
    std::unique_ptr<IndexedDBConnection> connection,
    const url::Origin& origin,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host)
    : dispatcher_host_(std::move(dispatcher_host)),
      connection_(std::move(connection)),
      origin_(origin),
      weak_factory_(this) {
  dispatcher_host_->context()->ConnectionOpened(origin_, connection.get());
}

DatabaseImpl::IDBThreadHelper::~IDBThreadHelper() {
  if (connection_->IsConnected())
    connection_->Close();
  dispatcher_host_->context()->ConnectionClosed(origin_, connection_.get());
}

void DatabaseImpl::IDBThreadHelper::CreateObjectStore(
    int64_t transaction_id,
    int64_t object_store_id,
    const base::string16& name,
    const IndexedDBKeyPath& key_path,
    bool auto_increment) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->CreateObjectStore(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      name, key_path, auto_increment);
}

void DatabaseImpl::IDBThreadHelper::DeleteObjectStore(int64_t transaction_id,
                                                      int64_t object_store_id) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->DeleteObjectStore(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id);
}

void DatabaseImpl::IDBThreadHelper::RenameObjectStore(
    int64_t transaction_id,
    int64_t object_store_id,
    const base::string16& new_name) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->RenameObjectStore(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      new_name);
}

void DatabaseImpl::IDBThreadHelper::CreateTransaction(
    int64_t transaction_id,
    const std::vector<int64_t>& object_store_ids,
    blink::WebIDBTransactionMode mode) {
  if (!connection_->IsConnected())
    return;

  int64_t host_transaction_id =
      dispatcher_host_->HostTransactionId(transaction_id);
  if (!dispatcher_host_->RegisterTransactionId(host_transaction_id, origin_)) {
    DLOG(ERROR) << "Duplicate host_transaction_id.";
    return;
  }

  connection_->database()->CreateTransaction(
      host_transaction_id, connection_.get(), object_store_ids, mode);
}

void DatabaseImpl::IDBThreadHelper::Close() {
  if (!connection_->IsConnected())
    return;

  connection_->Close();
}

void DatabaseImpl::IDBThreadHelper::VersionChangeIgnored() {
  if (!connection_->IsConnected())
    return;

  connection_->VersionChangeIgnored();
}

void DatabaseImpl::IDBThreadHelper::AddObserver(int64_t transaction_id,
                                                int32_t observer_id,
                                                bool include_transaction,
                                                bool no_records,
                                                bool values,
                                                uint16_t operation_types) {
  if (!connection_->IsConnected())
    return;

  IndexedDBObserver::Options options(include_transaction, no_records, values,
                                     operation_types);
  connection_->database()->AddPendingObserver(
      dispatcher_host_->HostTransactionId(transaction_id), observer_id,
      options);
}

void DatabaseImpl::IDBThreadHelper::RemoveObservers(
    const std::vector<int32_t>& observers) {
  if (!connection_->IsConnected())
    return;

  connection_->RemoveObservers(observers);
}

void DatabaseImpl::IDBThreadHelper::Get(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    bool key_only,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->Get(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      index_id, base::MakeUnique<IndexedDBKeyRange>(key_range), key_only,
      callbacks);
}

void DatabaseImpl::IDBThreadHelper::GetAll(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    bool key_only,
    int64_t max_count,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->GetAll(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      index_id, base::MakeUnique<IndexedDBKeyRange>(key_range), key_only,
      max_count, std::move(callbacks));
}

void DatabaseImpl::IDBThreadHelper::Put(
    int64_t transaction_id,
    int64_t object_store_id,
    ::indexed_db::mojom::ValuePtr mojo_value,
    std::vector<std::unique_ptr<storage::BlobDataHandle>> handles,
    const IndexedDBKey& key,
    blink::WebIDBPutMode mode,
    const std::vector<IndexedDBIndexKeys>& index_keys,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  if (!connection_->IsConnected())
    return;

  int64_t host_transaction_id =
      dispatcher_host_->HostTransactionId(transaction_id);

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  std::vector<IndexedDBBlobInfo> blob_info(
      mojo_value->blob_or_file_info.size());
  for (size_t i = 0; i < mojo_value->blob_or_file_info.size(); ++i) {
    const auto& info = mojo_value->blob_or_file_info[i];
    if (info->file) {
      if (!info->file->path.empty()) {
        if (!policy->CanReadFile(dispatcher_host_->ipc_process_id(),
                                 info->file->path)) {
          bad_message::ReceivedBadMessage(dispatcher_host_.get(),
                                          bad_message::IDBDH_CAN_READ_FILE);
          return;
        }
      }
      blob_info[i] = IndexedDBBlobInfo(info->uuid, info->file->path,
                                       info->file->name, info->mime_type);
      if (info->size != static_cast<uint64_t>(-1)) {
        blob_info[i].set_last_modified(info->file->last_modified);
        blob_info[i].set_size(info->size);
      }
    } else {
      blob_info[i] = IndexedDBBlobInfo(info->uuid, info->mime_type, info->size);
    }
  }

  uint64_t commit_size = mojo_value->bits.size();
  IndexedDBValue value;
  swap(value.bits, mojo_value->bits);
  swap(value.blob_info, blob_info);
  connection_->database()->Put(host_transaction_id, object_store_id, &value,
                               &handles, base::MakeUnique<IndexedDBKey>(key),
                               mode, std::move(callbacks), index_keys);

  // Size can't be big enough to overflow because it represents the
  // actual bytes passed through IPC.
  dispatcher_host_->AddToTransaction(host_transaction_id, commit_size);
}

void DatabaseImpl::IDBThreadHelper::SetIndexKeys(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKey& primary_key,
    const std::vector<IndexedDBIndexKeys>& index_keys) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->SetIndexKeys(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      base::MakeUnique<IndexedDBKey>(primary_key), index_keys);
}

void DatabaseImpl::IDBThreadHelper::SetIndexesReady(
    int64_t transaction_id,
    int64_t object_store_id,
    const std::vector<int64_t>& index_ids) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->SetIndexesReady(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      index_ids);
}

void DatabaseImpl::IDBThreadHelper::OpenCursor(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::WebIDBCursorDirection direction,
    bool key_only,
    blink::WebIDBTaskType task_type,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->OpenCursor(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      index_id, base::MakeUnique<IndexedDBKeyRange>(key_range), direction,
      key_only, task_type, std::move(callbacks));
}

void DatabaseImpl::IDBThreadHelper::Count(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->Count(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      index_id, base::MakeUnique<IndexedDBKeyRange>(key_range),
      std::move(callbacks));
}

void DatabaseImpl::IDBThreadHelper::DeleteRange(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->DeleteRange(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      base::MakeUnique<IndexedDBKeyRange>(key_range), std::move(callbacks));
}

void DatabaseImpl::IDBThreadHelper::Clear(
    int64_t transaction_id,
    int64_t object_store_id,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->Clear(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      callbacks);
}

void DatabaseImpl::IDBThreadHelper::CreateIndex(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const base::string16& name,
    const IndexedDBKeyPath& key_path,
    bool unique,
    bool multi_entry) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->CreateIndex(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      index_id, name, key_path, unique, multi_entry);
}

void DatabaseImpl::IDBThreadHelper::DeleteIndex(int64_t transaction_id,
                                                int64_t object_store_id,
                                                int64_t index_id) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->DeleteIndex(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      index_id);
}

void DatabaseImpl::IDBThreadHelper::RenameIndex(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const base::string16& new_name) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->RenameIndex(
      dispatcher_host_->HostTransactionId(transaction_id), object_store_id,
      index_id, new_name);
}

void DatabaseImpl::IDBThreadHelper::Abort(int64_t transaction_id) {
  if (!connection_->IsConnected())
    return;

  connection_->database()->Abort(
      dispatcher_host_->HostTransactionId(transaction_id));
}

void DatabaseImpl::IDBThreadHelper::Commit(int64_t transaction_id) {
  if (!connection_->IsConnected())
    return;

  int64_t host_transaction_id =
      dispatcher_host_->HostTransactionId(transaction_id);
  // May have been aborted by back end before front-end could request commit.
  int64_t transaction_size;
  if (!dispatcher_host_->GetTransactionSize(host_transaction_id,
                                            &transaction_size))
    return;

  // Always allow empty or delete-only transactions.
  if (transaction_size == 0) {
    connection_->database()->Commit(host_transaction_id);
    return;
  }

  dispatcher_host_->context()->quota_manager_proxy()->GetUsageAndQuota(
      dispatcher_host_->context()->TaskRunner(), origin_.GetURL(),
      storage::kStorageTypeTemporary,
      base::Bind(&IDBThreadHelper::OnGotUsageAndQuotaForCommit,
                 weak_factory_.GetWeakPtr(), transaction_id));
}

void DatabaseImpl::IDBThreadHelper::OnGotUsageAndQuotaForCommit(
    int64_t transaction_id,
    storage::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  // May have disconnected while quota check was pending.
  if (!connection_->IsConnected())
    return;

  int64_t host_transaction_id =
      dispatcher_host_->HostTransactionId(transaction_id);
  // May have aborted while quota check was pending.
  int64_t transaction_size;
  if (!dispatcher_host_->GetTransactionSize(host_transaction_id,
                                            &transaction_size))
    return;

  if (status == storage::kQuotaStatusOk && usage + transaction_size <= quota) {
    connection_->database()->Commit(host_transaction_id);
  } else {
    connection_->database()->Abort(
        host_transaction_id,
        IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionQuotaError));
  }
}

}  // namespace content
