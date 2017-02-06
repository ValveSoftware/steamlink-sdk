// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_sync_channel.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_sync_message.h"

using base::TimeDelta;
using base::TimeTicks;
using base::WaitableEvent;

namespace IPC {
// When we're blocked in a Send(), we need to process incoming synchronous
// messages right away because it could be blocking our reply (either
// directly from the same object we're calling, or indirectly through one or
// more other channels).  That means that in SyncContext's OnMessageReceived,
// we need to process sync message right away if we're blocked.  However a
// simple check isn't sufficient, because the listener thread can be in the
// process of calling Send.
// To work around this, when SyncChannel filters a sync message, it sets
// an event that the listener thread waits on during its Send() call.  This
// allows us to dispatch incoming sync messages when blocked.  The race
// condition is handled because if Send is in the process of being called, it
// will check the event.  In case the listener thread isn't sending a message,
// we queue a task on the listener thread to dispatch the received messages.
// The messages are stored in this queue object that's shared among all
// SyncChannel objects on the same thread (since one object can receive a
// sync message while another one is blocked).

class SyncChannel::ReceivedSyncMsgQueue :
    public base::RefCountedThreadSafe<ReceivedSyncMsgQueue> {
 public:
  // Returns the ReceivedSyncMsgQueue instance for this thread, creating one
  // if necessary.  Call RemoveContext on the same thread when done.
  static ReceivedSyncMsgQueue* AddContext() {
    // We want one ReceivedSyncMsgQueue per listener thread (i.e. since multiple
    // SyncChannel objects can block the same thread).
    ReceivedSyncMsgQueue* rv = lazy_tls_ptr_.Pointer()->Get();
    if (!rv) {
      rv = new ReceivedSyncMsgQueue();
      ReceivedSyncMsgQueue::lazy_tls_ptr_.Pointer()->Set(rv);
    }
    rv->listener_count_++;
    return rv;
  }

  // Called on IPC thread when a synchronous message or reply arrives.
  void QueueMessage(const Message& msg, SyncChannel::SyncContext* context) {
    bool was_task_pending;
    {
      base::AutoLock auto_lock(message_lock_);

      was_task_pending = task_pending_;
      task_pending_ = true;

      // We set the event in case the listener thread is blocked (or is about
      // to). In case it's not, the PostTask dispatches the messages.
      message_queue_.push_back(QueuedMessage(new Message(msg), context));
      message_queue_version_++;
    }

    dispatch_event_.Signal();
    if (!was_task_pending) {
      listener_task_runner_->PostTask(
          FROM_HERE, base::Bind(&ReceivedSyncMsgQueue::DispatchMessagesTask,
                                this, base::RetainedRef(context)));
    }
  }

  void QueueReply(const Message &msg, SyncChannel::SyncContext* context) {
    received_replies_.push_back(QueuedMessage(new Message(msg), context));
  }

  // Called on the listener's thread to process any queues synchronous
  // messages.
  void DispatchMessagesTask(SyncContext* context) {
    {
      base::AutoLock auto_lock(message_lock_);
      task_pending_ = false;
    }
    context->DispatchMessages();
  }

  void DispatchMessages(SyncContext* dispatching_context) {
    bool first_time = true;
    uint32_t expected_version = 0;
    SyncMessageQueue::iterator it;
    while (true) {
      Message* message = NULL;
      scoped_refptr<SyncChannel::SyncContext> context;
      {
        base::AutoLock auto_lock(message_lock_);
        if (first_time || message_queue_version_ != expected_version) {
          it = message_queue_.begin();
          first_time = false;
        }
        for (; it != message_queue_.end(); it++) {
          int message_group = it->context->restrict_dispatch_group();
          if (message_group == kRestrictDispatchGroup_None ||
              message_group == dispatching_context->restrict_dispatch_group()) {
            message = it->message;
            context = it->context;
            it = message_queue_.erase(it);
            message_queue_version_++;
            expected_version = message_queue_version_;
            break;
          }
        }
      }

      if (message == NULL)
        break;
      context->OnDispatchMessage(*message);
      delete message;
    }
  }

  // SyncChannel calls this in its destructor.
  void RemoveContext(SyncContext* context) {
    base::AutoLock auto_lock(message_lock_);

    SyncMessageQueue::iterator iter = message_queue_.begin();
    while (iter != message_queue_.end()) {
      if (iter->context.get() == context) {
        delete iter->message;
        iter = message_queue_.erase(iter);
        message_queue_version_++;
      } else {
        iter++;
      }
    }

    if (--listener_count_ == 0) {
      DCHECK(lazy_tls_ptr_.Pointer()->Get());
      lazy_tls_ptr_.Pointer()->Set(NULL);
    }
  }

  WaitableEvent* dispatch_event() { return &dispatch_event_; }
  base::SingleThreadTaskRunner* listener_task_runner() {
    return listener_task_runner_.get();
  }

  // Holds a pointer to the per-thread ReceivedSyncMsgQueue object.
  static base::LazyInstance<base::ThreadLocalPointer<ReceivedSyncMsgQueue> >
      lazy_tls_ptr_;

  // Called on the ipc thread to check if we can unblock any current Send()
  // calls based on a queued reply.
  void DispatchReplies() {
    for (size_t i = 0; i < received_replies_.size(); ++i) {
      Message* message = received_replies_[i].message;
      if (received_replies_[i].context->TryToUnblockListener(message)) {
        delete message;
        received_replies_.erase(received_replies_.begin() + i);
        return;
      }
    }
  }

  base::WaitableEventWatcher* top_send_done_watcher() {
    return top_send_done_watcher_;
  }

  void set_top_send_done_watcher(base::WaitableEventWatcher* watcher) {
    top_send_done_watcher_ = watcher;
  }

 private:
  friend class base::RefCountedThreadSafe<ReceivedSyncMsgQueue>;

  // See the comment in SyncChannel::SyncChannel for why this event is created
  // as manual reset.
  ReceivedSyncMsgQueue()
      : message_queue_version_(0),
        dispatch_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
        listener_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        task_pending_(false),
        listener_count_(0),
        top_send_done_watcher_(NULL) {}

  ~ReceivedSyncMsgQueue() {}

  // Holds information about a queued synchronous message or reply.
  struct QueuedMessage {
    QueuedMessage(Message* m, SyncContext* c) : message(m), context(c) { }
    Message* message;
    scoped_refptr<SyncChannel::SyncContext> context;
  };

  typedef std::list<QueuedMessage> SyncMessageQueue;
  SyncMessageQueue message_queue_;
  uint32_t message_queue_version_;  // Used to signal DispatchMessages to rescan

  std::vector<QueuedMessage> received_replies_;

  // Set when we got a synchronous message that we must respond to as the
  // sender needs its reply before it can reply to our original synchronous
  // message.
  WaitableEvent dispatch_event_;
  scoped_refptr<base::SingleThreadTaskRunner> listener_task_runner_;
  base::Lock message_lock_;
  bool task_pending_;
  int listener_count_;

  // The current send done event watcher for this thread. Used to maintain
  // a local global stack of send done watchers to ensure that nested sync
  // message loops complete correctly.
  base::WaitableEventWatcher* top_send_done_watcher_;
};

base::LazyInstance<base::ThreadLocalPointer<SyncChannel::ReceivedSyncMsgQueue> >
    SyncChannel::ReceivedSyncMsgQueue::lazy_tls_ptr_ =
        LAZY_INSTANCE_INITIALIZER;

SyncChannel::SyncContext::SyncContext(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    WaitableEvent* shutdown_event)
    : ChannelProxy::Context(listener, ipc_task_runner),
      received_sync_msgs_(ReceivedSyncMsgQueue::AddContext()),
      shutdown_event_(shutdown_event),
      restrict_dispatch_group_(kRestrictDispatchGroup_None) {
}

SyncChannel::SyncContext::~SyncContext() {
  while (!deserializers_.empty())
    Pop();
}

// Adds information about an outgoing sync message to the context so that
// we know how to deserialize the reply.  Returns a handle that's set when
// the reply has arrived.
void SyncChannel::SyncContext::Push(SyncMessage* sync_msg) {
  // Create the tracking information for this message. This object is stored
  // by value since all members are pointers that are cheap to copy. These
  // pointers are cleaned up in the Pop() function.
  //
  // The event is created as manual reset because in between Signal and
  // OnObjectSignalled, another Send can happen which would stop the watcher
  // from being called.  The event would get watched later, when the nested
  // Send completes, so the event will need to remain set.
  PendingSyncMsg pending(
      SyncMessage::GetMessageId(*sync_msg), sync_msg->GetReplyDeserializer(),
      new WaitableEvent(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED));
  base::AutoLock auto_lock(deserializers_lock_);
  deserializers_.push_back(pending);
}

bool SyncChannel::SyncContext::Pop() {
  bool result;
  {
    base::AutoLock auto_lock(deserializers_lock_);
    PendingSyncMsg msg = deserializers_.back();
    delete msg.deserializer;
    delete msg.done_event;
    msg.done_event = NULL;
    deserializers_.pop_back();
    result = msg.send_result;
  }

  // We got a reply to a synchronous Send() call that's blocking the listener
  // thread.  However, further down the call stack there could be another
  // blocking Send() call, whose reply we received after we made this last
  // Send() call.  So check if we have any queued replies available that
  // can now unblock the listener thread.
  ipc_task_runner()->PostTask(
      FROM_HERE, base::Bind(&ReceivedSyncMsgQueue::DispatchReplies,
                            received_sync_msgs_.get()));

  return result;
}

WaitableEvent* SyncChannel::SyncContext::GetSendDoneEvent() {
  base::AutoLock auto_lock(deserializers_lock_);
  return deserializers_.back().done_event;
}

WaitableEvent* SyncChannel::SyncContext::GetDispatchEvent() {
  return received_sync_msgs_->dispatch_event();
}

void SyncChannel::SyncContext::DispatchMessages() {
  received_sync_msgs_->DispatchMessages(this);
}

bool SyncChannel::SyncContext::TryToUnblockListener(const Message* msg) {
  base::AutoLock auto_lock(deserializers_lock_);
  if (deserializers_.empty() ||
      !SyncMessage::IsMessageReplyTo(*msg, deserializers_.back().id)) {
    return false;
  }

  if (!msg->is_reply_error()) {
    bool send_result = deserializers_.back().deserializer->
        SerializeOutputParameters(*msg);
    deserializers_.back().send_result = send_result;
    DVLOG_IF(1, !send_result) << "Couldn't deserialize reply message";
  } else {
    DVLOG(1) << "Received error reply";
  }

  base::WaitableEvent* done_event = deserializers_.back().done_event;
  TRACE_EVENT_FLOW_BEGIN0(
      TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
      "SyncChannel::SyncContext::TryToUnblockListener", done_event);

  done_event->Signal();

  return true;
}

void SyncChannel::SyncContext::Clear() {
  CancelPendingSends();
  received_sync_msgs_->RemoveContext(this);
  Context::Clear();
}

bool SyncChannel::SyncContext::OnMessageReceived(const Message& msg) {
  // Give the filters a chance at processing this message.
  if (TryFilters(msg))
    return true;

  if (TryToUnblockListener(&msg))
    return true;

  if (msg.is_reply()) {
    received_sync_msgs_->QueueReply(msg, this);
    return true;
  }

  if (msg.should_unblock()) {
    received_sync_msgs_->QueueMessage(msg, this);
    return true;
  }

  return Context::OnMessageReceivedNoFilter(msg);
}

void SyncChannel::SyncContext::OnChannelError() {
  CancelPendingSends();
  shutdown_watcher_.StopWatching();
  Context::OnChannelError();
}

void SyncChannel::SyncContext::OnChannelOpened() {
  shutdown_watcher_.StartWatching(
      shutdown_event_,
      base::Bind(&SyncChannel::SyncContext::OnWaitableEventSignaled,
                 base::Unretained(this)));
  Context::OnChannelOpened();
}

void SyncChannel::SyncContext::OnChannelClosed() {
  CancelPendingSends();
  shutdown_watcher_.StopWatching();
  Context::OnChannelClosed();
}

void SyncChannel::SyncContext::CancelPendingSends() {
  base::AutoLock auto_lock(deserializers_lock_);
  PendingSyncMessageQueue::iterator iter;
  DVLOG(1) << "Canceling pending sends";
  for (iter = deserializers_.begin(); iter != deserializers_.end(); iter++) {
    TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                            "SyncChannel::SyncContext::CancelPendingSends",
                            iter->done_event);
    iter->done_event->Signal();
  }
}

void SyncChannel::SyncContext::OnWaitableEventSignaled(WaitableEvent* event) {
  if (event == shutdown_event_) {
    // Process shut down before we can get a reply to a synchronous message.
    // Cancel pending Send calls, which will end up setting the send done event.
    CancelPendingSends();
  } else {
    // We got the reply, timed out or the process shutdown.
    DCHECK_EQ(GetSendDoneEvent(), event);
    base::MessageLoop::current()->QuitNow();
  }
}

base::WaitableEventWatcher::EventCallback
    SyncChannel::SyncContext::MakeWaitableEventCallback() {
  return base::Bind(&SyncChannel::SyncContext::OnWaitableEventSignaled, this);
}

// static
std::unique_ptr<SyncChannel> SyncChannel::Create(
    const IPC::ChannelHandle& channel_handle,
    Channel::Mode mode,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    bool create_pipe_now,
    base::WaitableEvent* shutdown_event) {
  std::unique_ptr<SyncChannel> channel =
      Create(listener, ipc_task_runner, shutdown_event);
  channel->Init(channel_handle, mode, create_pipe_now);
  return channel;
}

// static
std::unique_ptr<SyncChannel> SyncChannel::Create(
    std::unique_ptr<ChannelFactory> factory,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    bool create_pipe_now,
    base::WaitableEvent* shutdown_event) {
  std::unique_ptr<SyncChannel> channel =
      Create(listener, ipc_task_runner, shutdown_event);
  channel->Init(std::move(factory), create_pipe_now);
  return channel;
}

// static
std::unique_ptr<SyncChannel> SyncChannel::Create(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    WaitableEvent* shutdown_event) {
  return base::WrapUnique(
      new SyncChannel(listener, ipc_task_runner, shutdown_event));
}

SyncChannel::SyncChannel(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    WaitableEvent* shutdown_event)
    : ChannelProxy(new SyncContext(listener, ipc_task_runner, shutdown_event)) {
  // The current (listener) thread must be distinct from the IPC thread, or else
  // sending synchronous messages will deadlock.
  DCHECK_NE(ipc_task_runner.get(), base::ThreadTaskRunnerHandle::Get().get());
  StartWatching();
}

SyncChannel::~SyncChannel() {
}

void SyncChannel::SetRestrictDispatchChannelGroup(int group) {
  sync_context()->set_restrict_dispatch_group(group);
}

scoped_refptr<SyncMessageFilter> SyncChannel::CreateSyncMessageFilter() {
  scoped_refptr<SyncMessageFilter> filter = new SyncMessageFilter(
      sync_context()->shutdown_event(),
      sync_context()->IsChannelSendThreadSafe());
  AddFilter(filter.get());
  if (!did_init())
    pre_init_sync_message_filters_.push_back(filter);
  return filter;
}

bool SyncChannel::Send(Message* message) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  std::string name;
  Logging::GetInstance()->GetMessageText(message->type(), &name, message, NULL);
  TRACE_EVENT1("ipc", "SyncChannel::Send", "name", name);
#else
  TRACE_EVENT2("ipc", "SyncChannel::Send",
               "class", IPC_MESSAGE_ID_CLASS(message->type()),
               "line", IPC_MESSAGE_ID_LINE(message->type()));
#endif
  if (!message->is_sync()) {
    ChannelProxy::Send(message);
    return true;
  }

  // *this* might get deleted in WaitForReply.
  scoped_refptr<SyncContext> context(sync_context());
  if (context->shutdown_event()->IsSignaled()) {
    DVLOG(1) << "shutdown event is signaled";
    delete message;
    return false;
  }

  SyncMessage* sync_msg = static_cast<SyncMessage*>(message);
  context->Push(sync_msg);
  WaitableEvent* pump_messages_event = sync_msg->pump_messages_event();

  ChannelProxy::Send(message);

  // Wait for reply, or for any other incoming synchronous messages.
  // *this* might get deleted, so only call static functions at this point.
  WaitForReply(context.get(), pump_messages_event);

  TRACE_EVENT_FLOW_END0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                        "SyncChannel::Send", context->GetSendDoneEvent());

  return context->Pop();
}

void SyncChannel::WaitForReply(
    SyncContext* context, WaitableEvent* pump_messages_event) {
  context->DispatchMessages();
  while (true) {
    WaitableEvent* objects[] = {
      context->GetDispatchEvent(),
      context->GetSendDoneEvent(),
      pump_messages_event
    };

    unsigned count = pump_messages_event ? 3: 2;
    size_t result = WaitableEvent::WaitMany(objects, count);
    if (result == 0 /* dispatch event */) {
      // We're waiting for a reply, but we received a blocking synchronous
      // call.  We must process it or otherwise a deadlock might occur.
      context->GetDispatchEvent()->Reset();
      context->DispatchMessages();
      continue;
    }

    if (result == 2 /* pump_messages_event */)
      WaitForReplyWithNestedMessageLoop(context);  // Run a nested message loop.

    break;
  }
}

void SyncChannel::WaitForReplyWithNestedMessageLoop(SyncContext* context) {
  base::WaitableEventWatcher send_done_watcher;

  ReceivedSyncMsgQueue* sync_msg_queue = context->received_sync_msgs();
  DCHECK(sync_msg_queue != NULL);

  base::WaitableEventWatcher* old_send_done_event_watcher =
      sync_msg_queue->top_send_done_watcher();

  base::WaitableEventWatcher::EventCallback old_callback;
  base::WaitableEvent* old_event = NULL;

  // Maintain a local global stack of send done delegates to ensure that
  // nested sync calls complete in the correct sequence, i.e. the
  // outermost call completes first, etc.
  if (old_send_done_event_watcher) {
    old_callback = old_send_done_event_watcher->callback();
    old_event = old_send_done_event_watcher->GetWatchedEvent();
    old_send_done_event_watcher->StopWatching();
  }

  sync_msg_queue->set_top_send_done_watcher(&send_done_watcher);

  send_done_watcher.StartWatching(context->GetSendDoneEvent(),
                                  context->MakeWaitableEventCallback());

  {
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    base::MessageLoop::current()->Run();
  }

  sync_msg_queue->set_top_send_done_watcher(old_send_done_event_watcher);
  if (old_send_done_event_watcher && old_event) {
    old_send_done_event_watcher->StartWatching(old_event, old_callback);
  }
}

void SyncChannel::OnWaitableEventSignaled(WaitableEvent* event) {
  DCHECK(event == sync_context()->GetDispatchEvent());
  // The call to DispatchMessages might delete this object, so reregister
  // the object watcher first.
  event->Reset();
  dispatch_watcher_.StartWatching(event, dispatch_watcher_callback_);
  sync_context()->DispatchMessages();
}

void SyncChannel::StartWatching() {
  // Ideally we only want to watch this object when running a nested message
  // loop.  However, we don't know when it exits if there's another nested
  // message loop running under it or not, so we wouldn't know whether to
  // stop or keep watching.  So we always watch it, and create the event as
  // manual reset since the object watcher might otherwise reset the event
  // when we're doing a WaitMany.
  dispatch_watcher_callback_ =
      base::Bind(&SyncChannel::OnWaitableEventSignaled,
                  base::Unretained(this));
  dispatch_watcher_.StartWatching(sync_context()->GetDispatchEvent(),
                                  dispatch_watcher_callback_);
}

void SyncChannel::OnChannelInit() {
  for (const auto& filter : pre_init_sync_message_filters_) {
    filter->set_is_channel_send_thread_safe(
        context()->IsChannelSendThreadSafe());
  }
  pre_init_sync_message_filters_.clear();
}

}  // namespace IPC
