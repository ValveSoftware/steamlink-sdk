// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_message_filter.h"

#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/thread_safe_sender.h"
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

}  // namespace content
