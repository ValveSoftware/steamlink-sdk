// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu//compositor_forwarding_message_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_message.h"

namespace content {

CompositorForwardingMessageFilter::CompositorForwardingMessageFilter(
    base::TaskRunner* compositor_task_runner)
    : compositor_task_runner_(compositor_task_runner) {
  DCHECK(compositor_task_runner_.get());
  // This check will only be used by functions running on compositor thread.
  compositor_thread_checker_.DetachFromThread();
}

CompositorForwardingMessageFilter::~CompositorForwardingMessageFilter() {
}

void CompositorForwardingMessageFilter::AddHandlerOnCompositorThread(
    int routing_id,
    const Handler& handler) {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());
  DCHECK(!handler.is_null());
  multi_handlers_.insert(std::make_pair(routing_id, handler));
}

void CompositorForwardingMessageFilter::RemoveHandlerOnCompositorThread(
    int routing_id,
    const Handler& handler) {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());
  auto handlers = multi_handlers_.equal_range(routing_id);
  for (auto it = handlers.first; it != handlers.second; ++it) {
    if (it->second.Equals(handler)) {
      multi_handlers_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

bool CompositorForwardingMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  switch (message.type()) {
    case ViewMsg_SetBeginFramePaused::ID:
    case ViewMsg_BeginFrame::ID:
    case ViewMsg_ReclaimCompositorResources::ID:
      break;
    default:
      return false;
  }

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &CompositorForwardingMessageFilter::ProcessMessageOnCompositorThread,
          this,
          message));
  return true;
}

void CompositorForwardingMessageFilter::ProcessMessageOnCompositorThread(
    const IPC::Message& message) {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());
  auto handlers = multi_handlers_.equal_range(message.routing_id());
  if (handlers.first == handlers.second)
    return;

  for (auto it = handlers.first; it != handlers.second; ++it)
    it->second.Run(message);
}

}  // namespace content
