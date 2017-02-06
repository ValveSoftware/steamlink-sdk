// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/message_filter.h"
#include "ipc/message_filter_router.h"

namespace IPC {

//------------------------------------------------------------------------------

ChannelProxy::Context::Context(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner)
    : listener_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      listener_(listener),
      ipc_task_runner_(ipc_task_runner),
      channel_connected_called_(false),
      channel_send_thread_safe_(false),
      message_filter_router_(new MessageFilterRouter()),
      peer_pid_(base::kNullProcessId),
      attachment_broker_endpoint_(false) {
  DCHECK(ipc_task_runner_.get());
  // The Listener thread where Messages are handled must be a separate thread
  // to avoid oversubscribing the IO thread. If you trigger this error, you
  // need to either:
  // 1) Create the ChannelProxy on a different thread, or
  // 2) Just use Channel
  // Note, we currently make an exception for a NULL listener. That usage
  // basically works, but is outside the intent of ChannelProxy. This support
  // will disappear, so please don't rely on it. See crbug.com/364241
  DCHECK(!listener || (ipc_task_runner_.get() != listener_task_runner_.get()));
}

ChannelProxy::Context::~Context() {
}

void ChannelProxy::Context::ClearIPCTaskRunner() {
  ipc_task_runner_ = NULL;
}

void ChannelProxy::Context::CreateChannel(
    std::unique_ptr<ChannelFactory> factory) {
  base::AutoLock l(channel_lifetime_lock_);
  DCHECK(!channel_);
  channel_id_ = factory->GetName();
  channel_ = factory->BuildChannel(this);
  channel_send_thread_safe_ = channel_->IsSendThreadSafe();
  channel_->SetAttachmentBrokerEndpoint(attachment_broker_endpoint_);
}

bool ChannelProxy::Context::TryFilters(const Message& message) {
  DCHECK(message_filter_router_);
#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging* logger = Logging::GetInstance();
  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  if (message_filter_router_->TryFilters(message)) {
    if (message.dispatch_error()) {
      listener_task_runner_->PostTask(
          FROM_HERE, base::Bind(&Context::OnDispatchBadMessage, this, message));
    }
#ifdef IPC_MESSAGE_LOG_ENABLED
    if (logger->Enabled())
      logger->OnPostDispatchMessage(message, channel_id_);
#endif
    return true;
  }
  return false;
}

// Called on the IPC::Channel thread
bool ChannelProxy::Context::OnMessageReceived(const Message& message) {
  // First give a chance to the filters to process this message.
  if (!TryFilters(message))
    OnMessageReceivedNoFilter(message);
  return true;
}

// Called on the IPC::Channel thread
bool ChannelProxy::Context::OnMessageReceivedNoFilter(const Message& message) {
  listener_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Context::OnDispatchMessage, this, message));
  return true;
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelConnected(int32_t peer_pid) {
  // We cache off the peer_pid so it can be safely accessed from both threads.
  peer_pid_ = channel_->GetPeerPID();

  // Add any pending filters.  This avoids a race condition where someone
  // creates a ChannelProxy, calls AddFilter, and then right after starts the
  // peer process.  The IO thread could receive a message before the task to add
  // the filter is run on the IO thread.
  OnAddFilter();

  // See above comment about using listener_task_runner_ here.
  listener_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Context::OnDispatchConnected, this));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelError() {
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelError();

  // See above comment about using listener_task_runner_ here.
  listener_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Context::OnDispatchError, this));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelOpened() {
  DCHECK(channel_ != NULL);

  // Assume a reference to ourselves on behalf of this thread.  This reference
  // will be released when we are closed.
  AddRef();

  if (!channel_->Connect()) {
    OnChannelError();
    return;
  }

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnFilterAdded(channel_.get());
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelClosed() {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "477117 ChannelProxy::Context::OnChannelClosed"));
  // It's okay for IPC::ChannelProxy::Close to be called more than once, which
  // would result in this branch being taken.
  if (!channel_)
    return;

  for (size_t i = 0; i < filters_.size(); ++i) {
    filters_[i]->OnChannelClosing();
    filters_[i]->OnFilterRemoved();
  }

  // We don't need the filters anymore.
  message_filter_router_->Clear();
  filters_.clear();
  // We don't need the lock, because at this point, the listener thread can't
  // access it any more.
  pending_filters_.clear();

  ClearChannel();

  // Balance with the reference taken during startup.  This may result in
  // self-destruction.
  Release();
}

void ChannelProxy::Context::Clear() {
  listener_ = NULL;
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnSendMessage(std::unique_ptr<Message> message) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "477117 ChannelProxy::Context::OnSendMessage"));
  if (!channel_) {
    OnChannelClosed();
    return;
  }

  if (!channel_->Send(message.release()))
    OnChannelError();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnAddFilter() {
  // Our OnChannelConnected method has not yet been called, so we can't be
  // sure that channel_ is valid yet. When OnChannelConnected *is* called,
  // it invokes OnAddFilter, so any pending filter(s) will be added at that
  // time.
  if (peer_pid_ == base::kNullProcessId)
    return;

  std::vector<scoped_refptr<MessageFilter> > new_filters;
  {
    base::AutoLock auto_lock(pending_filters_lock_);
    new_filters.swap(pending_filters_);
  }

  for (size_t i = 0; i < new_filters.size(); ++i) {
    filters_.push_back(new_filters[i]);

    message_filter_router_->AddFilter(new_filters[i].get());

    // The channel has already been created and connected, so we need to
    // inform the filters right now.
    new_filters[i]->OnFilterAdded(channel_.get());
    new_filters[i]->OnChannelConnected(peer_pid_);
  }
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnRemoveFilter(MessageFilter* filter) {
  if (peer_pid_ == base::kNullProcessId) {
    // The channel is not yet connected, so any filters are still pending.
    base::AutoLock auto_lock(pending_filters_lock_);
    for (size_t i = 0; i < pending_filters_.size(); ++i) {
      if (pending_filters_[i].get() == filter) {
        filter->OnFilterRemoved();
        pending_filters_.erase(pending_filters_.begin() + i);
        return;
      }
    }
    return;
  }
  if (!channel_)
    return;  // The filters have already been deleted.

  message_filter_router_->RemoveFilter(filter);

  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i].get() == filter) {
      filter->OnFilterRemoved();
      filters_.erase(filters_.begin() + i);
      return;
    }
  }

  NOTREACHED() << "filter to be removed not found";
}

// Called on the listener's thread
void ChannelProxy::Context::AddFilter(MessageFilter* filter) {
  base::AutoLock auto_lock(pending_filters_lock_);
  pending_filters_.push_back(make_scoped_refptr(filter));
  ipc_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Context::OnAddFilter, this));
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchMessage(const Message& message) {
  if (!listener_)
    return;

  OnDispatchConnected();

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging* logger = Logging::GetInstance();
  if (message.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(message);
    return;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (logger->Enabled())
    logger->OnPostDispatchMessage(message, channel_id_);
#endif
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchConnected() {
  if (channel_connected_called_)
    return;

  channel_connected_called_ = true;
  if (listener_)
    listener_->OnChannelConnected(peer_pid_);
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchError() {
  if (listener_)
    listener_->OnChannelError();
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchBadMessage(const Message& message) {
  if (listener_)
    listener_->OnBadMessageReceived(message);
}

void ChannelProxy::Context::ClearChannel() {
  base::AutoLock l(channel_lifetime_lock_);
  channel_.reset();
}

void ChannelProxy::Context::SendFromThisThread(Message* message) {
  base::AutoLock l(channel_lifetime_lock_);
  if (!channel_)
    return;
  DCHECK(channel_->IsSendThreadSafe());
  channel_->Send(message);
}

void ChannelProxy::Context::Send(Message* message) {
  if (channel_send_thread_safe_) {
    SendFromThisThread(message);
    return;
  }

  ipc_task_runner()->PostTask(
      FROM_HERE, base::Bind(&ChannelProxy::Context::OnSendMessage, this,
                            base::Passed(base::WrapUnique(message))));
}

bool ChannelProxy::Context::IsChannelSendThreadSafe() const {
  return channel_send_thread_safe_;
}

//-----------------------------------------------------------------------------

// static
std::unique_ptr<ChannelProxy> ChannelProxy::Create(
    const IPC::ChannelHandle& channel_handle,
    Channel::Mode mode,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  std::unique_ptr<ChannelProxy> channel(
      new ChannelProxy(listener, ipc_task_runner));
  channel->Init(channel_handle, mode, true);
  return channel;
}

// static
std::unique_ptr<ChannelProxy> ChannelProxy::Create(
    std::unique_ptr<ChannelFactory> factory,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  std::unique_ptr<ChannelProxy> channel(
      new ChannelProxy(listener, ipc_task_runner));
  channel->Init(std::move(factory), true);
  return channel;
}

ChannelProxy::ChannelProxy(Context* context)
    : context_(context),
      did_init_(false) {
#if defined(ENABLE_IPC_FUZZER)
  outgoing_message_filter_ = NULL;
#endif
}

ChannelProxy::ChannelProxy(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner)
    : context_(new Context(listener, ipc_task_runner)), did_init_(false) {
#if defined(ENABLE_IPC_FUZZER)
  outgoing_message_filter_ = NULL;
#endif
}

ChannelProxy::~ChannelProxy() {
  DCHECK(CalledOnValidThread());

  Close();
}

void ChannelProxy::Init(const IPC::ChannelHandle& channel_handle,
                        Channel::Mode mode,
                        bool create_pipe_now) {
#if defined(OS_POSIX)
  // When we are creating a server on POSIX, we need its file descriptor
  // to be created immediately so that it can be accessed and passed
  // to other processes. Forcing it to be created immediately avoids
  // race conditions that may otherwise arise.
  if (mode & Channel::MODE_SERVER_FLAG) {
    create_pipe_now = true;
  }
#endif  // defined(OS_POSIX)
  Init(ChannelFactory::Create(channel_handle, mode), create_pipe_now);
}

void ChannelProxy::Init(std::unique_ptr<ChannelFactory> factory,
                        bool create_pipe_now) {
  DCHECK(CalledOnValidThread());
  DCHECK(!did_init_);

  if (create_pipe_now) {
    // Create the channel immediately.  This effectively sets up the
    // low-level pipe so that the client can connect.  Without creating
    // the pipe immediately, it is possible for a listener to attempt
    // to connect and get an error since the pipe doesn't exist yet.
    context_->CreateChannel(std::move(factory));
  } else {
    context_->ipc_task_runner()->PostTask(
        FROM_HERE, base::Bind(&Context::CreateChannel, context_.get(),
                              base::Passed(&factory)));
  }

  // complete initialization on the background thread
  context_->ipc_task_runner()->PostTask(
      FROM_HERE, base::Bind(&Context::OnChannelOpened, context_.get()));

  did_init_ = true;
  OnChannelInit();
}

void ChannelProxy::Close() {
  DCHECK(CalledOnValidThread());

  // Clear the backpointer to the listener so that any pending calls to
  // Context::OnDispatchMessage or OnDispatchError will be ignored.  It is
  // possible that the channel could be closed while it is receiving messages!
  context_->Clear();

  if (context_->ipc_task_runner()) {
    context_->ipc_task_runner()->PostTask(
        FROM_HERE, base::Bind(&Context::OnChannelClosed, context_.get()));
  }
}

bool ChannelProxy::Send(Message* message) {
  DCHECK(did_init_);

  // TODO(alexeypa): add DCHECK(CalledOnValidThread()) here. Currently there are
  // tests that call Send() from a wrong thread. See http://crbug.com/163523.

#ifdef ENABLE_IPC_FUZZER
  // In IPC fuzzing builds, it is possible to define a filter to apply to
  // outgoing messages. It will either rewrite the message and return a new
  // one, freeing the original, or return the message unchanged.
  if (outgoing_message_filter())
    message = outgoing_message_filter()->Rewrite(message);
#endif

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging::GetInstance()->OnSendMessage(message, context_->channel_id());
#endif

  context_->Send(message);
  return true;
}

void ChannelProxy::AddFilter(MessageFilter* filter) {
  DCHECK(CalledOnValidThread());

  context_->AddFilter(filter);
}

void ChannelProxy::RemoveFilter(MessageFilter* filter) {
  DCHECK(CalledOnValidThread());

  context_->ipc_task_runner()->PostTask(
      FROM_HERE, base::Bind(&Context::OnRemoveFilter, context_.get(),
                            base::RetainedRef(filter)));
}

void ChannelProxy::ClearIPCTaskRunner() {
  DCHECK(CalledOnValidThread());

  context()->ClearIPCTaskRunner();
}

base::ProcessId ChannelProxy::GetPeerPID() const {
  return context_->peer_pid_;
}

void ChannelProxy::OnSetAttachmentBrokerEndpoint() {
  CHECK(!did_init_);
  context()->set_attachment_broker_endpoint(is_attachment_broker_endpoint());
}

#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
// See the TODO regarding lazy initialization of the channel in
// ChannelProxy::Init().
int ChannelProxy::GetClientFileDescriptor() {
  DCHECK(CalledOnValidThread());

  Channel* channel = context_.get()->channel_.get();
  // Channel must have been created first.
  DCHECK(channel) << context_.get()->channel_id_;
  return channel->GetClientFileDescriptor();
}

base::ScopedFD ChannelProxy::TakeClientFileDescriptor() {
  DCHECK(CalledOnValidThread());

  Channel* channel = context_.get()->channel_.get();
  // Channel must have been created first.
  DCHECK(channel) << context_.get()->channel_id_;
  return channel->TakeClientFileDescriptor();
}
#endif

void ChannelProxy::OnChannelInit() {
}

//-----------------------------------------------------------------------------

}  // namespace IPC
