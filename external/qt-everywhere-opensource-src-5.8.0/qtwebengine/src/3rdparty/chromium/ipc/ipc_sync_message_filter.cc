// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_sync_message_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_sync_message.h"

namespace IPC {

bool SyncMessageFilter::Send(Message* message) {
  if (!message->is_sync()) {
    {
      base::AutoLock auto_lock(lock_);
      if (sender_ && is_channel_send_thread_safe_) {
        sender_->Send(message);
        return true;
      } else if (!io_task_runner_.get()) {
        pending_messages_.push_back(message);
        return true;
      }
    }
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SyncMessageFilter::SendOnIOThread, this, message));
    return true;
  }

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PendingSyncMsg pending_message(
      SyncMessage::GetMessageId(*message),
      static_cast<SyncMessage*>(message)->GetReplyDeserializer(),
      &done_event);

  {
    base::AutoLock auto_lock(lock_);
    // Can't use this class on the main thread or else it can lead to deadlocks.
    // Also by definition, can't use this on IO thread since we're blocking it.
    if (base::ThreadTaskRunnerHandle::IsSet()) {
      DCHECK(base::ThreadTaskRunnerHandle::Get() != listener_task_runner_);
      DCHECK(base::ThreadTaskRunnerHandle::Get() != io_task_runner_);
    }
    pending_sync_messages_.insert(&pending_message);

    if (io_task_runner_.get()) {
      io_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&SyncMessageFilter::SendOnIOThread, this, message));
    } else {
      pending_messages_.push_back(message);
    }
  }

  base::WaitableEvent* events[2] = { shutdown_event_, &done_event };
  if (base::WaitableEvent::WaitMany(events, 2) == 1) {
    TRACE_EVENT_FLOW_END0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                          "SyncMessageFilter::Send", &done_event);
  }

  {
    base::AutoLock auto_lock(lock_);
    delete pending_message.deserializer;
    pending_sync_messages_.erase(&pending_message);
  }

  return pending_message.send_result;
}

void SyncMessageFilter::OnFilterAdded(Sender* sender) {
  std::vector<Message*> pending_messages;
  {
    base::AutoLock auto_lock(lock_);
    sender_ = sender;
    io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    pending_messages_.release(&pending_messages);
  }
  for (auto* msg : pending_messages)
    SendOnIOThread(msg);
}

void SyncMessageFilter::OnChannelError() {
  base::AutoLock auto_lock(lock_);
  sender_ = NULL;
  SignalAllEvents();
}

void SyncMessageFilter::OnChannelClosing() {
  base::AutoLock auto_lock(lock_);
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
      TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                              "SyncMessageFilter::OnMessageReceived",
                              (*iter)->done_event);
      (*iter)->done_event->Signal();
      return true;
    }
  }

  return false;
}

SyncMessageFilter::SyncMessageFilter(base::WaitableEvent* shutdown_event,
                                     bool is_channel_send_thread_safe)
    : sender_(NULL),
      is_channel_send_thread_safe_(is_channel_send_thread_safe),
      listener_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      shutdown_event_(shutdown_event) {
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
    base::AutoLock auto_lock(lock_);
    SignalAllEvents();
  }

  delete message;
}

void SyncMessageFilter::SignalAllEvents() {
  lock_.AssertAcquired();
  for (PendingSyncMessages::iterator iter = pending_sync_messages_.begin();
       iter != pending_sync_messages_.end(); ++iter) {
    TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                            "SyncMessageFilter::SignalAllEvents",
                            (*iter)->done_event);
    (*iter)->done_event->Signal();
  }
}

}  // namespace IPC
