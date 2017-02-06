// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/queue_message_swap_promise.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "ipc/ipc_sync_message_filter.h"

namespace content {

QueueMessageSwapPromise::QueueMessageSwapPromise(
    scoped_refptr<IPC::SyncMessageFilter> message_sender,
    scoped_refptr<content::FrameSwapMessageQueue> message_queue,
    int source_frame_number)
    : message_sender_(message_sender),
      message_queue_(message_queue),
      source_frame_number_(source_frame_number)
#if DCHECK_IS_ON()
      ,
      completed_(false)
#endif
{
  DCHECK(message_sender_.get());
  DCHECK(message_queue_.get());
}

QueueMessageSwapPromise::~QueueMessageSwapPromise() {
  // The promise should have either been kept or broken before it's deleted.
#if DCHECK_IS_ON()
  DCHECK(completed_);
#endif
}

void QueueMessageSwapPromise::DidActivate() {
#if DCHECK_IS_ON()
  DCHECK(!completed_);
#endif
  message_queue_->DidActivate(source_frame_number_);
  // The OutputSurface will take care of the Drain+Send.

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseRemoteCompositing)) {
    // The remote compositing mode doesn't have an output surface, so we need to
    // Drain+Send on activation. Also, we can't use the SyncMessageFilter, since
    // this call is actually made on the main thread.
    std::vector<std::unique_ptr<IPC::Message>> messages_to_deliver;
    std::unique_ptr<FrameSwapMessageQueue::SendMessageScope>
        send_message_scope = message_queue_->AcquireSendMessageScope();
    message_queue_->DrainMessages(&messages_to_deliver);
    for (auto& message : messages_to_deliver)
      RenderThread::Get()->Send(message.release());
    PromiseCompleted();
  }
}

void QueueMessageSwapPromise::DidSwap(cc::CompositorFrameMetadata* metadata) {
#if DCHECK_IS_ON()
  DCHECK(!completed_);
#endif
  message_queue_->DidSwap(source_frame_number_);
  // The OutputSurface will take care of the Drain+Send.
  PromiseCompleted();
}

void QueueMessageSwapPromise::DidNotSwap(DidNotSwapReason reason) {
#if DCHECK_IS_ON()
  DCHECK(!completed_);
#endif
  std::vector<std::unique_ptr<IPC::Message>> messages;
  message_queue_->DidNotSwap(source_frame_number_, reason, &messages);
  for (auto& msg : messages) {
    message_sender_->Send(msg.release());
  }
  PromiseCompleted();
}

void QueueMessageSwapPromise::PromiseCompleted() {
#if DCHECK_IS_ON()
  completed_ = true;
#endif
}

int64_t QueueMessageSwapPromise::TraceId() const {
  return 0;
}

}  // namespace content
