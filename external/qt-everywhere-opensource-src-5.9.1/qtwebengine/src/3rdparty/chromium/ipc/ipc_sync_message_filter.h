// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_SYNC_MESSAGE_FILTER_H_
#define IPC_IPC_SYNC_MESSAGE_FILTER_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/message_filter.h"
#include "ipc/mojo_event.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}

namespace IPC {
class SyncChannel;

// This MessageFilter allows sending synchronous IPC messages from a thread
// other than the listener thread associated with the SyncChannel.  It does not
// support fancy features that SyncChannel does, such as handling recursion or
// receiving messages while waiting for a response.  Note that this object can
// be used to send simultaneous synchronous messages from different threads.
class IPC_EXPORT SyncMessageFilter : public MessageFilter, public Sender {
 public:
  // Sender implementation.
  bool Send(Message* message) override;

  // MessageFilter implementation.
  void OnFilterAdded(Channel* channel) override;
  void OnChannelError() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const Message& message) override;

  // Binds an associated interface proxy to an interface in the browser process.
  // Interfaces acquired through this method are associated with the IPC Channel
  // and as such retain FIFO with legacy IPC messages.
  //
  // NOTE: This must ONLY be called on the Channel's thread, after
  // OnFilterAdded.
  template <typename Interface>
  void GetRemoteAssociatedInterface(
      mojo::AssociatedInterfacePtr<Interface>* proxy) {
    mojo::AssociatedInterfaceRequest<Interface> request =
        mojo::GetProxy(proxy, &channel_associated_group_);
    GetGenericRemoteAssociatedInterface(Interface::Name_, request.PassHandle());
  }

 protected:
  explicit SyncMessageFilter(base::WaitableEvent* shutdown_event);
  ~SyncMessageFilter() override;

 private:
  class IOMessageLoopObserver;

  friend class SyncChannel;
  friend class IOMessageLoopObserver;

  void SendOnIOThread(Message* message);
  // Signal all the pending sends as done, used in an error condition.
  void SignalAllEvents();

  void OnShutdownEventSignaled(base::WaitableEvent* event);
  void OnIOMessageLoopDestroyed();

  // NOTE: This must ONLY be called on the Channel's thread.
  void GetGenericRemoteAssociatedInterface(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle);

  // The channel to which this filter was added.
  Channel* channel_;

  // The process's main thread.
  scoped_refptr<base::SingleThreadTaskRunner> listener_task_runner_;

  // The message loop where the Channel lives.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  typedef std::set<PendingSyncMsg*> PendingSyncMessages;
  PendingSyncMessages pending_sync_messages_;

  // Messages waiting to be delivered after IO initialization.
  std::vector<std::unique_ptr<Message>> pending_messages_;

  // Locks data members above.
  base::Lock lock_;

  base::WaitableEvent* const shutdown_event_;

  // Used to asynchronously watch |shutdown_event_| on the IO thread and forward
  // to |shutdown_mojo_event_| (see below.)
  base::WaitableEventWatcher shutdown_watcher_;

  // A Mojo event which can be watched for shutdown. Signals are forwarded to
  // this event asynchronously from |shutdown_event_|.
  MojoEvent shutdown_mojo_event_;

  scoped_refptr<IOMessageLoopObserver> io_message_loop_observer_;

  // The AssociatedGroup for the underlying channel, used to construct new
  // associated interface endpoints.
  mojo::AssociatedGroup channel_associated_group_;

  base::WeakPtrFactory<SyncMessageFilter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncMessageFilter);
};

}  // namespace IPC

#endif  // IPC_IPC_SYNC_MESSAGE_FILTER_H_
