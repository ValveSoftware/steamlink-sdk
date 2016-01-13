// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_forwarding_message_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "ipc/ipc_message.h"

namespace IPC {

ForwardingMessageFilter::ForwardingMessageFilter(
    const uint32* message_ids_to_filter,
    size_t num_message_ids_to_filter,
    base::TaskRunner* target_task_runner)
    : target_task_runner_(target_task_runner) {
  DCHECK(target_task_runner_.get());
  for (size_t i = 0; i < num_message_ids_to_filter; i++)
    message_ids_to_filter_.insert(message_ids_to_filter[i]);
}

void ForwardingMessageFilter::AddRoute(int routing_id, const Handler& handler) {
  DCHECK(!handler.is_null());
  base::AutoLock locked(handlers_lock_);
  handlers_.insert(std::make_pair(routing_id, handler));
}

void ForwardingMessageFilter::RemoveRoute(int routing_id) {
  base::AutoLock locked(handlers_lock_);
  handlers_.erase(routing_id);
}

bool ForwardingMessageFilter::OnMessageReceived(const Message& message) {
  if (message_ids_to_filter_.find(message.type()) ==
      message_ids_to_filter_.end())
    return false;


  Handler handler;

  {
    base::AutoLock locked(handlers_lock_);
    std::map<int, Handler>::iterator it = handlers_.find(message.routing_id());
    if (it == handlers_.end())
      return false;
    handler = it->second;
  }

  target_task_runner_->PostTask(FROM_HERE, base::Bind(handler, message));
  return true;
}

ForwardingMessageFilter::~ForwardingMessageFilter() {
}

}  // namespace IPC
