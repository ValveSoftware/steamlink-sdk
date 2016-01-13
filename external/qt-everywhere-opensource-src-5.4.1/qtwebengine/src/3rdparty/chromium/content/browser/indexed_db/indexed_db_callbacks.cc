// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_callbacks.h"

#include <algorithm>

#include "base/guid.h"
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
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "webkit/browser/blob/blob_storage_context.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/blob/blob_data.h"
#include "webkit/common/blob/shareable_file_reference.h"

using webkit_blob::ShareableFileReference;

namespace content {

namespace {
const int32 kNoCursor = -1;
const int32 kNoDatabaseCallbacks = -1;
const int64 kNoTransaction = -1;
}

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32 ipc_thread_id,
                                       int32 ipc_callbacks_id)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(kNoCursor),
      host_transaction_id_(kNoTransaction),
      ipc_database_id_(kNoDatabase),
      ipc_database_callbacks_id_(kNoDatabaseCallbacks),
      data_loss_(blink::WebIDBDataLossNone) {}

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32 ipc_thread_id,
                                       int32 ipc_callbacks_id,
                                       int32 ipc_cursor_id)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(ipc_cursor_id),
      host_transaction_id_(kNoTransaction),
      ipc_database_id_(kNoDatabase),
      ipc_database_callbacks_id_(kNoDatabaseCallbacks),
      data_loss_(blink::WebIDBDataLossNone) {}

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32 ipc_thread_id,
                                       int32 ipc_callbacks_id,
                                       int32 ipc_database_callbacks_id,
                                       int64 host_transaction_id,
                                       const GURL& origin_url)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(kNoCursor),
      host_transaction_id_(host_transaction_id),
      origin_url_(origin_url),
      ipc_database_id_(kNoDatabase),
      ipc_database_callbacks_id_(ipc_database_callbacks_id),
      data_loss_(blink::WebIDBDataLossNone) {}

IndexedDBCallbacks::~IndexedDBCallbacks() {}

void IndexedDBCallbacks::OnError(const IndexedDBDatabaseError& error) {
  DCHECK(dispatcher_host_.get());

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksError(
      ipc_thread_id_, ipc_callbacks_id_, error.code(), error.message()));
  dispatcher_host_ = NULL;
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

void IndexedDBCallbacks::OnBlocked(int64 existing_version) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  // No transaction/db callbacks for DeleteDatabase.
  DCHECK_EQ(kNoTransaction == host_transaction_id_,
            kNoDatabaseCallbacks == ipc_database_callbacks_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksIntBlocked(
      ipc_thread_id_, ipc_callbacks_id_, existing_version));
}

void IndexedDBCallbacks::OnDataLoss(blink::WebIDBDataLoss data_loss,
                                    std::string data_loss_message) {
  DCHECK_NE(blink::WebIDBDataLossNone, data_loss);
  data_loss_ = data_loss;
  data_loss_message_ = data_loss_message;
}

void IndexedDBCallbacks::OnUpgradeNeeded(
    int64 old_version,
    scoped_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_NE(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_NE(kNoDatabaseCallbacks, ipc_database_callbacks_id_);

  dispatcher_host_->RegisterTransactionId(host_transaction_id_, origin_url_);
  int32 ipc_database_id =
      dispatcher_host_->Add(connection.release(), ipc_thread_id_, origin_url_);
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
}

void IndexedDBCallbacks::OnSuccess(scoped_ptr<IndexedDBConnection> connection,
                                   const IndexedDBDatabaseMetadata& metadata) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_NE(kNoTransaction, host_transaction_id_);
  DCHECK_NE(ipc_database_id_ == kNoDatabase, !connection);
  DCHECK_NE(kNoDatabaseCallbacks, ipc_database_callbacks_id_);

  scoped_refptr<IndexedDBCallbacks> self(this);

  int32 ipc_object_id = kNoDatabase;
  // Only register if the connection was not previously sent in OnUpgradeNeeded.
  if (ipc_database_id_ == kNoDatabase) {
    ipc_object_id = dispatcher_host_->Add(
        connection.release(), ipc_thread_id_, origin_url_);
  }

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessIDBDatabase(
      ipc_thread_id_,
      ipc_callbacks_id_,
      ipc_database_callbacks_id_,
      ipc_object_id,
      IndexedDBDispatcherHost::ConvertMetadata(metadata)));
  dispatcher_host_ = NULL;
}

static std::string CreateBlobData(
    const IndexedDBBlobInfo& blob_info,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    webkit_blob::BlobStorageContext* blob_storage_context,
    base::TaskRunner* task_runner) {
  std::string uuid = blob_info.uuid();
  if (!uuid.empty()) {
    // We're sending back a live blob, not a reference into our backing store.
    scoped_ptr<webkit_blob::BlobDataHandle> blob_data_handle(
        blob_storage_context->GetBlobDataFromUUID(uuid));
    dispatcher_host->HoldBlobDataHandle(uuid, blob_data_handle);
    return uuid;
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

  uuid = base::GenerateGUID();
  scoped_refptr<webkit_blob::BlobData> blob_data =
      new webkit_blob::BlobData(uuid);
  blob_data->AppendFile(
      blob_info.file_path(), 0, blob_info.size(), blob_info.last_modified());
  scoped_ptr<webkit_blob::BlobDataHandle> blob_data_handle(
      blob_storage_context->AddFinishedBlob(blob_data.get()));
  dispatcher_host->HoldBlobDataHandle(uuid, blob_data_handle);

  return uuid;
}

static bool CreateAllBlobs(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<IndexedDBMsg_BlobOrFileInfo>* blob_or_file_info,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host) {
  DCHECK_EQ(blob_info.size(), blob_or_file_info->size());
  size_t i;
  if (!dispatcher_host->blob_storage_context())
    return false;
  for (i = 0; i < blob_info.size(); ++i) {
    (*blob_or_file_info)[i].uuid =
        CreateBlobData(blob_info[i],
                       dispatcher_host,
                       dispatcher_host->blob_storage_context(),
                       dispatcher_host->Context()->TaskRunner());
  }
  return true;
}

template <class ParamType, class MsgType>
static void CreateBlobsAndSend(
    ParamType* params,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<IndexedDBMsg_BlobOrFileInfo>* blob_or_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (CreateAllBlobs(blob_info, blob_or_file_info, dispatcher_host))
    dispatcher_host->Send(new MsgType(*params));
}

static void BlobLookupForCursorPrefetch(
    IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params* params,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    const std::vector<IndexedDBValue>& values) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(values.size(), params->blob_or_file_infos.size());

  std::vector<IndexedDBValue>::const_iterator value_iter;
  std::vector<std::vector<IndexedDBMsg_BlobOrFileInfo> >::iterator blob_iter;
  for (value_iter = values.begin(), blob_iter =
       params->blob_or_file_infos.begin(); value_iter != values.end();
       ++value_iter, ++blob_iter) {
    if (!CreateAllBlobs(value_iter->blob_info, &*blob_iter, dispatcher_host))
      return;
  }
  dispatcher_host->Send(
      new IndexedDBMsg_CallbacksSuccessCursorPrefetch(*params));
}

static void FillInBlobData(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<IndexedDBMsg_BlobOrFileInfo>* blob_or_file_info) {
  for (std::vector<IndexedDBBlobInfo>::const_iterator iter = blob_info.begin();
       iter != blob_info.end();
       ++iter) {
    if (iter->is_file()) {
      IndexedDBMsg_BlobOrFileInfo info;
      info.is_file = true;
      info.mime_type = iter->type();
      info.file_name = iter->file_name();
      info.file_path = iter->file_path().AsUTF16Unsafe();
      info.size = iter->size();
      info.last_modified = iter->last_modified().ToDoubleT();
      blob_or_file_info->push_back(info);
    } else {
      IndexedDBMsg_BlobOrFileInfo info;
      info.mime_type = iter->type();
      info.size = iter->size();
      blob_or_file_info->push_back(info);
    }
  }
}

void IndexedDBCallbacks::RegisterBlobsAndSend(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    const base::Closure& callback) {
  std::vector<IndexedDBBlobInfo>::const_iterator iter;
  for (iter = blob_info.begin(); iter != blob_info.end(); ++iter) {
    if (!iter->mark_used_callback().is_null())
      iter->mark_used_callback().Run();
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

  int32 ipc_object_id = dispatcher_host_->Add(cursor.get());
  scoped_ptr<IndexedDBMsg_CallbacksSuccessIDBCursor_Params> params(
      new IndexedDBMsg_CallbacksSuccessIDBCursor_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->ipc_cursor_id = ipc_object_id;
  params->key = key;
  params->primary_key = primary_key;
  if (value && !value->empty())
    std::swap(params->value, value->bits);
  // TODO(alecflett): Avoid a copy here: the whole params object is
  // being copied into the message.
  if (!value || value->blob_info.empty()) {
    dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessIDBCursor(*params));
  } else {
    IndexedDBMsg_CallbacksSuccessIDBCursor_Params* p = params.get();
    FillInBlobData(value->blob_info, &p->blob_or_file_info);
    RegisterBlobsAndSend(
        value->blob_info,
        base::Bind(
            CreateBlobsAndSend<IndexedDBMsg_CallbacksSuccessIDBCursor_Params,
                               IndexedDBMsg_CallbacksSuccessIDBCursor>,
            base::Owned(params.release()),
            dispatcher_host_,
            value->blob_info,
            base::Unretained(&p->blob_or_file_info)));
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

  scoped_ptr<IndexedDBMsg_CallbacksSuccessCursorContinue_Params> params(
      new IndexedDBMsg_CallbacksSuccessCursorContinue_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->ipc_cursor_id = ipc_cursor_id_;
  params->key = key;
  params->primary_key = primary_key;
  if (value && !value->empty())
    std::swap(params->value, value->bits);
  // TODO(alecflett): Avoid a copy here: the whole params object is
  // being copied into the message.
  if (!value || value->blob_info.empty()) {
    dispatcher_host_->Send(
        new IndexedDBMsg_CallbacksSuccessCursorContinue(*params));
  } else {
    IndexedDBMsg_CallbacksSuccessCursorContinue_Params* p = params.get();
    FillInBlobData(value->blob_info, &p->blob_or_file_info);
    RegisterBlobsAndSend(
        value->blob_info,
        base::Bind(CreateBlobsAndSend<
                       IndexedDBMsg_CallbacksSuccessCursorContinue_Params,
                       IndexedDBMsg_CallbacksSuccessCursorContinue>,
                   base::Owned(params.release()),
                   dispatcher_host_,
                   value->blob_info,
                   base::Unretained(&p->blob_or_file_info)));
  }
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccessWithPrefetch(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    std::vector<IndexedDBValue>& values) {
  DCHECK_EQ(keys.size(), primary_keys.size());
  DCHECK_EQ(keys.size(), values.size());

  DCHECK(dispatcher_host_.get());

  DCHECK_NE(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  std::vector<IndexedDBKey> msgKeys;
  std::vector<IndexedDBKey> msgPrimaryKeys;

  for (size_t i = 0; i < keys.size(); ++i) {
    msgKeys.push_back(keys[i]);
    msgPrimaryKeys.push_back(primary_keys[i]);
  }

  scoped_ptr<IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params> params(
      new IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->ipc_cursor_id = ipc_cursor_id_;
  params->keys = msgKeys;
  params->primary_keys = msgPrimaryKeys;
  std::vector<std::string>& values_bits = params->values;
  values_bits.resize(values.size());
  std::vector<std::vector<IndexedDBMsg_BlobOrFileInfo> >& values_blob_infos =
      params->blob_or_file_infos;
  values_blob_infos.resize(values.size());

  bool found_blob_info = false;
  std::vector<IndexedDBValue>::iterator iter = values.begin();
  for (size_t i = 0; iter != values.end(); ++iter, ++i) {
    values_bits[i].swap(iter->bits);
    if (iter->blob_info.size()) {
      found_blob_info = true;
      FillInBlobData(iter->blob_info, &values_blob_infos[i]);
      std::vector<IndexedDBBlobInfo>::const_iterator blob_iter;
      for (blob_iter = iter->blob_info.begin();
           blob_iter != iter->blob_info.end();
           ++blob_iter) {
        if (!blob_iter->mark_used_callback().is_null())
          blob_iter->mark_used_callback().Run();
      }
    }
  }

  if (found_blob_info) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(BlobLookupForCursorPrefetch,
                   base::Owned(params.release()),
                   dispatcher_host_,
                   values));
  } else {
    dispatcher_host_->Send(
        new IndexedDBMsg_CallbacksSuccessCursorPrefetch(*params.get()));
  }
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(IndexedDBValue* value,
                                   const IndexedDBKey& key,
                                   const IndexedDBKeyPath& key_path) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  scoped_ptr<IndexedDBMsg_CallbacksSuccessValueWithKey_Params> params(
      new IndexedDBMsg_CallbacksSuccessValueWithKey_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->primary_key = key;
  params->key_path = key_path;
  if (value && !value->empty())
    std::swap(params->value, value->bits);
  if (!value || value->blob_info.empty()) {
    dispatcher_host_->Send(
        new IndexedDBMsg_CallbacksSuccessValueWithKey(*params));
  } else {
    IndexedDBMsg_CallbacksSuccessValueWithKey_Params* p = params.get();
    FillInBlobData(value->blob_info, &p->blob_or_file_info);
    RegisterBlobsAndSend(
        value->blob_info,
        base::Bind(
            CreateBlobsAndSend<IndexedDBMsg_CallbacksSuccessValueWithKey_Params,
                               IndexedDBMsg_CallbacksSuccessValueWithKey>,
            base::Owned(params.release()),
            dispatcher_host_,
            value->blob_info,
            base::Unretained(&p->blob_or_file_info)));
  }
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(IndexedDBValue* value) {
  DCHECK(dispatcher_host_.get());
  DCHECK(kNoCursor == ipc_cursor_id_ || value == NULL);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  scoped_ptr<IndexedDBMsg_CallbacksSuccessValue_Params> params(
      new IndexedDBMsg_CallbacksSuccessValue_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  if (value && !value->empty())
    std::swap(params->value, value->bits);
  if (!value || value->blob_info.empty()) {
    dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessValue(*params));
  } else {
    IndexedDBMsg_CallbacksSuccessValue_Params* p = params.get();
    FillInBlobData(value->blob_info, &p->blob_or_file_info);
    RegisterBlobsAndSend(
        value->blob_info,
        base::Bind(CreateBlobsAndSend<IndexedDBMsg_CallbacksSuccessValue_Params,
                                      IndexedDBMsg_CallbacksSuccessValue>,
                   base::Owned(params.release()),
                   dispatcher_host_,
                   value->blob_info,
                   base::Unretained(&p->blob_or_file_info)));
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

void IndexedDBCallbacks::OnSuccess(int64 value) {
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

}  // namespace content
