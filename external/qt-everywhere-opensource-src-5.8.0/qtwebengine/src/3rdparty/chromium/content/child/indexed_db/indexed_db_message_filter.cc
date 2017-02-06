// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_message_filter.h"

#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/common/indexed_db/indexed_db_messages.h"

namespace content {

IndexedDBMessageFilter::IndexedDBMessageFilter(
    ThreadSafeSender* thread_safe_sender)
    : WorkerThreadMessageFilter(thread_safe_sender) {
}

IndexedDBMessageFilter::~IndexedDBMessageFilter() {}

bool IndexedDBMessageFilter::ShouldHandleMessage(
    const IPC::Message& msg) const {
  return IPC_MESSAGE_CLASS(msg) == IndexedDBMsgStart;
}

void IndexedDBMessageFilter::OnFilteredMessageReceived(
    const IPC::Message& msg) {
  IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender())
      ->OnMessageReceived(msg);
}

bool IndexedDBMessageFilter::GetWorkerThreadIdForMessage(
    const IPC::Message& msg,
    int* ipc_thread_id) {
  return base::PickleIterator(msg).ReadInt(ipc_thread_id);
}

void IndexedDBMessageFilter::OnStaleMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(IndexedDBMessageFilter, msg)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBDatabase,
                        OnStaleSuccessIDBDatabase)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksUpgradeNeeded,
                        OnStaleUpgradeNeeded)
  IPC_END_MESSAGE_MAP()
}

void IndexedDBMessageFilter::OnStaleSuccessIDBDatabase(
    int32_t ipc_thread_id,
    int32_t ipc_callbacks_id,
    int32_t ipc_database_callbacks_id,
    int32_t ipc_database_id,
    const IndexedDBDatabaseMetadata& idb_metadata) {
  if (ipc_database_id == kNoDatabase)
    return;
  thread_safe_sender()->Send(
      new IndexedDBHostMsg_DatabaseClose(ipc_database_id));
}

void IndexedDBMessageFilter::OnStaleUpgradeNeeded(
    const IndexedDBMsg_CallbacksUpgradeNeeded_Params& p) {
  thread_safe_sender()->Send(
      new IndexedDBHostMsg_DatabaseClose(p.ipc_database_id));
}

}  // namespace content
