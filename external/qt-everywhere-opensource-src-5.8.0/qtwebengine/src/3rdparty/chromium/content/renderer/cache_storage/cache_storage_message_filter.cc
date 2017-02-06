// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/cache_storage/cache_storage_message_filter.h"

#include "content/child/thread_safe_sender.h"
#include "content/common/cache_storage/cache_storage_messages.h"
#include "content/renderer/cache_storage/cache_storage_dispatcher.h"

namespace content {

CacheStorageMessageFilter::CacheStorageMessageFilter(
    ThreadSafeSender* thread_safe_sender)
    : WorkerThreadMessageFilter(thread_safe_sender) {
}

CacheStorageMessageFilter::~CacheStorageMessageFilter() {
}

bool CacheStorageMessageFilter::ShouldHandleMessage(
    const IPC::Message& msg) const {
  return IPC_MESSAGE_CLASS(msg) == CacheStorageMsgStart;
}

void CacheStorageMessageFilter::OnFilteredMessageReceived(
    const IPC::Message& msg) {
  CacheStorageDispatcher::ThreadSpecificInstance(thread_safe_sender())
      ->OnMessageReceived(msg);
}

bool CacheStorageMessageFilter::GetWorkerThreadIdForMessage(
    const IPC::Message& msg,
    int* ipc_thread_id) {
  return base::PickleIterator(msg).ReadInt(ipc_thread_id);
}

}  // namespace content
