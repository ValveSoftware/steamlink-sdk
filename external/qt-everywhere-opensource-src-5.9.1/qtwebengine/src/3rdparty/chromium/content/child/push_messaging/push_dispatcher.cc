// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/push_messaging/push_dispatcher.h"

#include "content/child/push_messaging/push_provider.h"
#include "content/common/push_messaging_messages.h"

namespace content {

PushDispatcher::PushDispatcher(ThreadSafeSender* thread_safe_sender)
    : WorkerThreadMessageFilter(thread_safe_sender), next_request_id_(0) {}

PushDispatcher::~PushDispatcher() {}

int PushDispatcher::GenerateRequestId(int thread_id) {
  base::AutoLock lock(request_id_map_lock_);
  request_id_map_[next_request_id_] = thread_id;
  return next_request_id_++;
}

bool PushDispatcher::ShouldHandleMessage(const IPC::Message& msg) const {
  // Note that not all Push API IPC messages flow through this class. A subset
  // of the API functionality requires a direct association with a document and
  // a frame, and for those cases the IPC messages are handled by a
  // RenderFrameObserver.
  return msg.type() == PushMessagingMsg_SubscribeFromWorkerSuccess::ID ||
         msg.type() == PushMessagingMsg_SubscribeFromWorkerError::ID ||
         msg.type() == PushMessagingMsg_GetSubscriptionSuccess::ID ||
         msg.type() == PushMessagingMsg_GetSubscriptionError::ID ||
         msg.type() == PushMessagingMsg_GetPermissionStatusSuccess::ID ||
         msg.type() == PushMessagingMsg_GetPermissionStatusError::ID ||
         msg.type() == PushMessagingMsg_UnsubscribeSuccess::ID ||
         msg.type() == PushMessagingMsg_UnsubscribeError::ID;
}

void PushDispatcher::OnFilteredMessageReceived(const IPC::Message& msg) {
  bool handled =
      PushProvider::ThreadSpecificInstance(thread_safe_sender(), this)
          ->OnMessageReceived(msg);
  DCHECK(handled);
}

bool PushDispatcher::GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                                 int* ipc_thread_id) {
  int request_id = -1;

  const bool success = base::PickleIterator(msg).ReadInt(&request_id);
  DCHECK(success);

  base::AutoLock lock(request_id_map_lock_);
  auto it = request_id_map_.find(request_id);
  if (it != request_id_map_.end()) {
    *ipc_thread_id = it->second;
    request_id_map_.erase(it);
    return true;
  }
  return false;
}

}  // namespace content
