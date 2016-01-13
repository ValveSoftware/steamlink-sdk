// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_sync_message_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_sync_message.h"

using base::MessageLoopProxy;

namespace IPC {

SyncMessageFilter::SyncMessageFilter(base::WaitableEvent* shutdown_event)
    : sender_(NULL),
      listener_loop_(MessageLoopProxy::current()),
      shutdown_event_(shutdown_event) {
}

bool SyncMessageFilter::Send(Message* message) {
  {
    base::AutoLock auto_lock(lock_);
    if (!io_loop_.get()) {
      delete message;
      return false;
    }
  }

  if (!message->is_sync()) {
    io_loop_->PostTask(
      FROM_HERE, base::Bind(&SyncMessageFilter::SendOnIOThread, this, message));
    return true;
  }

  base::WaitableEvent done_event(true, false);
  PendingSyncMsg pending_message(
      SyncMessage::GetMessageId(*message),
      static_cast<SyncMessage*>(message)->GetReplyDeserializer(),
      &done_event);

  {
    base::AutoLock auto_lock(lock_);
    // Can't use this class on the main thread or else it can lead to deadlocks.
    // Also by definition, can't use this on IO thread since we're blocking it.
    DCHECK(MessageLoopProxy::current().get() != listener_loop_.get());
    DCHECK(MessageLoopProxy::current().get() != io_loop_.get());
    pending_sync_messages_.insert(&pending_message);
  }

  io_loop_->PostTask(
      FROM_HERE, base::Bind(&SyncMessageFilter::SendOnIOThread, this, message));

  base::WaitableEvent* events[2] = { shutdown_event_, &done_event };
  base::WaitableEvent::WaitMany(events, 2);

  {
    base::AutoLock auto_lock(lock_);
    delete pending_message.deserializer;
    pending_sync_messages_.erase(&pending_message);
  }

  return pending_message.send_result;
}

void SyncMessageFilter::OnFilterAdded(Sender* sender) {
  sender_ = sender;
  base::AutoLock auto_lock(lock_);
  io_loop_ = MessageLoopProxy::current();
}

void SyncMessageFilter::OnChannelError() {
  sender_ = NULL;
  SignalAllEvents();
}

void SyncMessageFilter::OnChannelClosing() {
  sender_ = NULL;
  SignalAllEvents();
}

bool SyncMessageFilter::OnMessageReceived(const Message& message) {
  base::AutoLock auto_lock(lock_);
  for (PendingSyncMessages::iterator iter = pending_sync_messages_.begin();
       iter != pending_sync_messages_.end(); ++iter) {
    if (SyncMessage::IsMessageReplyTo(message, (*iter)->id)) {
      if (!message.is_reply_error()) {
        (*iter)->send_result =
            (*iter)->deserializer->SerializeOutputParameters(message);
      }
      (*iter)->done_event->Signal();
      return true;
    }
  }

  return false;
}

SyncMessageFilter::~SyncMessageFilter() {
}

void SyncMessageFilter::SendOnIOThread(Message* message) {
  if (sender_) {
    sender_->Send(message);
    return;
  }

  if (message->is_sync()) {
    // We don't know which thread sent it, but it doesn't matter, just signal
    // them all.
    SignalAllEvents();
  }

  delete message;
}

void SyncMessageFilter::SignalAllEvents() {
  base::AutoLock auto_lock(lock_);
  for (PendingSyncMessages::iterator iter = pending_sync_messages_.begin();
       iter != pending_sync_messages_.end(); ++iter) {
    (*iter)->done_event->Signal();
  }
}

}  // namespace IPC
