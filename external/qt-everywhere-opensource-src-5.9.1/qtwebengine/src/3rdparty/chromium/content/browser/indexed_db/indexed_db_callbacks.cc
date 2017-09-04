// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_callbacks.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/fileapi_message_filter.h"
#include "content/browser/indexed_db/database_impl.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_return_value.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/common/indexed_db/indexed_db_metadata.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/quota/quota_manager.h"

using indexed_db::mojom::CallbacksAssociatedPtrInfo;
using std::swap;
using storage::ShareableFileReference;

namespace content {

namespace {
const int32_t kNoCursor = -1;
const int64_t kNoTransaction = -1;

void ConvertBlobInfo(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<::indexed_db::mojom::BlobInfoPtr>* blob_or_file_info) {
  blob_or_file_info->reserve(blob_info.size());
  for (const auto& iter : blob_info) {
    if (!iter.mark_used_callback().is_null())
      iter.mark_used_callback().Run();

    auto info = ::indexed_db::mojom::BlobInfo::New();
    info->mime_type = iter.type();
    info->size = iter.size();
    if (iter.is_file()) {
      info->file = ::indexed_db::mojom::FileInfo::New();
      info->file->name = iter.file_name();
      info->file->path = iter.file_path();
      info->file->last_modified = iter.last_modified();
    }
    blob_or_file_info->push_back(std::move(info));
  }
}

// Destructively converts an IndexedDBReturnValue to a Mojo ReturnValue.
::indexed_db::mojom::ReturnValuePtr ConvertReturnValue(
    IndexedDBReturnValue* value) {
  auto mojo_value = ::indexed_db::mojom::ReturnValue::New();
  mojo_value->value = ::indexed_db::mojom::Value::New();
  if (value->primary_key.IsValid()) {
    mojo_value->primary_key = value->primary_key;
    mojo_value->key_path = value->key_path;
  }
  if (!value->empty())
    swap(mojo_value->value->bits, value->bits);
  ConvertBlobInfo(value->blob_info, &mojo_value->value->blob_or_file_info);
  return mojo_value;
}

}  // namespace

class IndexedDBCallbacks::IOThreadHelper {
 public:
  IOThreadHelper(CallbacksAssociatedPtrInfo callbacks_info,
                 scoped_refptr<IndexedDBDispatcherHost> dispatcher_host);
  ~IOThreadHelper();

  void SendError(const IndexedDBDatabaseError& error);
  void SendSuccessStringList(const std::vector<base::string16>& value);
  void SendBlocked(int64_t existing_version);
  void SendUpgradeNeeded(std::unique_ptr<DatabaseImpl> database,
                         int64_t old_version,
                         blink::WebIDBDataLoss data_loss,
                         const std::string& data_loss_message,
                         const content::IndexedDBDatabaseMetadata& metadata);
  void SendSuccessDatabase(std::unique_ptr<DatabaseImpl> database,
                           const content::IndexedDBDatabaseMetadata& metadata);
  void SendSuccessCursor(int32_t cursor_id,
                         const IndexedDBKey& key,
                         const IndexedDBKey& primary_key,
                         ::indexed_db::mojom::ValuePtr value,
                         const std::vector<IndexedDBBlobInfo>& blob_info);
  void SendSuccessValue(::indexed_db::mojom::ReturnValuePtr value,
                        const std::vector<IndexedDBBlobInfo>& blob_info);
  void SendSuccessArray(
      std::vector<::indexed_db::mojom::ReturnValuePtr> mojo_values,
      const std::vector<IndexedDBReturnValue>& values);
  void SendSuccessKey(const IndexedDBKey& value);
  void SendSuccessInteger(int64_t value);
  void SendSuccess();

  std::string CreateBlobData(const IndexedDBBlobInfo& blob_info);
  bool CreateAllBlobs(
      const std::vector<IndexedDBBlobInfo>& blob_info,
      std::vector<::indexed_db::mojom::BlobInfoPtr>* blob_or_file_info);

 private:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  ::indexed_db::mojom::CallbacksAssociatedPtr callbacks_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadHelper);
};

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32_t ipc_thread_id,
                                       int32_t ipc_callbacks_id)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(kNoCursor),
      host_transaction_id_(kNoTransaction),
      data_loss_(blink::WebIDBDataLossNone),
      sent_blocked_(false),
      io_helper_(nullptr) {}

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32_t ipc_thread_id,
                                       int32_t ipc_callbacks_id,
                                       int32_t ipc_cursor_id)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(ipc_cursor_id),
      host_transaction_id_(kNoTransaction),
      data_loss_(blink::WebIDBDataLossNone),
      sent_blocked_(false),
      io_helper_(nullptr) {}

IndexedDBCallbacks::IndexedDBCallbacks(
    IndexedDBDispatcherHost* dispatcher_host,
    const url::Origin& origin,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info)
    : dispatcher_host_(dispatcher_host),
      ipc_cursor_id_(kNoCursor),
      host_transaction_id_(kNoTransaction),
      origin_(origin),
      data_loss_(blink::WebIDBDataLossNone),
      sent_blocked_(false),
      io_helper_(
          new IOThreadHelper(std::move(callbacks_info), dispatcher_host_)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  thread_checker_.DetachFromThread();
}

IndexedDBCallbacks::~IndexedDBCallbacks() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void IndexedDBCallbacks::OnError(const IndexedDBDatabaseError& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);

  if (io_helper_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&IOThreadHelper::SendError,
                   base::Unretained(io_helper_.get()), error));
  } else {
    dispatcher_host_->Send(new IndexedDBMsg_CallbacksError(
        ipc_thread_id_, ipc_callbacks_id_, error.code(), error.message()));
  }
  dispatcher_host_ = nullptr;

  if (!connection_open_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebCore.IndexedDB.OpenTime.Error",
        base::TimeTicks::Now() - connection_open_start_time_);
    connection_open_start_time_ = base::TimeTicks();
  }
}

void IndexedDBCallbacks::OnSuccess(const std::vector<base::string16>& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);
  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendSuccessStringList,
                 base::Unretained(io_helper_.get()), value));
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnBlocked(int64_t existing_version) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);
  DCHECK_EQ(kNoCursor, ipc_cursor_id_);

  if (sent_blocked_)
    return;

  sent_blocked_ = true;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendBlocked,
                 base::Unretained(io_helper_.get()), existing_version));

  if (!connection_open_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebCore.IndexedDB.OpenTime.Blocked",
        base::TimeTicks::Now() - connection_open_start_time_);
    connection_open_start_time_ = base::TimeTicks();
  }
}

void IndexedDBCallbacks::OnUpgradeNeeded(
    int64_t old_version,
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata,
    const IndexedDBDataLossInfo& data_loss_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);

  DCHECK_NE(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK(!database_sent_);

  data_loss_ = data_loss_info.status;
  dispatcher_host_->RegisterTransactionId(host_transaction_id_, origin_);
  database_sent_ = true;
  auto database = base::MakeUnique<DatabaseImpl>(std::move(connection), origin_,
                                                 dispatcher_host_);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendUpgradeNeeded,
                 base::Unretained(io_helper_.get()), base::Passed(&database),
                 old_version, data_loss_info.status, data_loss_info.message,
                 metadata));

  if (!connection_open_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebCore.IndexedDB.OpenTime.UpgradeNeeded",
        base::TimeTicks::Now() - connection_open_start_time_);
    connection_open_start_time_ = base::TimeTicks();
  }
}

void IndexedDBCallbacks::OnSuccess(
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_NE(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(database_sent_, !connection);

  scoped_refptr<IndexedDBCallbacks> self(this);

  // Only send a new Database if the connection was not previously sent in
  // OnUpgradeNeeded.
  std::unique_ptr<DatabaseImpl> database;
  if (!database_sent_)
    database.reset(
        new DatabaseImpl(std::move(connection), origin_, dispatcher_host_));

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&IOThreadHelper::SendSuccessDatabase,
                                     base::Unretained(io_helper_.get()),
                                     base::Passed(&database), metadata));
  dispatcher_host_ = nullptr;

  if (!connection_open_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebCore.IndexedDB.OpenTime.Success",
        base::TimeTicks::Now() - connection_open_start_time_);
    connection_open_start_time_ = base::TimeTicks();
  }
}

static std::string CreateBlobData(
    const IndexedDBBlobInfo& blob_info,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    base::TaskRunner* task_runner) {
  if (!blob_info.uuid().empty()) {
    // We're sending back a live blob, not a reference into our backing store.
    return dispatcher_host->HoldBlobData(blob_info);
  }
  scoped_refptr<ShareableFileReference> shareable_file =
      ShareableFileReference::Get(blob_info.file_path());
  if (!shareable_file.get()) {
    shareable_file = ShareableFileReference::GetOrCreate(
        blob_info.file_path(),
        ShareableFileReference::DONT_DELETE_ON_FINAL_RELEASE,
        task_runner);
    if (!blob_info.release_callback().is_null())
      shareable_file->AddFinalReleaseCallback(blob_info.release_callback());
  }
  return dispatcher_host->HoldBlobData(blob_info);
}

static bool CreateAllBlobs(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<IndexedDBMsg_BlobOrFileInfo>* blob_or_file_info,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host) {
  IDB_TRACE("IndexedDBCallbacks::CreateAllBlobs");
  DCHECK_EQ(blob_info.size(), blob_or_file_info->size());
  size_t i;
  if (!dispatcher_host->blob_storage_context())
    return false;
  for (i = 0; i < blob_info.size(); ++i) {
    (*blob_or_file_info)[i].uuid =
        CreateBlobData(blob_info[i], dispatcher_host,
                       dispatcher_host->context()->TaskRunner());
  }
  return true;
}

template <class ParamType, class MsgType>
static void CreateBlobsAndSend(
    ParamType* params,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<IndexedDBMsg_BlobOrFileInfo>* blob_or_file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (CreateAllBlobs(blob_info, blob_or_file_info, dispatcher_host))
    dispatcher_host->Send(new MsgType(*params));
}

static void BlobLookupForCursorPrefetch(
    IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params* params,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    const std::vector<IndexedDBValue>& values) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(values.size(), params->values.size());

  for (size_t i = 0; i < values.size(); ++i) {
    if (!CreateAllBlobs(values[i].blob_info,
                        &params->values[i].blob_or_file_info, dispatcher_host))
      return;
  }

  dispatcher_host->Send(
      new IndexedDBMsg_CallbacksSuccessCursorPrefetch(*params));
}

static void FillInBlobData(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<IndexedDBMsg_BlobOrFileInfo>* blob_or_file_info) {
  for (const auto& iter : blob_info) {
    if (iter.is_file()) {
      IndexedDBMsg_BlobOrFileInfo info;
      info.is_file = true;
      info.mime_type = iter.type();
      info.file_name = iter.file_name();
      info.file_path = iter.file_path().AsUTF16Unsafe();
      info.size = iter.size();
      info.last_modified = iter.last_modified().ToDoubleT();
      blob_or_file_info->push_back(info);
    } else {
      IndexedDBMsg_BlobOrFileInfo info;
      info.mime_type = iter.type();
      info.size = iter.size();
      blob_or_file_info->push_back(info);
    }
  }
}

void IndexedDBCallbacks::RegisterBlobsAndSend(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    const base::Closure& callback) {
  for (const auto& iter : blob_info) {
    if (!iter.mark_used_callback().is_null())
      iter.mark_used_callback().Run();
  }
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, callback);
}

void IndexedDBCallbacks::OnSuccess(scoped_refptr<IndexedDBCursor> cursor,
                                   const IndexedDBKey& key,
                                   const IndexedDBKey& primary_key,
                                   IndexedDBValue* value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  ::indexed_db::mojom::ValuePtr mojo_value;
  std::vector<IndexedDBBlobInfo> blob_info;
  if (value) {
    mojo_value = ::indexed_db::mojom::Value::New();
    if (!value->empty())
      swap(mojo_value->bits, value->bits);
    ConvertBlobInfo(value->blob_info, &mojo_value->blob_or_file_info);
    blob_info = value->blob_info;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendSuccessCursor,
                 base::Unretained(io_helper_.get()),
                 dispatcher_host_->Add(cursor.get()), key, primary_key,
                 base::Passed(&mojo_value), base::Passed(&blob_info)));
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccess(const IndexedDBKey& key,
                                   const IndexedDBKey& primary_key,
                                   IndexedDBValue* value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(!io_helper_);

  DCHECK_NE(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  IndexedDBCursor* idb_cursor =
      dispatcher_host_->GetCursorFromId(ipc_cursor_id_);

  DCHECK(idb_cursor);
  if (!idb_cursor)
    return;

  std::unique_ptr<IndexedDBMsg_CallbacksSuccessCursorContinue_Params> params(
      new IndexedDBMsg_CallbacksSuccessCursorContinue_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->ipc_cursor_id = ipc_cursor_id_;
  params->key = key;
  params->primary_key = primary_key;
  if (value && !value->empty())
    swap(params->value.bits, value->bits);
  // TODO(alecflett): Avoid a copy here: the whole params object is
  // being copied into the message.
  if (!value || value->blob_info.empty()) {
    dispatcher_host_->Send(
        new IndexedDBMsg_CallbacksSuccessCursorContinue(*params));
  } else {
    IndexedDBMsg_CallbacksSuccessCursorContinue_Params* p = params.get();
    FillInBlobData(value->blob_info, &p->value.blob_or_file_info);
    RegisterBlobsAndSend(
        value->blob_info,
        base::Bind(CreateBlobsAndSend<
                       IndexedDBMsg_CallbacksSuccessCursorContinue_Params,
                       IndexedDBMsg_CallbacksSuccessCursorContinue>,
                   base::Owned(params.release()), dispatcher_host_,
                   value->blob_info,
                   base::Unretained(&p->value.blob_or_file_info)));
  }
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccessWithPrefetch(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    std::vector<IndexedDBValue>* values) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(!io_helper_);
  DCHECK_EQ(keys.size(), primary_keys.size());
  DCHECK_EQ(keys.size(), values->size());

  DCHECK_NE(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  std::vector<IndexedDBKey> msg_keys;
  std::vector<IndexedDBKey> msg_primary_keys;

  for (size_t i = 0; i < keys.size(); ++i) {
    msg_keys.push_back(keys[i]);
    msg_primary_keys.push_back(primary_keys[i]);
  }

  std::unique_ptr<IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params> params(
      new IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->ipc_cursor_id = ipc_cursor_id_;
  params->keys = msg_keys;
  params->primary_keys = msg_primary_keys;
  params->values.resize(values->size());

  bool found_blob_info = false;
  for (size_t i = 0; i < values->size(); ++i) {
    params->values[i].bits.swap(values->at(i).bits);
    if (!values->at(i).blob_info.empty()) {
      found_blob_info = true;
      FillInBlobData(values->at(i).blob_info,
                     &params->values[i].blob_or_file_info);
      for (const auto& blob_iter : values->at(i).blob_info) {
        if (!blob_iter.mark_used_callback().is_null())
          blob_iter.mark_used_callback().Run();
      }
    }
  }

  if (found_blob_info) {
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(BlobLookupForCursorPrefetch,
                                       base::Owned(params.release()),
                                       dispatcher_host_,
                                       *values));
  } else {
    dispatcher_host_->Send(
        new IndexedDBMsg_CallbacksSuccessCursorPrefetch(*params.get()));
  }
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccess(IndexedDBReturnValue* value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);

  if (value && value->primary_key.IsValid()) {
    DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  } else {
    DCHECK(kNoCursor == ipc_cursor_id_ || value == NULL);
  }
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  if (io_helper_) {
    DCHECK_EQ(kNoCursor, ipc_cursor_id_);

    ::indexed_db::mojom::ReturnValuePtr mojo_value;
    std::vector<IndexedDBBlobInfo> blob_info;
    if (value) {
      mojo_value = ConvertReturnValue(value);
      blob_info = value->blob_info;
    }
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&IOThreadHelper::SendSuccessValue,
                   base::Unretained(io_helper_.get()),
                   base::Passed(&mojo_value), base::Passed(&blob_info)));
  } else {
    auto params = base::MakeUnique<IndexedDBMsg_CallbacksSuccessValue_Params>();
    params->ipc_thread_id = ipc_thread_id_;
    params->ipc_callbacks_id = ipc_callbacks_id_;
    if (value && value->primary_key.IsValid()) {
      params->value.primary_key = value->primary_key;
      params->value.key_path = value->key_path;
    }
    if (value && !value->empty())
      swap(params->value.bits, value->bits);
    if (!value || value->blob_info.empty()) {
      dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessValue(*params));
    } else {
      IndexedDBMsg_CallbacksSuccessValue_Params* p = params.get();
      FillInBlobData(value->blob_info, &p->value.blob_or_file_info);
      RegisterBlobsAndSend(
          value->blob_info,
          base::Bind(
              CreateBlobsAndSend<IndexedDBMsg_CallbacksSuccessValue_Params,
                                 IndexedDBMsg_CallbacksSuccessValue>,
              base::Owned(params.release()), dispatcher_host_, value->blob_info,
              base::Unretained(&p->value.blob_or_file_info)));
    }
  }
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccessArray(
    std::vector<IndexedDBReturnValue>* values) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);

  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);
  DCHECK_EQ(kNoCursor, ipc_cursor_id_);

  std::vector<::indexed_db::mojom::ReturnValuePtr> mojo_values;
  mojo_values.reserve(values->size());
  for (size_t i = 0; i < values->size(); ++i)
    mojo_values.push_back(ConvertReturnValue(&(*values)[i]));

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&IOThreadHelper::SendSuccessArray,
                                     base::Unretained(io_helper_.get()),
                                     base::Passed(&mojo_values), *values));
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccess(const IndexedDBKey& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendSuccessKey,
                 base::Unretained(io_helper_.get()), value));
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccess(int64_t value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  if (io_helper_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&IOThreadHelper::SendSuccessInteger,
                   base::Unretained(io_helper_.get()), value));
  } else {
    DCHECK_EQ(kNoCursor, ipc_cursor_id_);
    DCHECK_EQ(kNoTransaction, host_transaction_id_);
    DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

    dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessInteger(
        ipc_thread_id_, ipc_callbacks_id_, value));
  }
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccess() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&IOThreadHelper::SendSuccess,
                                     base::Unretained(io_helper_.get())));
  dispatcher_host_ = nullptr;
}

bool IndexedDBCallbacks::IsValid() const {
  DCHECK(dispatcher_host_.get());

  return dispatcher_host_->IsOpen();
}

void IndexedDBCallbacks::SetConnectionOpenStartTime(
    const base::TimeTicks& start_time) {
  connection_open_start_time_ = start_time;
}

IndexedDBCallbacks::IOThreadHelper::IOThreadHelper(
    CallbacksAssociatedPtrInfo callbacks_info,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host)
    : dispatcher_host_(std::move(dispatcher_host)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callbacks_.Bind(std::move(callbacks_info));
}

IndexedDBCallbacks::IOThreadHelper::~IOThreadHelper() {}

void IndexedDBCallbacks::IOThreadHelper::SendError(
    const IndexedDBDatabaseError& error) {
  callbacks_->Error(error.code(), error.message());
}

void IndexedDBCallbacks::IOThreadHelper::SendSuccessStringList(
    const std::vector<base::string16>& value) {
  callbacks_->SuccessStringList(value);
}

void IndexedDBCallbacks::IOThreadHelper::SendBlocked(int64_t existing_version) {
  callbacks_->Blocked(existing_version);
}

void IndexedDBCallbacks::IOThreadHelper::SendUpgradeNeeded(
    std::unique_ptr<DatabaseImpl> database,
    int64_t old_version,
    blink::WebIDBDataLoss data_loss,
    const std::string& data_loss_message,
    const content::IndexedDBDatabaseMetadata& metadata) {
  ::indexed_db::mojom::DatabaseAssociatedPtrInfo ptr_info;
  ::indexed_db::mojom::DatabaseAssociatedRequest request;
  callbacks_.associated_group()->CreateAssociatedInterface(
      mojo::AssociatedGroup::WILL_PASS_PTR, &ptr_info, &request);
  mojo::MakeStrongAssociatedBinding(std::move(database), std::move(request));
  callbacks_->UpgradeNeeded(std::move(ptr_info), old_version, data_loss,
                            data_loss_message, metadata);
}

void IndexedDBCallbacks::IOThreadHelper::SendSuccessDatabase(
    std::unique_ptr<DatabaseImpl> database,
    const content::IndexedDBDatabaseMetadata& metadata) {
  ::indexed_db::mojom::DatabaseAssociatedPtrInfo ptr_info;
  if (database) {
    ::indexed_db::mojom::DatabaseAssociatedRequest request;
    callbacks_.associated_group()->CreateAssociatedInterface(
        mojo::AssociatedGroup::WILL_PASS_PTR, &ptr_info, &request);
    mojo::MakeStrongAssociatedBinding(std::move(database), std::move(request));
  }
  callbacks_->SuccessDatabase(std::move(ptr_info), metadata);
}

void IndexedDBCallbacks::IOThreadHelper::SendSuccessCursor(
    int32_t cursor_id,
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    ::indexed_db::mojom::ValuePtr value,
    const std::vector<IndexedDBBlobInfo>& blob_info) {
  if (!value || CreateAllBlobs(blob_info, &value->blob_or_file_info))
    callbacks_->SuccessCursor(cursor_id, key, primary_key, std::move(value));
}

void IndexedDBCallbacks::IOThreadHelper::SendSuccessValue(
    ::indexed_db::mojom::ReturnValuePtr value,
    const std::vector<IndexedDBBlobInfo>& blob_info) {
  if (!value || CreateAllBlobs(blob_info, &value->value->blob_or_file_info))
    callbacks_->SuccessValue(std::move(value));
}

void IndexedDBCallbacks::IOThreadHelper::SendSuccessArray(
    std::vector<::indexed_db::mojom::ReturnValuePtr> mojo_values,
    const std::vector<IndexedDBReturnValue>& values) {
  DCHECK_EQ(mojo_values.size(), values.size());

  for (size_t i = 0; i < mojo_values.size(); ++i) {
    if (!CreateAllBlobs(values[i].blob_info,
                        &mojo_values[i]->value->blob_or_file_info))
      return;
  }
  callbacks_->SuccessArray(std::move(mojo_values));
}

void IndexedDBCallbacks::IOThreadHelper::SendSuccessKey(
    const IndexedDBKey& value) {
  callbacks_->SuccessKey(value);
}

void IndexedDBCallbacks::IOThreadHelper::SendSuccessInteger(int64_t value) {
  callbacks_->SuccessInteger(value);
}

void IndexedDBCallbacks::IOThreadHelper::SendSuccess() {
  callbacks_->Success();
}

std::string IndexedDBCallbacks::IOThreadHelper::CreateBlobData(
    const IndexedDBBlobInfo& blob_info) {
  if (!blob_info.uuid().empty()) {
    // We're sending back a live blob, not a reference into our backing store.
    return dispatcher_host_->HoldBlobData(blob_info);
  }
  scoped_refptr<ShareableFileReference> shareable_file =
      ShareableFileReference::Get(blob_info.file_path());
  if (!shareable_file) {
    shareable_file = ShareableFileReference::GetOrCreate(
        blob_info.file_path(),
        ShareableFileReference::DONT_DELETE_ON_FINAL_RELEASE,
        dispatcher_host_->context()->TaskRunner());
    if (!blob_info.release_callback().is_null())
      shareable_file->AddFinalReleaseCallback(blob_info.release_callback());
  }
  return dispatcher_host_->HoldBlobData(blob_info);
}

bool IndexedDBCallbacks::IOThreadHelper::CreateAllBlobs(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<::indexed_db::mojom::BlobInfoPtr>* blob_or_file_info) {
  IDB_TRACE("IndexedDBCallbacks::CreateAllBlobs");
  DCHECK_EQ(blob_info.size(), blob_or_file_info->size());
  if (!dispatcher_host_->blob_storage_context())
    return false;
  for (size_t i = 0; i < blob_info.size(); ++i)
    (*blob_or_file_info)[i]->uuid = CreateBlobData(blob_info[i]);
  return true;
}

}  // namespace content
