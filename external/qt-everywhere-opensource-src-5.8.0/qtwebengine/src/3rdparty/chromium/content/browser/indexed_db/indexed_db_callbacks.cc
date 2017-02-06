// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_callbacks.h"

#include <stddef.h>

#include <algorithm>

#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/fileapi_message_filter.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/browser/indexed_db/indexed_db_return_value.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/quota/quota_manager.h"

using storage::ShareableFileReference;

namespace content {

namespace {
const int32_t kNoCursor = -1;
const int32_t kNoDatabaseCallbacks = -1;
const int64_t kNoTransaction = -1;
}

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32_t ipc_thread_id,
                                       int32_t ipc_callbacks_id)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(kNoCursor),
      host_transaction_id_(kNoTransaction),
      ipc_database_id_(kNoDatabase),
      ipc_database_callbacks_id_(kNoDatabaseCallbacks),
      data_loss_(blink::WebIDBDataLossNone),
      sent_blocked_(false) {}

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32_t ipc_thread_id,
                                       int32_t ipc_callbacks_id,
                                       int32_t ipc_cursor_id)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(ipc_cursor_id),
      host_transaction_id_(kNoTransaction),
      ipc_database_id_(kNoDatabase),
      ipc_database_callbacks_id_(kNoDatabaseCallbacks),
      data_loss_(blink::WebIDBDataLossNone),
      sent_blocked_(false) {}

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32_t ipc_thread_id,
                                       int32_t ipc_callbacks_id,
                                       int32_t ipc_database_callbacks_id,
                                       int64_t host_transaction_id,
                                       const url::Origin& origin)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(kNoCursor),
      host_transaction_id_(host_transaction_id),
      origin_(origin),
      ipc_database_id_(kNoDatabase),
      ipc_database_callbacks_id_(ipc_database_callbacks_id),
      data_loss_(blink::WebIDBDataLossNone),
      sent_blocked_(false) {}

IndexedDBCallbacks::~IndexedDBCallbacks() {}

void IndexedDBCallbacks::OnError(const IndexedDBDatabaseError& error) {
  DCHECK(dispatcher_host_.get());

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksError(
      ipc_thread_id_, ipc_callbacks_id_, error.code(), error.message()));
  dispatcher_host_ = NULL;

  if (!connection_open_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebCore.IndexedDB.OpenTime.Error",
        base::TimeTicks::Now() - connection_open_start_time_);
    connection_open_start_time_ = base::TimeTicks();
  }
}

void IndexedDBCallbacks::OnSuccess(const std::vector<base::string16>& value) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  std::vector<base::string16> list;
  for (unsigned i = 0; i < value.size(); ++i)
    list.push_back(value[i]);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessStringList(
      ipc_thread_id_, ipc_callbacks_id_, list));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnBlocked(int64_t existing_version) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  // No transaction/db callbacks for DeleteDatabase.
  DCHECK_EQ(kNoTransaction == host_transaction_id_,
            kNoDatabaseCallbacks == ipc_database_callbacks_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);

  if (sent_blocked_)
    return;

  sent_blocked_ = true;
  dispatcher_host_->Send(new IndexedDBMsg_CallbacksIntBlocked(
      ipc_thread_id_, ipc_callbacks_id_, existing_version));

  if (!connection_open_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebCore.IndexedDB.OpenTime.Blocked",
        base::TimeTicks::Now() - connection_open_start_time_);
    connection_open_start_time_ = base::TimeTicks();
  }
}

void IndexedDBCallbacks::OnDataLoss(blink::WebIDBDataLoss data_loss,
                                    std::string data_loss_message) {
  DCHECK_NE(blink::WebIDBDataLossNone, data_loss);
  data_loss_ = data_loss;
  data_loss_message_ = data_loss_message;
}

void IndexedDBCallbacks::OnUpgradeNeeded(
    int64_t old_version,
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_NE(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_NE(kNoDatabaseCallbacks, ipc_database_callbacks_id_);

  dispatcher_host_->RegisterTransactionId(host_transaction_id_, origin_);
  int32_t ipc_database_id =
      dispatcher_host_->Add(connection.release(), ipc_thread_id_, origin_);
  if (ipc_database_id < 0)
    return;
  ipc_database_id_ = ipc_database_id;
  IndexedDBMsg_CallbacksUpgradeNeeded_Params params;
  params.ipc_thread_id = ipc_thread_id_;
  params.ipc_callbacks_id = ipc_callbacks_id_;
  params.ipc_database_id = ipc_database_id;
  params.ipc_database_callbacks_id = ipc_database_callbacks_id_;
  params.old_version = old_version;
  params.idb_metadata = IndexedDBDispatcherHost::ConvertMetadata(metadata);
  params.data_loss = data_loss_;
  params.data_loss_message = data_loss_message_;
  dispatcher_host_->Send(new IndexedDBMsg_CallbacksUpgradeNeeded(params));

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
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_NE(kNoTransaction, host_transaction_id_);
  DCHECK_NE(ipc_database_id_ == kNoDatabase, !connection);
  DCHECK_NE(kNoDatabaseCallbacks, ipc_database_callbacks_id_);

  scoped_refptr<IndexedDBCallbacks> self(this);

  int32_t ipc_object_id = kNoDatabase;
  // Only register if the connection was not previously sent in OnUpgradeNeeded.
  if (ipc_database_id_ == kNoDatabase) {
    ipc_object_id =
        dispatcher_host_->Add(connection.release(), ipc_thread_id_, origin_);
  }

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessIDBDatabase(
      ipc_thread_id_,
      ipc_callbacks_id_,
      ipc_database_callbacks_id_,
      ipc_object_id,
      IndexedDBDispatcherHost::ConvertMetadata(metadata)));
  dispatcher_host_ = NULL;

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

static void BlobLookupForGetAll(
    IndexedDBMsg_CallbacksSuccessArray_Params* params,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    const std::vector<IndexedDBReturnValue>& values) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(values.size(), params->values.size());

  for (size_t i = 0; i < values.size(); ++i) {
    if (!CreateAllBlobs(values[i].blob_info,
                        &params->values[i].blob_or_file_info, dispatcher_host))
      return;
  }

  dispatcher_host->Send(new IndexedDBMsg_CallbacksSuccessArray(*params));
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
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  int32_t ipc_object_id = dispatcher_host_->Add(cursor.get());
  std::unique_ptr<IndexedDBMsg_CallbacksSuccessIDBCursor_Params> params(
      new IndexedDBMsg_CallbacksSuccessIDBCursor_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->ipc_cursor_id = ipc_object_id;
  params->key = key;
  params->primary_key = primary_key;
  if (value && !value->empty())
    std::swap(params->value.bits, value->bits);
  // TODO(alecflett): Avoid a copy here: the whole params object is
  // being copied into the message.
  if (!value || value->blob_info.empty()) {
    dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessIDBCursor(*params));
  } else {
    IndexedDBMsg_CallbacksSuccessIDBCursor_Params* p = params.get();
    FillInBlobData(value->blob_info, &p->value.blob_or_file_info);
    RegisterBlobsAndSend(
        value->blob_info,
        base::Bind(
            CreateBlobsAndSend<IndexedDBMsg_CallbacksSuccessIDBCursor_Params,
                               IndexedDBMsg_CallbacksSuccessIDBCursor>,
            base::Owned(params.release()), dispatcher_host_, value->blob_info,
            base::Unretained(&p->value.blob_or_file_info)));
  }
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(const IndexedDBKey& key,
                                   const IndexedDBKey& primary_key,
                                   IndexedDBValue* value) {
  DCHECK(dispatcher_host_.get());

  DCHECK_NE(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
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
    std::swap(params->value.bits, value->bits);
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
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccessWithPrefetch(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    std::vector<IndexedDBValue>* values) {
  DCHECK_EQ(keys.size(), primary_keys.size());
  DCHECK_EQ(keys.size(), values->size());

  DCHECK(dispatcher_host_.get());

  DCHECK_NE(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
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
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(IndexedDBReturnValue* value) {
  DCHECK(dispatcher_host_.get());

  if (value && value->primary_key.IsValid()) {
    DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  } else {
    DCHECK(kNoCursor == ipc_cursor_id_ || value == NULL);
  }
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  std::unique_ptr<IndexedDBMsg_CallbacksSuccessValue_Params> params(
      new IndexedDBMsg_CallbacksSuccessValue_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  if (value && value->primary_key.IsValid()) {
    params->value.primary_key = value->primary_key;
    params->value.key_path = value->key_path;
  }
  if (value && !value->empty())
    std::swap(params->value.bits, value->bits);
  if (!value || value->blob_info.empty()) {
    dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessValue(*params));
  } else {
    IndexedDBMsg_CallbacksSuccessValue_Params* p = params.get();
    FillInBlobData(value->blob_info, &p->value.blob_or_file_info);
    RegisterBlobsAndSend(
        value->blob_info,
        base::Bind(CreateBlobsAndSend<IndexedDBMsg_CallbacksSuccessValue_Params,
                                      IndexedDBMsg_CallbacksSuccessValue>,
                   base::Owned(params.release()), dispatcher_host_,
                   value->blob_info,
                   base::Unretained(&p->value.blob_or_file_info)));
  }
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccessArray(
    std::vector<IndexedDBReturnValue>* values,
    const IndexedDBKeyPath& key_path) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  std::unique_ptr<IndexedDBMsg_CallbacksSuccessArray_Params> params(
      new IndexedDBMsg_CallbacksSuccessArray_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->values.resize(values->size());

  bool found_blob_info = false;
  for (size_t i = 0; i < values->size(); ++i) {
    IndexedDBMsg_ReturnValue& pvalue = params->values[i];
    IndexedDBReturnValue& value = (*values)[i];
    pvalue.bits.swap(value.bits);
    if (!value.blob_info.empty()) {
      found_blob_info = true;
      FillInBlobData(value.blob_info, &pvalue.blob_or_file_info);
      for (const auto& blob_info : value.blob_info) {
        if (!blob_info.mark_used_callback().is_null())
          blob_info.mark_used_callback().Run();
      }
    }
    pvalue.primary_key = value.primary_key;
    pvalue.key_path = key_path;
  }

  if (found_blob_info) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(BlobLookupForGetAll, base::Owned(params.release()),
                   dispatcher_host_, *values));
  } else {
    dispatcher_host_->Send(
        new IndexedDBMsg_CallbacksSuccessArray(*params.get()));
  }
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(const IndexedDBKey& value) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessIndexedDBKey(
      ipc_thread_id_, ipc_callbacks_id_, value));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(int64_t value) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessInteger(
      ipc_thread_id_, ipc_callbacks_id_, value));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess() {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessUndefined(
      ipc_thread_id_, ipc_callbacks_id_));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::SetConnectionOpenStartTime(
    const base::TimeTicks& start_time) {
  connection_open_start_time_ = start_time;
}

}  // namespace content
