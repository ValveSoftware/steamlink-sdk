// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_message_filter.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_task_runner.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/common/indexed_db/indexed_db_messages.h"

namespace content {

IndexedDBMessageFilter::IndexedDBMessageFilter(
    ThreadSafeSender* thread_safe_sender)
    : main_thread_loop_(base::MessageLoopProxy::current()),
      thread_safe_sender_(thread_safe_sender) {}

IndexedDBMessageFilter::~IndexedDBMessageFilter() {}

base::TaskRunner* IndexedDBMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != IndexedDBMsgStart)
    return NULL;
  int ipc_thread_id = 0;
  const bool success = PickleIterator(msg).ReadInt(&ipc_thread_id);
  DCHECK(success);
  if (!ipc_thread_id)
    return main_thread_loop_.get();
  return new WorkerThreadTaskRunner(ipc_thread_id);
}

bool IndexedDBMessageFilter::OnMessageReceived(const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != IndexedDBMsgStart)
    return false;
  IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get())
      ->OnMessageReceived(msg);
  return true;
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
    int32 ipc_thread_id,
    int32 ipc_callbacks_id,
    int32 ipc_database_callbacks_id,
    int32 ipc_database_id,
    const IndexedDBDatabaseMetadata& idb_metadata) {
  if (ipc_database_id == kNoDatabase)
    return;
  thread_safe_sender_->Send(
      new IndexedDBHostMsg_DatabaseClose(ipc_database_id));
}

void IndexedDBMessageFilter::OnStaleUpgradeNeeded(
    const IndexedDBMsg_CallbacksUpgradeNeeded_Params& p) {
  thread_safe_sender_->Send(
      new IndexedDBHostMsg_DatabaseClose(p.ipc_database_id));
}

}  // namespace content
