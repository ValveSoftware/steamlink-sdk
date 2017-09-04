// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/quota_message_filter.h"

#include "content/child/quota_dispatcher.h"
#include "content/common/quota_messages.h"

namespace content {

QuotaMessageFilter::QuotaMessageFilter(ThreadSafeSender* thread_safe_sender)
    : WorkerThreadMessageFilter(thread_safe_sender), next_request_id_(0) {
}

QuotaMessageFilter::~QuotaMessageFilter() {}

int QuotaMessageFilter::GenerateRequestID(int thread_id) {
  base::AutoLock lock(request_id_map_lock_);
  request_id_map_[next_request_id_] = thread_id;
  return next_request_id_++;
}

void QuotaMessageFilter::ClearThreadRequests(int thread_id) {
  base::AutoLock lock(request_id_map_lock_);
  for (RequestIdToThreadId::iterator iter = request_id_map_.begin();
       iter != request_id_map_.end();) {
    if (iter->second == thread_id)
      request_id_map_.erase(iter++);
    else
      iter++;
  }
}

bool QuotaMessageFilter::ShouldHandleMessage(const IPC::Message& msg) const {
  return IPC_MESSAGE_CLASS(msg) == QuotaMsgStart;
}

void QuotaMessageFilter::OnFilteredMessageReceived(const IPC::Message& msg) {
  QuotaDispatcher::ThreadSpecificInstance(thread_safe_sender(), this)
      ->OnMessageReceived(msg);
}

bool QuotaMessageFilter::GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                                     int* ipc_thread_id) {
  int request_id = -1;
  const bool success = base::PickleIterator(msg).ReadInt(&request_id);
  DCHECK(success);

  base::AutoLock lock(request_id_map_lock_);
  RequestIdToThreadId::iterator found = request_id_map_.find(request_id);
  if (found != request_id_map_.end()) {
    *ipc_thread_id = found->second;
    request_id_map_.erase(found);
    return true;
  }
  return false;
}

}  // namespace content
