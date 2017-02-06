// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/webidbdatabase_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/indexed_db_key_builders.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_registry.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "third_party/WebKit/public/platform/WebBlobInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBKeyPath.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBMetadata.h"

using blink::WebBlobInfo;
using blink::WebIDBCallbacks;
using blink::WebIDBCursor;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseCallbacks;
using blink::WebIDBMetadata;
using blink::WebIDBKey;
using blink::WebIDBKeyPath;
using blink::WebIDBKeyRange;
using blink::WebString;
using blink::WebVector;

namespace content {

WebIDBDatabaseImpl::WebIDBDatabaseImpl(int32_t ipc_database_id,
                                       int32_t ipc_database_callbacks_id,
                                       ThreadSafeSender* thread_safe_sender)
    : ipc_database_id_(ipc_database_id),
      ipc_database_callbacks_id_(ipc_database_callbacks_id),
      thread_safe_sender_(thread_safe_sender) {}

WebIDBDatabaseImpl::~WebIDBDatabaseImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object which owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  thread_safe_sender_->Send(
      new IndexedDBHostMsg_DatabaseDestroyed(ipc_database_id_));
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->DatabaseDestroyed(ipc_database_id_);
}

void WebIDBDatabaseImpl::createObjectStore(long long transaction_id,
                                           long long object_store_id,
                                           const WebString& name,
                                           const WebIDBKeyPath& key_path,
                                           bool auto_increment) {
  IndexedDBHostMsg_DatabaseCreateObjectStore_Params params;
  params.ipc_database_id = ipc_database_id_;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.name = name;
  params.key_path = IndexedDBKeyPathBuilder::Build(key_path);
  params.auto_increment = auto_increment;

  thread_safe_sender_->Send(
      new IndexedDBHostMsg_DatabaseCreateObjectStore(params));
}

void WebIDBDatabaseImpl::deleteObjectStore(long long transaction_id,
                                           long long object_store_id) {
  thread_safe_sender_->Send(new IndexedDBHostMsg_DatabaseDeleteObjectStore(
      ipc_database_id_, transaction_id, object_store_id));
}

void WebIDBDatabaseImpl::createTransaction(
    long long transaction_id,
    WebIDBDatabaseCallbacks* callbacks,
    const WebVector<long long>& object_store_ids,
    blink::WebIDBTransactionMode mode) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBDatabaseCreateTransaction(
      ipc_database_id_, transaction_id, callbacks, object_store_ids, mode);
}

void WebIDBDatabaseImpl::close() {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBDatabaseClose(ipc_database_id_,
                                      ipc_database_callbacks_id_);
}

void WebIDBDatabaseImpl::versionChangeIgnored() {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->NotifyIDBDatabaseVersionChangeIgnored(ipc_database_id_);
}

void WebIDBDatabaseImpl::get(long long transaction_id,
                             long long object_store_id,
                             long long index_id,
                             const WebIDBKeyRange& key_range,
                             bool key_only,
                             WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBDatabaseGet(ipc_database_id_,
                                    transaction_id,
                                    object_store_id,
                                    index_id,
                                    IndexedDBKeyRangeBuilder::Build(key_range),
                                    key_only,
                                    callbacks);
}

void WebIDBDatabaseImpl::getAll(long long transaction_id,
                                long long object_store_id,
                                long long index_id,
                                const WebIDBKeyRange& key_range,
                                long long max_count,
                                bool key_only,
                                WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBDatabaseGetAll(
      ipc_database_id_, transaction_id, object_store_id, index_id,
      IndexedDBKeyRangeBuilder::Build(key_range), key_only, max_count,
      callbacks);
}

void WebIDBDatabaseImpl::put(long long transaction_id,
                             long long object_store_id,
                             const blink::WebData& value,
                             const blink::WebVector<WebBlobInfo>& web_blob_info,
                             const WebIDBKey& key,
                             blink::WebIDBPutMode put_mode,
                             WebIDBCallbacks* callbacks,
                             const WebVector<long long>& web_index_ids,
                             const WebVector<WebIndexKeys>& web_index_keys) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBDatabasePut(ipc_database_id_,
                                    transaction_id,
                                    object_store_id,
                                    value,
                                    web_blob_info,
                                    IndexedDBKeyBuilder::Build(key),
                                    put_mode,
                                    callbacks,
                                    web_index_ids,
                                    web_index_keys);
}

void WebIDBDatabaseImpl::setIndexKeys(
    long long transaction_id,
    long long object_store_id,
    const WebIDBKey& primary_key,
    const WebVector<long long>& index_ids,
    const WebVector<WebIndexKeys>& index_keys) {
  IndexedDBHostMsg_DatabaseSetIndexKeys_Params params;
  params.ipc_database_id = ipc_database_id_;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.primary_key = IndexedDBKeyBuilder::Build(primary_key);

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

  thread_safe_sender_->Send(new IndexedDBHostMsg_DatabaseSetIndexKeys(params));
}

void WebIDBDatabaseImpl::setIndexesReady(
    long long transaction_id,
    long long object_store_id,
    const WebVector<long long>& web_index_ids) {
  std::vector<int64_t> index_ids(web_index_ids.data(),
                                 web_index_ids.data() + web_index_ids.size());
  thread_safe_sender_->Send(new IndexedDBHostMsg_DatabaseSetIndexesReady(
      ipc_database_id_, transaction_id, object_store_id, index_ids));
}

void WebIDBDatabaseImpl::openCursor(long long transaction_id,
                                    long long object_store_id,
                                    long long index_id,
                                    const WebIDBKeyRange& key_range,
                                    blink::WebIDBCursorDirection direction,
                                    bool key_only,
                                    blink::WebIDBTaskType task_type,
                                    WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBDatabaseOpenCursor(
      ipc_database_id_,
      transaction_id,
      object_store_id,
      index_id,
      IndexedDBKeyRangeBuilder::Build(key_range),
      direction,
      key_only,
      task_type,
      callbacks);
}

void WebIDBDatabaseImpl::count(long long transaction_id,
                               long long object_store_id,
                               long long index_id,
                               const WebIDBKeyRange& key_range,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBDatabaseCount(
      ipc_database_id_,
      transaction_id,
      object_store_id,
      index_id,
      IndexedDBKeyRangeBuilder::Build(key_range),
      callbacks);
}

void WebIDBDatabaseImpl::deleteRange(long long transaction_id,
                                     long long object_store_id,
                                     const WebIDBKeyRange& key_range,
                                     WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBDatabaseDeleteRange(
      ipc_database_id_,
      transaction_id,
      object_store_id,
      IndexedDBKeyRangeBuilder::Build(key_range),
      callbacks);
}

void WebIDBDatabaseImpl::clear(long long transaction_id,
                               long long object_store_id,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBDatabaseClear(
      ipc_database_id_, transaction_id, object_store_id, callbacks);
}

void WebIDBDatabaseImpl::createIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id,
                                     const WebString& name,
                                     const WebIDBKeyPath& key_path,
                                     bool unique,
                                     bool multi_entry) {
  IndexedDBHostMsg_DatabaseCreateIndex_Params params;
  params.ipc_database_id = ipc_database_id_;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.index_id = index_id;
  params.name = name;
  params.key_path = IndexedDBKeyPathBuilder::Build(key_path);
  params.unique = unique;
  params.multi_entry = multi_entry;

  thread_safe_sender_->Send(new IndexedDBHostMsg_DatabaseCreateIndex(params));
}

void WebIDBDatabaseImpl::deleteIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id) {
  thread_safe_sender_->Send(new IndexedDBHostMsg_DatabaseDeleteIndex(
      ipc_database_id_, transaction_id, object_store_id, index_id));
}

void WebIDBDatabaseImpl::abort(long long transaction_id) {
  thread_safe_sender_->Send(
      new IndexedDBHostMsg_DatabaseAbort(ipc_database_id_, transaction_id));
}

void WebIDBDatabaseImpl::commit(long long transaction_id) {
  thread_safe_sender_->Send(
      new IndexedDBHostMsg_DatabaseCommit(ipc_database_id_, transaction_id));
}

void WebIDBDatabaseImpl::ackReceivedBlobs(const WebVector<WebString>& uuids) {
  DCHECK(uuids.size());
  std::vector<std::string> param(uuids.size());
  for (size_t i = 0; i < uuids.size(); ++i)
    param[i] = uuids[i].latin1().data();
  thread_safe_sender_->Send(new IndexedDBHostMsg_AckReceivedBlobs(param));
}

}  // namespace content
