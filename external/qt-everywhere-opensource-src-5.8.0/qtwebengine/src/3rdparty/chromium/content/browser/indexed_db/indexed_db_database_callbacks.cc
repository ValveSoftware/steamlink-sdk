// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database_callbacks.h"

#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/common/indexed_db/indexed_db_messages.h"

namespace content {

IndexedDBDatabaseCallbacks::IndexedDBDatabaseCallbacks(
    IndexedDBDispatcherHost* dispatcher_host,
    int ipc_thread_id,
    int ipc_database_callbacks_id)
    : dispatcher_host_(dispatcher_host),
      ipc_thread_id_(ipc_thread_id),
      ipc_database_callbacks_id_(ipc_database_callbacks_id) {}

IndexedDBDatabaseCallbacks::~IndexedDBDatabaseCallbacks() {}

void IndexedDBDatabaseCallbacks::OnForcedClose() {
  if (!dispatcher_host_.get())
    return;

  dispatcher_host_->Send(new IndexedDBMsg_DatabaseCallbacksForcedClose(
      ipc_thread_id_, ipc_database_callbacks_id_));

  dispatcher_host_ = NULL;
}

void IndexedDBDatabaseCallbacks::OnVersionChange(int64_t old_version,
                                                 int64_t new_version) {
  if (!dispatcher_host_.get())
    return;

  dispatcher_host_->Send(new IndexedDBMsg_DatabaseCallbacksVersionChange(
      ipc_thread_id_, ipc_database_callbacks_id_, old_version, new_version));
}

void IndexedDBDatabaseCallbacks::OnAbort(int64_t host_transaction_id,
                                         const IndexedDBDatabaseError& error) {
  if (!dispatcher_host_.get())
    return;

  dispatcher_host_->FinishTransaction(host_transaction_id, false);
  dispatcher_host_->Send(new IndexedDBMsg_DatabaseCallbacksAbort(
      ipc_thread_id_,
      ipc_database_callbacks_id_,
      dispatcher_host_->RendererTransactionId(host_transaction_id),
      error.code(),
      error.message()));
}

void IndexedDBDatabaseCallbacks::OnComplete(int64_t host_transaction_id) {
  if (!dispatcher_host_.get())
    return;

  dispatcher_host_->FinishTransaction(host_transaction_id, true);
  dispatcher_host_->Send(new IndexedDBMsg_DatabaseCallbacksComplete(
      ipc_thread_id_,
      ipc_database_callbacks_id_,
      dispatcher_host_->RendererTransactionId(host_transaction_id)));
}

}  // namespace content
