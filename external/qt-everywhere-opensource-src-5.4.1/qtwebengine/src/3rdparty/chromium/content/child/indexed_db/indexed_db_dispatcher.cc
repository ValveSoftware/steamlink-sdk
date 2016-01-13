// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_dispatcher.h"

#include <utility>

#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_local.h"
#include "content/child/indexed_db/indexed_db_key_builders.h"
#include "content/child/indexed_db/webidbcursor_impl.h"
#include "content/child/indexed_db/webidbdatabase_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseError.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseException.h"

using blink::WebBlobInfo;
using blink::WebData;
using blink::WebIDBCallbacks;
using blink::WebIDBCursor;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseCallbacks;
using blink::WebIDBDatabaseError;
using blink::WebIDBKey;
using blink::WebIDBMetadata;
using blink::WebString;
using blink::WebVector;
using base::ThreadLocalPointer;

namespace content {
static base::LazyInstance<ThreadLocalPointer<IndexedDBDispatcher> >::Leaky
    g_idb_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

namespace {

IndexedDBDispatcher* const kHasBeenDeleted =
    reinterpret_cast<IndexedDBDispatcher*>(0x1);

}  // unnamed namespace

const size_t kMaxIDBMessageOverhead = 1024 * 1024;  // 1MB; arbitrarily chosen.
const size_t kMaxIDBValueSizeInBytes =
    IPC::Channel::kMaximumMessageSize - kMaxIDBMessageOverhead;

IndexedDBDispatcher::IndexedDBDispatcher(ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender) {
  g_idb_dispatcher_tls.Pointer()->Set(this);
}

IndexedDBDispatcher::~IndexedDBDispatcher() {
  // Clear any pending callbacks - which may result in dispatch requests -
  // before marking the dispatcher as deleted.
  pending_callbacks_.Clear();
  pending_database_callbacks_.Clear();

  DCHECK(pending_callbacks_.IsEmpty());
  DCHECK(pending_database_callbacks_.IsEmpty());

  g_idb_dispatcher_tls.Pointer()->Set(kHasBeenDeleted);
}

IndexedDBDispatcher* IndexedDBDispatcher::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender) {
  if (g_idb_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS IndexedDBDispatcher.";
    g_idb_dispatcher_tls.Pointer()->Set(NULL);
  }
  if (g_idb_dispatcher_tls.Pointer()->Get())
    return g_idb_dispatcher_tls.Pointer()->Get();

  IndexedDBDispatcher* dispatcher = new IndexedDBDispatcher(thread_safe_sender);
  if (WorkerTaskRunner::Instance()->CurrentWorkerId())
    WorkerTaskRunner::Instance()->AddStopObserver(dispatcher);
  return dispatcher;
}

void IndexedDBDispatcher::OnWorkerRunLoopStopped() { delete this; }

WebIDBMetadata IndexedDBDispatcher::ConvertMetadata(
    const IndexedDBDatabaseMetadata& idb_metadata) {
  WebIDBMetadata web_metadata;
  web_metadata.id = idb_metadata.id;
  web_metadata.name = idb_metadata.name;
  web_metadata.version = idb_metadata.version;
  web_metadata.intVersion = idb_metadata.int_version;
  web_metadata.maxObjectStoreId = idb_metadata.max_object_store_id;
  web_metadata.objectStores =
      WebVector<WebIDBMetadata::ObjectStore>(idb_metadata.object_stores.size());

  for (size_t i = 0; i < idb_metadata.object_stores.size(); ++i) {
    const IndexedDBObjectStoreMetadata& idb_store_metadata =
        idb_metadata.object_stores[i];
    WebIDBMetadata::ObjectStore& web_store_metadata =
        web_metadata.objectStores[i];

    web_store_metadata.id = idb_store_metadata.id;
    web_store_metadata.name = idb_store_metadata.name;
    web_store_metadata.keyPath =
        WebIDBKeyPathBuilder::Build(idb_store_metadata.keyPath);
    web_store_metadata.autoIncrement = idb_store_metadata.autoIncrement;
    web_store_metadata.maxIndexId = idb_store_metadata.max_index_id;
    web_store_metadata.indexes =
        WebVector<WebIDBMetadata::Index>(idb_store_metadata.indexes.size());

    for (size_t j = 0; j < idb_store_metadata.indexes.size(); ++j) {
      const IndexedDBIndexMetadata& idb_index_metadata =
          idb_store_metadata.indexes[j];
      WebIDBMetadata::Index& web_index_metadata = web_store_metadata.indexes[j];

      web_index_metadata.id = idb_index_metadata.id;
      web_index_metadata.name = idb_index_metadata.name;
      web_index_metadata.keyPath =
          WebIDBKeyPathBuilder::Build(idb_index_metadata.keyPath);
      web_index_metadata.unique = idb_index_metadata.unique;
      web_index_metadata.multiEntry = idb_index_metadata.multiEntry;
    }
  }

  return web_metadata;
}

void IndexedDBDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IndexedDBDispatcher, msg)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBCursor,
                        OnSuccessOpenCursor)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessCursorAdvance,
                        OnSuccessCursorContinue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessCursorContinue,
                        OnSuccessCursorContinue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessCursorPrefetch,
                        OnSuccessCursorPrefetch)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBDatabase,
                        OnSuccessIDBDatabase)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIndexedDBKey,
                        OnSuccessIndexedDBKey)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessStringList,
                        OnSuccessStringList)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessValue, OnSuccessValue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessValueWithKey,
                        OnSuccessValueWithKey)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessInteger, OnSuccessInteger)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessUndefined,
                        OnSuccessUndefined)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksError, OnError)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksIntBlocked, OnIntBlocked)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksUpgradeNeeded, OnUpgradeNeeded)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksForcedClose,
                        OnForcedClose)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksIntVersionChange,
                        OnIntVersionChange)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksAbort, OnAbort)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksComplete, OnComplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // If a message gets here, IndexedDBMessageFilter already determined that it
  // is an IndexedDB message.
  DCHECK(handled) << "Didn't handle a message defined at line "
                  << IPC_MESSAGE_ID_LINE(msg.type());
}

bool IndexedDBDispatcher::Send(IPC::Message* msg) {
  return thread_safe_sender_->Send(msg);
}

void IndexedDBDispatcher::RequestIDBCursorAdvance(
    unsigned long count,
    WebIDBCallbacks* callbacks_ptr,
    int32 ipc_cursor_id,
    int64 transaction_id) {
  // Reset all cursor prefetch caches except for this cursor.
  ResetCursorPrefetchCaches(transaction_id, ipc_cursor_id);

  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorAdvance(
      ipc_cursor_id, CurrentWorkerId(), ipc_callbacks_id, count));
}

void IndexedDBDispatcher::RequestIDBCursorContinue(
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    WebIDBCallbacks* callbacks_ptr,
    int32 ipc_cursor_id,
    int64 transaction_id) {
  // Reset all cursor prefetch caches except for this cursor.
  ResetCursorPrefetchCaches(transaction_id, ipc_cursor_id);

  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorContinue(
      ipc_cursor_id, CurrentWorkerId(), ipc_callbacks_id, key, primary_key));
}

void IndexedDBDispatcher::RequestIDBCursorPrefetch(
    int n,
    WebIDBCallbacks* callbacks_ptr,
    int32 ipc_cursor_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorPrefetch(
      ipc_cursor_id, CurrentWorkerId(), ipc_callbacks_id, n));
}

void IndexedDBDispatcher::RequestIDBCursorPrefetchReset(int used_prefetches,
                                                        int unused_prefetches,
                                                        int32 ipc_cursor_id) {
  Send(new IndexedDBHostMsg_CursorPrefetchReset(
      ipc_cursor_id, used_prefetches, unused_prefetches));
}

void IndexedDBDispatcher::RequestIDBFactoryOpen(
    const base::string16& name,
    int64 version,
    int64 transaction_id,
    WebIDBCallbacks* callbacks_ptr,
    WebIDBDatabaseCallbacks* database_callbacks_ptr,
    const std::string& database_identifier) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  scoped_ptr<WebIDBDatabaseCallbacks> database_callbacks(
      database_callbacks_ptr);

  IndexedDBHostMsg_FactoryOpen_Params params;
  params.ipc_thread_id = CurrentWorkerId();
  params.ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  params.ipc_database_callbacks_id =
      pending_database_callbacks_.Add(database_callbacks.release());
  params.database_identifier = database_identifier;
  params.name = name;
  params.transaction_id = transaction_id;
  params.version = version;
  Send(new IndexedDBHostMsg_FactoryOpen(params));
}

void IndexedDBDispatcher::RequestIDBFactoryGetDatabaseNames(
    WebIDBCallbacks* callbacks_ptr,
    const std::string& database_identifier) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  IndexedDBHostMsg_FactoryGetDatabaseNames_Params params;
  params.ipc_thread_id = CurrentWorkerId();
  params.ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  params.database_identifier = database_identifier;
  Send(new IndexedDBHostMsg_FactoryGetDatabaseNames(params));
}

void IndexedDBDispatcher::RequestIDBFactoryDeleteDatabase(
    const base::string16& name,
    WebIDBCallbacks* callbacks_ptr,
    const std::string& database_identifier) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  IndexedDBHostMsg_FactoryDeleteDatabase_Params params;
  params.ipc_thread_id = CurrentWorkerId();
  params.ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  params.database_identifier = database_identifier;
  params.name = name;
  Send(new IndexedDBHostMsg_FactoryDeleteDatabase(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseClose(
    int32 ipc_database_id,
    int32 ipc_database_callbacks_id) {
  Send(new IndexedDBHostMsg_DatabaseClose(ipc_database_id));
  // There won't be pending database callbacks if the transaction was aborted in
  // the initial upgradeneeded event handler.
  if (pending_database_callbacks_.Lookup(ipc_database_callbacks_id))
    pending_database_callbacks_.Remove(ipc_database_callbacks_id);
}

void IndexedDBDispatcher::RequestIDBDatabaseCreateTransaction(
    int32 ipc_database_id,
    int64 transaction_id,
    WebIDBDatabaseCallbacks* database_callbacks_ptr,
    WebVector<long long> object_store_ids,
    WebIDBDatabase::TransactionMode mode) {
  scoped_ptr<WebIDBDatabaseCallbacks> database_callbacks(
      database_callbacks_ptr);
  IndexedDBHostMsg_DatabaseCreateTransaction_Params params;
  params.ipc_thread_id = CurrentWorkerId();
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.ipc_database_callbacks_id =
      pending_database_callbacks_.Add(database_callbacks.release());
  params.object_store_ids
      .assign(object_store_ids.data(),
              object_store_ids.data() + object_store_ids.size());
  params.mode = mode;

  Send(new IndexedDBHostMsg_DatabaseCreateTransaction(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseGet(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& key_range,
    bool key_only,
    WebIDBCallbacks* callbacks) {
  ResetCursorPrefetchCaches(transaction_id, kAllCursors);
  IndexedDBHostMsg_DatabaseGet_Params params;
  init_params(&params, callbacks);
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.index_id = index_id;
  params.key_range = key_range;
  params.key_only = key_only;
  Send(new IndexedDBHostMsg_DatabaseGet(params));
}

void IndexedDBDispatcher::RequestIDBDatabasePut(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    const WebData& value,
    const blink::WebVector<WebBlobInfo>& web_blob_info,
    const IndexedDBKey& key,
    WebIDBDatabase::PutMode put_mode,
    WebIDBCallbacks* callbacks,
    const WebVector<long long>& index_ids,
    const WebVector<WebVector<WebIDBKey> >& index_keys) {

  if (value.size() + key.size_estimate() > kMaxIDBValueSizeInBytes) {
    callbacks->onError(WebIDBDatabaseError(
        blink::WebIDBDatabaseExceptionUnknownError,
        WebString::fromUTF8(base::StringPrintf(
            "The serialized value is too large"
            " (size=%" PRIuS " bytes, max=%" PRIuS " bytes).",
            value.size(),
            kMaxIDBValueSizeInBytes).c_str())));
    return;
  }

  ResetCursorPrefetchCaches(transaction_id, kAllCursors);
  IndexedDBHostMsg_DatabasePut_Params params;
  init_params(&params, callbacks);
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;

  params.value.assign(value.data(), value.data() + value.size());
  params.key = key;
  params.put_mode = put_mode;

  DCHECK_EQ(index_ids.size(), index_keys.size());
  params.index_keys.resize(index_ids.size());
  for (size_t i = 0, len = index_ids.size(); i < len; ++i) {
    params.index_keys[i].first = index_ids[i];
    params.index_keys[i].second.resize(index_keys[i].size());
    for (size_t j = 0; j < index_keys[i].size(); ++j) {
      params.index_keys[i].second[j] =
          IndexedDBKey(IndexedDBKeyBuilder::Build(index_keys[i][j]));
    }
  }

  params.blob_or_file_info.resize(web_blob_info.size());
  for (size_t i = 0; i < web_blob_info.size(); ++i) {
    const WebBlobInfo& info = web_blob_info[i];
    IndexedDBMsg_BlobOrFileInfo& blob_or_file_info =
        params.blob_or_file_info[i];
    blob_or_file_info.is_file = info.isFile();
    if (info.isFile()) {
      blob_or_file_info.file_path = info.filePath();
      blob_or_file_info.file_name = info.fileName();
      blob_or_file_info.last_modified = info.lastModified();
    }
    blob_or_file_info.size = info.size();
    blob_or_file_info.uuid = info.uuid().latin1();
    DCHECK(blob_or_file_info.uuid.size());
    blob_or_file_info.mime_type = info.type();
  }

  Send(new IndexedDBHostMsg_DatabasePut(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseOpenCursor(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& key_range,
    WebIDBCursor::Direction direction,
    bool key_only,
    WebIDBDatabase::TaskType task_type,
    WebIDBCallbacks* callbacks) {
  ResetCursorPrefetchCaches(transaction_id, kAllCursors);
  IndexedDBHostMsg_DatabaseOpenCursor_Params params;
  init_params(&params, callbacks);
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.index_id = index_id;
  params.key_range = key_range;
  params.direction = direction;
  params.key_only = key_only;
  params.task_type = task_type;
  Send(new IndexedDBHostMsg_DatabaseOpenCursor(params));

  DCHECK(cursor_transaction_ids_.find(params.ipc_callbacks_id) ==
         cursor_transaction_ids_.end());
  cursor_transaction_ids_[params.ipc_callbacks_id] = transaction_id;
}

void IndexedDBDispatcher::RequestIDBDatabaseCount(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& key_range,
    WebIDBCallbacks* callbacks) {
  ResetCursorPrefetchCaches(transaction_id, kAllCursors);
  IndexedDBHostMsg_DatabaseCount_Params params;
  init_params(&params, callbacks);
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.index_id = index_id;
  params.key_range = key_range;
  Send(new IndexedDBHostMsg_DatabaseCount(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseDeleteRange(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    const IndexedDBKeyRange& key_range,
    WebIDBCallbacks* callbacks) {
  ResetCursorPrefetchCaches(transaction_id, kAllCursors);
  IndexedDBHostMsg_DatabaseDeleteRange_Params params;
  init_params(&params, callbacks);
  params.ipc_database_id = ipc_database_id;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.key_range = key_range;
  Send(new IndexedDBHostMsg_DatabaseDeleteRange(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseClear(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    WebIDBCallbacks* callbacks_ptr) {
  ResetCursorPrefetchCaches(transaction_id, kAllCursors);
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  int32 ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_DatabaseClear(CurrentWorkerId(),
                                          ipc_callbacks_id,
                                          ipc_database_id,
                                          transaction_id,
                                          object_store_id));
}

void IndexedDBDispatcher::CursorDestroyed(int32 ipc_cursor_id) {
  cursors_.erase(ipc_cursor_id);
}

void IndexedDBDispatcher::DatabaseDestroyed(int32 ipc_database_id) {
  DCHECK_EQ(databases_.count(ipc_database_id), 1u);
  databases_.erase(ipc_database_id);
}

void IndexedDBDispatcher::OnSuccessIDBDatabase(
    int32 ipc_thread_id,
    int32 ipc_callbacks_id,
    int32 ipc_database_callbacks_id,
    int32 ipc_object_id,
    const IndexedDBDatabaseMetadata& idb_metadata) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  if (!callbacks)
    return;
  WebIDBMetadata metadata(ConvertMetadata(idb_metadata));
  // If an upgrade was performed, count will be non-zero.
  WebIDBDatabase* database = NULL;

  // Back-end will send kNoDatabase if it was already sent in OnUpgradeNeeded.
  // May already be deleted and removed from the table, but do not recreate..
  if (ipc_object_id != kNoDatabase) {
    DCHECK(!databases_.count(ipc_object_id));
    database = databases_[ipc_object_id] = new WebIDBDatabaseImpl(
        ipc_object_id, ipc_database_callbacks_id, thread_safe_sender_.get());
  }

  callbacks->onSuccess(database, metadata);
  pending_callbacks_.Remove(ipc_callbacks_id);
}

void IndexedDBDispatcher::OnSuccessIndexedDBKey(int32 ipc_thread_id,
                                                int32 ipc_callbacks_id,
                                                const IndexedDBKey& key) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(WebIDBKeyBuilder::Build(key));
  pending_callbacks_.Remove(ipc_callbacks_id);
}

void IndexedDBDispatcher::OnSuccessStringList(
    int32 ipc_thread_id,
    int32 ipc_callbacks_id,
    const std::vector<base::string16>& value) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(WebVector<WebString>(value));
  pending_callbacks_.Remove(ipc_callbacks_id);
}

static void PrepareWebValueAndBlobInfo(
    const std::string& value,
    const std::vector<IndexedDBMsg_BlobOrFileInfo>& blob_info,
    WebData* web_value,
    blink::WebVector<WebBlobInfo>* web_blob_info) {

  if (value.empty())
    return;

  web_value->assign(&*value.begin(), value.size());
  blink::WebVector<WebBlobInfo> local_blob_info(blob_info.size());
  for (size_t i = 0; i < blob_info.size(); ++i) {
    const IndexedDBMsg_BlobOrFileInfo& info = blob_info[i];
    if (info.is_file) {
      local_blob_info[i] = WebBlobInfo(WebString::fromUTF8(info.uuid.c_str()),
                                       info.file_path,
                                       info.file_name,
                                       info.mime_type,
                                       info.last_modified,
                                       info.size);
    } else {
      local_blob_info[i] = WebBlobInfo(
          WebString::fromUTF8(info.uuid.c_str()), info.mime_type, info.size);
    }
  }
  web_blob_info->swap(local_blob_info);
}

void IndexedDBDispatcher::OnSuccessValue(
    const IndexedDBMsg_CallbacksSuccessValue_Params& params) {
  DCHECK_EQ(params.ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks =
      pending_callbacks_.Lookup(params.ipc_callbacks_id);
  if (!callbacks)
    return;
  WebData web_value;
  WebVector<WebBlobInfo> web_blob_info;
  PrepareWebValueAndBlobInfo(
      params.value, params.blob_or_file_info, &web_value, &web_blob_info);
  callbacks->onSuccess(web_value, web_blob_info);
  pending_callbacks_.Remove(params.ipc_callbacks_id);
  cursor_transaction_ids_.erase(params.ipc_callbacks_id);
}

void IndexedDBDispatcher::OnSuccessValueWithKey(
    const IndexedDBMsg_CallbacksSuccessValueWithKey_Params& params) {
  DCHECK_EQ(params.ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks =
      pending_callbacks_.Lookup(params.ipc_callbacks_id);
  if (!callbacks)
    return;
  WebData web_value;
  WebVector<WebBlobInfo> web_blob_info;
  PrepareWebValueAndBlobInfo(
      params.value, params.blob_or_file_info, &web_value, &web_blob_info);
  callbacks->onSuccess(web_value,
                       web_blob_info,
                       WebIDBKeyBuilder::Build(params.primary_key),
                       WebIDBKeyPathBuilder::Build(params.key_path));
  pending_callbacks_.Remove(params.ipc_callbacks_id);
}

void IndexedDBDispatcher::OnSuccessInteger(int32 ipc_thread_id,
                                           int32 ipc_callbacks_id,
                                           int64 value) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(value);
  pending_callbacks_.Remove(ipc_callbacks_id);
}

void IndexedDBDispatcher::OnSuccessUndefined(int32 ipc_thread_id,
                                             int32 ipc_callbacks_id) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  if (!callbacks)
    return;
  callbacks->onSuccess();
  pending_callbacks_.Remove(ipc_callbacks_id);
}

void IndexedDBDispatcher::OnSuccessOpenCursor(
    const IndexedDBMsg_CallbacksSuccessIDBCursor_Params& p) {
  DCHECK_EQ(p.ipc_thread_id, CurrentWorkerId());
  int32 ipc_callbacks_id = p.ipc_callbacks_id;
  int32 ipc_object_id = p.ipc_cursor_id;
  const IndexedDBKey& key = p.key;
  const IndexedDBKey& primary_key = p.primary_key;
  WebData web_value;
  WebVector<WebBlobInfo> web_blob_info;
  PrepareWebValueAndBlobInfo(
      p.value, p.blob_or_file_info, &web_value, &web_blob_info);

  DCHECK(cursor_transaction_ids_.find(ipc_callbacks_id) !=
         cursor_transaction_ids_.end());
  int64 transaction_id = cursor_transaction_ids_[ipc_callbacks_id];
  cursor_transaction_ids_.erase(ipc_callbacks_id);

  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  if (!callbacks)
    return;

  WebIDBCursorImpl* cursor = new WebIDBCursorImpl(
      ipc_object_id, transaction_id, thread_safe_sender_.get());
  cursors_[ipc_object_id] = cursor;
  callbacks->onSuccess(cursor,
                       WebIDBKeyBuilder::Build(key),
                       WebIDBKeyBuilder::Build(primary_key),
                       web_value,
                       web_blob_info);

  pending_callbacks_.Remove(ipc_callbacks_id);
}

void IndexedDBDispatcher::OnSuccessCursorContinue(
    const IndexedDBMsg_CallbacksSuccessCursorContinue_Params& p) {
  DCHECK_EQ(p.ipc_thread_id, CurrentWorkerId());
  int32 ipc_callbacks_id = p.ipc_callbacks_id;
  int32 ipc_cursor_id = p.ipc_cursor_id;
  const IndexedDBKey& key = p.key;
  const IndexedDBKey& primary_key = p.primary_key;
  const std::string& value = p.value;

  if (cursors_.find(ipc_cursor_id) == cursors_.end())
    return;

  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  if (!callbacks)
    return;

  WebData web_value;
  WebVector<WebBlobInfo> web_blob_info;
  PrepareWebValueAndBlobInfo(
      value, p.blob_or_file_info, &web_value, &web_blob_info);
  callbacks->onSuccess(WebIDBKeyBuilder::Build(key),
                       WebIDBKeyBuilder::Build(primary_key),
                       web_value,
                       web_blob_info);

  pending_callbacks_.Remove(ipc_callbacks_id);
}

void IndexedDBDispatcher::OnSuccessCursorPrefetch(
    const IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params& p) {
  DCHECK_EQ(p.ipc_thread_id, CurrentWorkerId());
  int32 ipc_callbacks_id = p.ipc_callbacks_id;
  int32 ipc_cursor_id = p.ipc_cursor_id;
  const std::vector<IndexedDBKey>& keys = p.keys;
  const std::vector<IndexedDBKey>& primary_keys = p.primary_keys;
  std::vector<WebData> values(p.values.size());
  DCHECK_EQ(p.values.size(), p.blob_or_file_infos.size());
  std::vector<WebVector<WebBlobInfo> > blob_infos(p.blob_or_file_infos.size());
  for (size_t i = 0; i < p.values.size(); ++i) {
    PrepareWebValueAndBlobInfo(
        p.values[i], p.blob_or_file_infos[i], &values[i], &blob_infos[i]);
  }
  std::map<int32, WebIDBCursorImpl*>::const_iterator cur_iter =
      cursors_.find(ipc_cursor_id);
  if (cur_iter == cursors_.end())
    return;

  cur_iter->second->SetPrefetchData(keys, primary_keys, values, blob_infos);

  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  DCHECK(callbacks);
  cur_iter->second->CachedContinue(callbacks);
  pending_callbacks_.Remove(ipc_callbacks_id);
}

void IndexedDBDispatcher::OnIntBlocked(int32 ipc_thread_id,
                                       int32 ipc_callbacks_id,
                                       int64 existing_version) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  DCHECK(callbacks);
  callbacks->onBlocked(existing_version);
}

void IndexedDBDispatcher::OnUpgradeNeeded(
    const IndexedDBMsg_CallbacksUpgradeNeeded_Params& p) {
  DCHECK_EQ(p.ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(p.ipc_callbacks_id);
  DCHECK(callbacks);
  WebIDBMetadata metadata(ConvertMetadata(p.idb_metadata));
  DCHECK(!databases_.count(p.ipc_database_id));
  databases_[p.ipc_database_id] =
      new WebIDBDatabaseImpl(p.ipc_database_id,
                             p.ipc_database_callbacks_id,
                             thread_safe_sender_.get());
  callbacks->onUpgradeNeeded(
      p.old_version,
      databases_[p.ipc_database_id],
      metadata,
      static_cast<blink::WebIDBDataLoss>(p.data_loss),
      WebString::fromUTF8(p.data_loss_message));
}

void IndexedDBDispatcher::OnError(int32 ipc_thread_id,
                                  int32 ipc_callbacks_id,
                                  int code,
                                  const base::string16& message) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  if (!callbacks)
    return;
  if (message.empty())
    callbacks->onError(WebIDBDatabaseError(code));
  else
    callbacks->onError(WebIDBDatabaseError(code, message));
  pending_callbacks_.Remove(ipc_callbacks_id);
  cursor_transaction_ids_.erase(ipc_callbacks_id);
}

void IndexedDBDispatcher::OnAbort(int32 ipc_thread_id,
                                  int32 ipc_database_callbacks_id,
                                  int64 transaction_id,
                                  int code,
                                  const base::string16& message) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBDatabaseCallbacks* callbacks =
      pending_database_callbacks_.Lookup(ipc_database_callbacks_id);
  if (!callbacks)
    return;
  if (message.empty())
    callbacks->onAbort(transaction_id, WebIDBDatabaseError(code));
  else
    callbacks->onAbort(transaction_id, WebIDBDatabaseError(code, message));
}

void IndexedDBDispatcher::OnComplete(int32 ipc_thread_id,
                                     int32 ipc_database_callbacks_id,
                                     int64 transaction_id) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBDatabaseCallbacks* callbacks =
      pending_database_callbacks_.Lookup(ipc_database_callbacks_id);
  if (!callbacks)
    return;
  callbacks->onComplete(transaction_id);
}

void IndexedDBDispatcher::OnForcedClose(int32 ipc_thread_id,
                                        int32 ipc_database_callbacks_id) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBDatabaseCallbacks* callbacks =
      pending_database_callbacks_.Lookup(ipc_database_callbacks_id);
  if (!callbacks)
    return;
  callbacks->onForcedClose();
}

void IndexedDBDispatcher::OnIntVersionChange(int32 ipc_thread_id,
                                             int32 ipc_database_callbacks_id,
                                             int64 old_version,
                                             int64 new_version) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBDatabaseCallbacks* callbacks =
      pending_database_callbacks_.Lookup(ipc_database_callbacks_id);
  // callbacks would be NULL if a versionchange event is received after close
  // has been called.
  if (!callbacks)
    return;
  callbacks->onVersionChange(old_version, new_version);
}

void IndexedDBDispatcher::ResetCursorPrefetchCaches(
    int64 transaction_id,
    int32 ipc_exception_cursor_id) {
  typedef std::map<int32, WebIDBCursorImpl*>::iterator Iterator;
  for (Iterator i = cursors_.begin(); i != cursors_.end(); ++i) {
    if (i->first == ipc_exception_cursor_id ||
        i->second->transaction_id() != transaction_id)
      continue;
    i->second->ResetPrefetchCache();
  }
}

}  // namespace content
