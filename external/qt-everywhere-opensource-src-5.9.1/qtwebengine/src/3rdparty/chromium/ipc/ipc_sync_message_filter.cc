// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_sync_message_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/mojo_event.h"
#include "mojo/public/cpp/bindings/sync_handle_registry.h"

namespace IPC {

namespace {

// A generic callback used when watching handles synchronously. Sets |*signal|
// to true. Also sets |*error| to true in case of an error.
void OnSyncHandleReady(bool* signal, bool* error, MojoResult result) {
  *signal = true;
  *error = result != MOJO_RESULT_OK;
}

}  // namespace

// A helper class created by SyncMessageFilter to watch the lifetime of the IO
// MessageLoop. This holds a weak ref to the SyncMessageFilter and notifies it
// on its own thread if the SyncMessageFilter is still alive at the time of
// IO MessageLoop destruction.
class SyncMessageFilter::IOMessageLoopObserver
    : public base::MessageLoop::DestructionObserver,
      public base::RefCountedThreadSafe<IOMessageLoopObserver> {
 public:
  IOMessageLoopObserver(
      base::WeakPtr<SyncMessageFilter> weak_filter,
      scoped_refptr<base::SingleThreadTaskRunner> filter_task_runner)
      : weak_filter_(weak_filter), filter_task_runner_(filter_task_runner) {}

  void StartOnIOThread() {
    DCHECK(!watching_);
    watching_ = true;
    io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    base::MessageLoop::current()->AddDestructionObserver(this);
  }

  void Stop() {
    if (!io_task_runner_)
      return;

    if (io_task_runner_->BelongsToCurrentThread()) {
      StopOnIOThread();
    } else {
      io_task_runner_->PostTask(
          FROM_HERE, base::Bind(&IOMessageLoopObserver::StopOnIOThread, this));
    }
  }

 private:
  void StopOnIOThread() {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    if (!watching_)
      return;
    watching_ = false;
    base::MessageLoop::current()->RemoveDestructionObserver(this);
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    DCHECK(io_task_runner_ && io_task_runner_->BelongsToCurrentThread());
    DCHECK(watching_);
    StopOnIOThread();
    filter_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SyncMessageFilter::OnIOMessageLoopDestroyed, weak_filter_));
  }

  friend class base::RefCountedThreadSafe<IOMessageLoopObserver>;

  ~IOMessageLoopObserver() override {}

  bool watching_ = false;
  base::WeakPtr<SyncMessageFilter> weak_filter_;
  scoped_refptr<base::SingleThreadTaskRunner> filter_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(IOMessageLoopObserver);
};

bool SyncMessageFilter::Send(Message* message) {
  if (!message->is_sync()) {
    {
      base::AutoLock auto_lock(lock_);
      if (!io_task_runner_.get()) {
        pending_messages_.emplace_back(base::WrapUnique(message));
        return true;
      }
    }
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SyncMessageFilter::SendOnIOThread, this, message));
    return true;
  }

  MojoEvent done_event;
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
      pending_messages_.emplace_back(base::WrapUnique(message));
    }
  }

  bool done = false;
  bool shutdown = false;
  bool error = false;
  scoped_refptr<mojo::SyncHandleRegistry> registry =
      mojo::SyncHandleRegistry::current();
  registry->RegisterHandle(shutdown_mojo_event_.GetHandle(),
                           MOJO_HANDLE_SIGNAL_READABLE,
                           base::Bind(&OnSyncHandleReady, &shutdown, &error));
  registry->RegisterHandle(done_event.GetHandle(),
                           MOJO_HANDLE_SIGNAL_READABLE,
                           base::Bind(&OnSyncHandleReady, &done, &error));

  const bool* stop_flags[] = { &done, &shutdown };
  registry->WatchAllHandles(stop_flags, 2);
  DCHECK(!error);

  if (done) {
    TRACE_EVENT_FLOW_END0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                          "SyncMessageFilter::Send", &done_event);
  }
  registry->UnregisterHandle(shutdown_mojo_event_.GetHandle());
  registry->UnregisterHandle(done_event.GetHandle());

  {
    base::AutoLock auto_lock(lock_);
    delete pending_message.deserializer;
    pending_sync_messages_.erase(&pending_message);
  }

  return pending_message.send_result;
}

void SyncMessageFilter::OnFilterAdded(Channel* channel) {
  std::vector<std::unique_ptr<Message>> pending_messages;
  {
    base::AutoLock auto_lock(lock_);
    channel_ = channel;
    Channel::AssociatedInterfaceSupport* support =
        channel_->GetAssociatedInterfaceSupport();
    if (support)
      channel_associated_group_ = *support->GetAssociatedGroup();

    io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    shutdown_watcher_.StartWatching(
        shutdown_event_,
        base::Bind(&SyncMessageFilter::OnShutdownEventSignaled, this));
    io_message_loop_observer_->StartOnIOThread();
    std::swap(pending_messages_, pending_messages);
  }
  for (auto& msg : pending_messages)
    SendOnIOThread(msg.release());
}

void SyncMessageFilter::OnChannelError() {
  base::AutoLock auto_lock(lock_);
  channel_ = nullptr;
  shutdown_watcher_.StopWatching();
  SignalAllEvents();
}

void SyncMessageFilter::OnChannelClosing() {
  base::AutoLock auto_lock(lock_);
  channel_ = nullptr;
  shutdown_watcher_.StopWatching();
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

SyncMessageFilter::SyncMessageFilter(base::WaitableEvent* shutdown_event)
    : channel_(nullptr),
      listener_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      shutdown_event_(shutdown_event),
      weak_factory_(this) {
  io_message_loop_observer_ = new IOMessageLoopObserver(
      weak_factory_.GetWeakPtr(), listener_task_runner_);
}

SyncMessageFilter::~SyncMessageFilter() {
  io_message_loop_observer_->Stop();
}

void SyncMessageFilter::SendOnIOThread(Message* message) {
  if (channel_) {
    channel_->Send(message);
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

void SyncMessageFilter::OnShutdownEventSignaled(base::WaitableEvent* event) {
  DCHECK_EQ(event, shutdown_event_);
  shutdown_mojo_event_.Signal();
}

void SyncMessageFilter::OnIOMessageLoopDestroyed() {
  // Since we use an async WaitableEventWatcher to watch the shutdown event
  // from the IO thread, we can't forward the shutdown signal after the IO
  // message loop is destroyed. Since that destruction indicates shutdown
  // anyway, we manually signal the shutdown event in this case.
  shutdown_mojo_event_.Signal();
}

void SyncMessageFilter::GetGenericRemoteAssociatedInterface(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  base::AutoLock auto_lock(lock_);
  DCHECK(io_task_runner_ && io_task_runner_->BelongsToCurrentThread());
  if (!channel_)
    return;

  Channel::AssociatedInterfaceSupport* support =
      channel_->GetAssociatedInterfaceSupport();
  support->GetGenericRemoteAssociatedInterface(
      interface_name, std::move(handle));
}

}  // namespace IPC
