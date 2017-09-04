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
#include "content/common/indexed_db/indexed_db_messages.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseError.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseException.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBObservation.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBValue.h"

using blink::WebBlobInfo;
using blink::WebData;
using blink::WebIDBCallbacks;
using blink::WebIDBCursor;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseCallbacks;
using blink::WebIDBDatabaseError;
using blink::WebIDBKey;
using blink::WebIDBMetadata;
using blink::WebIDBObservation;
using blink::WebIDBObserver;
using blink::WebIDBValue;
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

IndexedDBDispatcher::IndexedDBDispatcher(ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender) {
  g_idb_dispatcher_tls.Pointer()->Set(this);
}

IndexedDBDispatcher::~IndexedDBDispatcher() {
  // Clear any pending callbacks - which may result in dispatch requests -
  // before marking the dispatcher as deleted.
  pending_callbacks_.Clear();

  DCHECK(pending_callbacks_.IsEmpty());

  in_destructor_ = true;
  mojo_owned_callback_state_.clear();
  mojo_owned_database_callback_state_.clear();

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
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(dispatcher);
  return dispatcher;
}

void IndexedDBDispatcher::WillStopCurrentWorkerThread() {
  delete this;
}

std::vector<WebIDBObservation> IndexedDBDispatcher::ConvertObservations(
    const std::vector<IndexedDBMsg_Observation>& idb_observations) {
  std::vector<WebIDBObservation> web_observations;
  for (const auto& idb_observation : idb_observations) {
    WebIDBObservation web_observation;
    web_observation.objectStoreId = idb_observation.object_store_id;
    web_observation.type = idb_observation.type;
    web_observation.keyRange =
        WebIDBKeyRangeBuilder::Build(idb_observation.key_range);
    // TODO(palakj): Assign value to web_observation.
    web_observations.push_back(std::move(web_observation));
  }
  return web_observations;
}

void IndexedDBDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IndexedDBDispatcher, msg)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessCursorAdvance,
                        OnSuccessCursorContinue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessCursorContinue,
                        OnSuccessCursorContinue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessCursorPrefetch,
                        OnSuccessCursorPrefetch)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessValue, OnSuccessValue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessInteger, OnSuccessInteger)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksError, OnError)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksChanges,
                        OnDatabaseChanges)
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

int32_t IndexedDBDispatcher::RegisterObserver(
    std::unique_ptr<WebIDBObserver> observer) {
  return observers_.Add(observer.release());
}

void IndexedDBDispatcher::RemoveObservers(
    const std::vector<int32_t>& observer_ids_to_remove) {
  for (int32_t id : observer_ids_to_remove)
    observers_.Remove(id);
}

void IndexedDBDispatcher::RequestIDBCursorAdvance(
    unsigned long count,
    WebIDBCallbacks* callbacks_ptr,
    int32_t ipc_cursor_id,
    int64_t transaction_id) {
  // Reset all cursor prefetch caches except for this cursor.
  ResetCursorPrefetchCaches(transaction_id, ipc_cursor_id);

  std::unique_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32_t ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorAdvance(
      ipc_cursor_id, CurrentWorkerId(), ipc_callbacks_id, count));
}

void IndexedDBDispatcher::RequestIDBCursorContinue(
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    WebIDBCallbacks* callbacks_ptr,
    int32_t ipc_cursor_id,
    int64_t transaction_id) {
  // Reset all cursor prefetch caches except for this cursor.
  ResetCursorPrefetchCaches(transaction_id, ipc_cursor_id);

  std::unique_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32_t ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorContinue(
      ipc_cursor_id, CurrentWorkerId(), ipc_callbacks_id, key, primary_key));
}

void IndexedDBDispatcher::RequestIDBCursorPrefetch(
    int n,
    WebIDBCallbacks* callbacks_ptr,
    int32_t ipc_cursor_id) {
  std::unique_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32_t ipc_callbacks_id = pending_callbacks_.Add(callbacks.release());
  Send(new IndexedDBHostMsg_CursorPrefetch(
      ipc_cursor_id, CurrentWorkerId(), ipc_callbacks_id, n));
}

void IndexedDBDispatcher::RequestIDBCursorPrefetchReset(int used_prefetches,
                                                        int unused_prefetches,
                                                        int32_t ipc_cursor_id) {
  Send(new IndexedDBHostMsg_CursorPrefetchReset(
      ipc_cursor_id, used_prefetches, unused_prefetches));
}

void IndexedDBDispatcher::RegisterCursor(int32_t ipc_cursor_id,
                                         WebIDBCursorImpl* cursor) {
  DCHECK(!base::ContainsKey(cursors_, ipc_cursor_id));
  cursors_[ipc_cursor_id] = cursor;
}

void IndexedDBDispatcher::CursorDestroyed(int32_t ipc_cursor_id) {
  cursors_.erase(ipc_cursor_id);
}

void IndexedDBDispatcher::RegisterMojoOwnedCallbacks(
    IndexedDBCallbacksImpl::InternalState* callbacks) {
  mojo_owned_callback_state_[callbacks] = base::WrapUnique(callbacks);
}

void IndexedDBDispatcher::UnregisterMojoOwnedCallbacks(
    IndexedDBCallbacksImpl::InternalState* callbacks) {
  if (in_destructor_)
    return;

  auto it = mojo_owned_callback_state_.find(callbacks);
  DCHECK(it != mojo_owned_callback_state_.end());
  it->second.release();
  mojo_owned_callback_state_.erase(it);
}

void IndexedDBDispatcher::RegisterMojoOwnedDatabaseCallbacks(
    blink::WebIDBDatabaseCallbacks* callbacks) {
  mojo_owned_database_callback_state_[callbacks] = base::WrapUnique(callbacks);
}

void IndexedDBDispatcher::UnregisterMojoOwnedDatabaseCallbacks(
    blink::WebIDBDatabaseCallbacks* callbacks) {
  if (in_destructor_)
    return;

  auto it = mojo_owned_database_callback_state_.find(callbacks);
  DCHECK(it != mojo_owned_database_callback_state_.end());
  it->second.release();
  mojo_owned_database_callback_state_.erase(it);
}

// Populate some WebIDBValue members (data & blob info) from the supplied
// value message (IndexedDBMsg_Value or one that includes it).
template <class IndexedDBMsgValueType>
static void PrepareWebValue(const IndexedDBMsgValueType& value,
                            WebIDBValue* web_value) {
  if (value.bits.empty())
    return;

  web_value->data.assign(&*value.bits.begin(), value.bits.size());
  blink::WebVector<WebBlobInfo> local_blob_info(value.blob_or_file_info.size());
  for (size_t i = 0; i < value.blob_or_file_info.size(); ++i) {
    const IndexedDBMsg_BlobOrFileInfo& info = value.blob_or_file_info[i];
    if (info.is_file) {
      local_blob_info[i] = WebBlobInfo(
          WebString::fromUTF8(info.uuid.c_str()), info.file_path,
          info.file_name, info.mime_type, info.last_modified, info.size);
    } else {
      local_blob_info[i] = WebBlobInfo(WebString::fromUTF8(info.uuid.c_str()),
                                       info.mime_type, info.size);
    }
  }

  web_value->webBlobInfo.swap(local_blob_info);
}

static void PrepareReturnWebValue(const IndexedDBMsg_ReturnValue& value,
                                  WebIDBValue* web_value) {
  PrepareWebValue(value, web_value);
  web_value->primaryKey = WebIDBKeyBuilder::Build(value.primary_key);
  web_value->keyPath = WebIDBKeyPathBuilder::Build(value.key_path);
}

void IndexedDBDispatcher::OnSuccessValue(
    const IndexedDBMsg_CallbacksSuccessValue_Params& params) {
  DCHECK_EQ(params.ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks =
      pending_callbacks_.Lookup(params.ipc_callbacks_id);
  if (!callbacks)
    return;
  WebIDBValue web_value;
  PrepareReturnWebValue(params.value, &web_value);
  if (params.value.primary_key.IsValid()) {
    web_value.primaryKey = WebIDBKeyBuilder::Build(params.value.primary_key);
    web_value.keyPath = WebIDBKeyPathBuilder::Build(params.value.key_path);
  }
  callbacks->onSuccess(web_value);
  pending_callbacks_.Remove(params.ipc_callbacks_id);
}

void IndexedDBDispatcher::OnSuccessInteger(int32_t ipc_thread_id,
                                           int32_t ipc_callbacks_id,
                                           int64_t value) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(value);
  pending_callbacks_.Remove(ipc_callbacks_id);
}

void IndexedDBDispatcher::OnSuccessCursorContinue(
    const IndexedDBMsg_CallbacksSuccessCursorContinue_Params& p) {
  DCHECK_EQ(p.ipc_thread_id, CurrentWorkerId());
  int32_t ipc_callbacks_id = p.ipc_callbacks_id;
  int32_t ipc_cursor_id = p.ipc_cursor_id;
  const IndexedDBKey& key = p.key;
  const IndexedDBKey& primary_key = p.primary_key;

  if (cursors_.find(ipc_cursor_id) == cursors_.end())
    return;

  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  if (!callbacks)
    return;

  WebIDBValue web_value;
  PrepareWebValue(p.value, &web_value);
  callbacks->onSuccess(WebIDBKeyBuilder::Build(key),
                       WebIDBKeyBuilder::Build(primary_key), web_value);

  pending_callbacks_.Remove(ipc_callbacks_id);
}

void IndexedDBDispatcher::OnSuccessCursorPrefetch(
    const IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params& p) {
  DCHECK_EQ(p.ipc_thread_id, CurrentWorkerId());
  int32_t ipc_callbacks_id = p.ipc_callbacks_id;
  int32_t ipc_cursor_id = p.ipc_cursor_id;
  std::vector<WebIDBValue> values(p.values.size());
  for (size_t i = 0; i < p.values.size(); ++i)
    PrepareWebValue(p.values[i], &values[i]);
  std::map<int32_t, WebIDBCursorImpl*>::const_iterator cur_iter =
      cursors_.find(ipc_cursor_id);
  if (cur_iter == cursors_.end())
    return;

  cur_iter->second->SetPrefetchData(p.keys, p.primary_keys, values);

  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(ipc_callbacks_id);
  DCHECK(callbacks);
  cur_iter->second->CachedContinue(callbacks);
  pending_callbacks_.Remove(ipc_callbacks_id);
}

void IndexedDBDispatcher::OnError(int32_t ipc_thread_id,
                                  int32_t ipc_callbacks_id,
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
}

void IndexedDBDispatcher::OnDatabaseChanges(
    int32_t ipc_thread_id,
    const IndexedDBMsg_ObserverChanges& changes) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  std::vector<WebIDBObservation> observations(
      ConvertObservations(changes.observations));
  for (auto& it : changes.observation_index) {
    WebIDBObserver* observer = observers_.Lookup(it.first);
    // An observer can be removed from the renderer, but still exist in the
    // backend. Moreover, observer might have recorded some changes before being
    // removed from the backend and thus, have its id be present in changes.
    if (!observer)
      continue;
    observer->onChange(observations, std::move(it.second));
  }
}

void IndexedDBDispatcher::ResetCursorPrefetchCaches(
    int64_t transaction_id,
    int32_t ipc_exception_cursor_id) {
  typedef std::map<int32_t, WebIDBCursorImpl*>::iterator Iterator;
  for (Iterator i = cursors_.begin(); i != cursors_.end(); ++i) {
    if (i->first == ipc_exception_cursor_id ||
        i->second->transaction_id() != transaction_id)
      continue;
    i->second->ResetPrefetchCache();
  }
}

}  // namespace content
