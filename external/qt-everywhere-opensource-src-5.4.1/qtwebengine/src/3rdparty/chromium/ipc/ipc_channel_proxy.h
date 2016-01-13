// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_PROXY_H_
#define IPC_IPC_CHANNEL_PROXY_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {

class MessageFilter;
class MessageFilterRouter;
class SendCallbackHelper;

//-----------------------------------------------------------------------------
// IPC::ChannelProxy
//
// This class is a helper class that is useful when you wish to run an IPC
// channel on a background thread.  It provides you with the option of either
// handling IPC messages on that background thread or having them dispatched to
// your main thread (the thread on which the IPC::ChannelProxy is created).
//
// The API for an IPC::ChannelProxy is very similar to that of an IPC::Channel.
// When you send a message to an IPC::ChannelProxy, the message is routed to
// the background thread, where it is then passed to the IPC::Channel's Send
// method.  This means that you can send a message from your thread and your
// message will be sent over the IPC channel when possible instead of being
// delayed until your thread returns to its message loop.  (Often IPC messages
// will queue up on the IPC::Channel when there is a lot of traffic, and the
// channel will not get cycles to flush its message queue until the thread, on
// which it is running, returns to its message loop.)
//
// An IPC::ChannelProxy can have a MessageFilter associated with it, which will
// be notified of incoming messages on the IPC::Channel's thread.  This gives
// the consumer of IPC::ChannelProxy the ability to respond to incoming
// messages on this background thread instead of on their own thread, which may
// be bogged down with other processing.  The result can be greatly improved
// latency for messages that can be handled on a background thread.
//
// The consumer of IPC::ChannelProxy is responsible for allocating the Thread
// instance where the IPC::Channel will be created and operated.
//
class IPC_EXPORT ChannelProxy : public Sender, public base::NonThreadSafe {
 public:
  // Initializes a channel proxy.  The channel_handle and mode parameters are
  // passed directly to the underlying IPC::Channel.  The listener is called on
  // the thread that creates the ChannelProxy.  The filter's OnMessageReceived
  // method is called on the thread where the IPC::Channel is running.  The
  // filter may be null if the consumer is not interested in handling messages
  // on the background thread.  Any message not handled by the filter will be
  // dispatched to the listener.  The given task runner correspond to a thread
  // on which IPC::Channel is created and used (e.g. IO thread).
  static scoped_ptr<ChannelProxy> Create(
      const IPC::ChannelHandle& channel_handle,
      Channel::Mode mode,
      Listener* listener,
      base::SingleThreadTaskRunner* ipc_task_runner);

  virtual ~ChannelProxy();

  // Initializes the channel proxy. Only call this once to initialize a channel
  // proxy that was not initialized in its constructor. If create_pipe_now is
  // true, the pipe is created synchronously. Otherwise it's created on the IO
  // thread.
  void Init(const IPC::ChannelHandle& channel_handle, Channel::Mode mode,
            bool create_pipe_now);

  // Close the IPC::Channel.  This operation completes asynchronously, once the
  // background thread processes the command to close the channel.  It is ok to
  // call this method multiple times.  Redundant calls are ignored.
  //
  // WARNING: MessageFilter objects held by the ChannelProxy is also
  // released asynchronously, and it may in fact have its final reference
  // released on the background thread.  The caller should be careful to deal
  // with / allow for this possibility.
  void Close();

  // Send a message asynchronously.  The message is routed to the background
  // thread where it is passed to the IPC::Channel's Send method.
  virtual bool Send(Message* message) OVERRIDE;

  // Used to intercept messages as they are received on the background thread.
  //
  // Ordinarily, messages sent to the ChannelProxy are routed to the matching
  // listener on the worker thread.  This API allows code to intercept messages
  // before they are sent to the worker thread.
  // If you call this before the target process is launched, then you're
  // guaranteed to not miss any messages.  But if you call this anytime after,
  // then some messages might be missed since the filter is added internally on
  // the IO thread.
  void AddFilter(MessageFilter* filter);
  void RemoveFilter(MessageFilter* filter);

  // Called to clear the pointer to the IPC task runner when it's going away.
  void ClearIPCTaskRunner();

  // Get the process ID for the connected peer.
  // Returns base::kNullProcessId if the peer is not connected yet.
  base::ProcessId GetPeerPID() const { return context_->peer_pid_; }

#if defined(OS_POSIX) && !defined(OS_NACL)
  // Calls through to the underlying channel's methods.
  int GetClientFileDescriptor();
  int TakeClientFileDescriptor();
#endif  // defined(OS_POSIX)

 protected:
  class Context;
  // A subclass uses this constructor if it needs to add more information
  // to the internal state.
  ChannelProxy(Context* context);

  ChannelProxy(Listener* listener,
               base::SingleThreadTaskRunner* ipc_task_runner);

  // Used internally to hold state that is referenced on the IPC thread.
  class Context : public base::RefCountedThreadSafe<Context>,
                  public Listener {
   public:
    Context(Listener* listener, base::SingleThreadTaskRunner* ipc_thread);
    void ClearIPCTaskRunner();
    base::SingleThreadTaskRunner* ipc_task_runner() const {
      return ipc_task_runner_.get();
    }
    const std::string& channel_id() const { return channel_id_; }

    // Dispatches a message on the listener thread.
    void OnDispatchMessage(const Message& message);

   protected:
    friend class base::RefCountedThreadSafe<Context>;
    virtual ~Context();

    // IPC::Listener methods:
    virtual bool OnMessageReceived(const Message& message) OVERRIDE;
    virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
    virtual void OnChannelError() OVERRIDE;

    // Like OnMessageReceived but doesn't try the filters.
    bool OnMessageReceivedNoFilter(const Message& message);

    // Gives the filters a chance at processing |message|.
    // Returns true if the message was processed, false otherwise.
    bool TryFilters(const Message& message);

    // Like Open and Close, but called on the IPC thread.
    virtual void OnChannelOpened();
    virtual void OnChannelClosed();

    // Called on the consumers thread when the ChannelProxy is closed.  At that
    // point the consumer is telling us that they don't want to receive any
    // more messages, so we honor that wish by forgetting them!
    virtual void Clear();

   private:
    friend class ChannelProxy;
    friend class SendCallbackHelper;

    // Create the Channel
    void CreateChannel(const IPC::ChannelHandle& channel_handle,
                       const Channel::Mode& mode);

    // Methods called on the IO thread.
    void OnSendMessage(scoped_ptr<Message> message_ptr);
    void OnAddFilter();
    void OnRemoveFilter(MessageFilter* filter);

    // Methods called on the listener thread.
    void AddFilter(MessageFilter* filter);
    void OnDispatchConnected();
    void OnDispatchError();
    void OnDispatchBadMessage(const Message& message);

    scoped_refptr<base::SingleThreadTaskRunner> listener_task_runner_;
    Listener* listener_;

    // List of filters.  This is only accessed on the IPC thread.
    std::vector<scoped_refptr<MessageFilter> > filters_;
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

    // Note, channel_ may be set on the Listener thread or the IPC thread.
    // But once it has been set, it must only be read or cleared on the IPC
    // thread.
    scoped_ptr<Channel> channel_;
    std::string channel_id_;
    bool channel_connected_called_;

    // Routes a given message to a proper subset of |filters_|, depending
    // on which message classes a filter might support.
    scoped_ptr<MessageFilterRouter> message_filter_router_;

    // Holds filters between the AddFilter call on the listerner thread and the
    // IPC thread when they're added to filters_.
    std::vector<scoped_refptr<MessageFilter> > pending_filters_;
    // Lock for pending_filters_.
    base::Lock pending_filters_lock_;

    // Cached copy of the peer process ID. Set on IPC but read on both IPC and
    // listener threads.
    base::ProcessId peer_pid_;
  };

  Context* context() { return context_.get(); }

 private:
  friend class SendCallbackHelper;

  // By maintaining this indirection (ref-counted) to our internal state, we
  // can safely be destroyed while the background thread continues to do stuff
  // that involves this data.
  scoped_refptr<Context> context_;

  // Whether the channel has been initialized.
  bool did_init_;
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_PROXY_H_
