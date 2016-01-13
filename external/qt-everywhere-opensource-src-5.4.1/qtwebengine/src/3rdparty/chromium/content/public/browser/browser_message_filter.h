// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_MESSAGE_FILTER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_MESSAGE_FILTER_H_

#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_channel_proxy.h"

#if defined(OS_WIN)
#include "base/synchronization/lock.h"
#endif

namespace base {
class TaskRunner;
}

namespace IPC {
class MessageFilter;
}

namespace content {
struct BrowserMessageFilterTraits;

// Base class for message filters in the browser process.  You can receive and
// send messages on any thread.
class CONTENT_EXPORT BrowserMessageFilter
    : public base::RefCountedThreadSafe<
          BrowserMessageFilter, BrowserMessageFilterTraits>,
      public IPC::Sender {
 public:
  explicit BrowserMessageFilter(uint32 message_class_to_filter);
  BrowserMessageFilter(const uint32* message_classes_to_filter,
                       size_t num_message_classes_to_filter);

  // These match the corresponding IPC::MessageFilter methods and are always
  // called on the IO thread.
  virtual void OnFilterAdded(IPC::Sender* sender) {}
  virtual void OnFilterRemoved() {}
  virtual void OnChannelClosing() {}
  virtual void OnChannelConnected(int32 peer_pid) {}

  // Called when the message filter is about to be deleted.  This gives
  // derived classes the option of controlling which thread they're deleted
  // on etc.
  virtual void OnDestruct() const;

  // IPC::Sender implementation.  Can be called on any thread.  Can't send sync
  // messages (since we don't want to block the browser on any other process).
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // If you want the given message to be dispatched to your OnMessageReceived on
  // a different thread, there are two options, either
  // OverrideThreadForMessage or OverrideTaskRunnerForMessage.
  // If neither is overriden, the message will be dispatched on the IO thread.

  // If you want the message to be dispatched on a particular well-known
  // browser thread, change |thread| to the id of the target thread
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      BrowserThread::ID* thread) {}

  // If you want the message to be dispatched via the SequencedWorkerPool,
  // return a non-null task runner which will target tasks accordingly.
  // Note: To target the UI thread, please use OverrideThreadForMessage
  // since that has extra checks to avoid deadlocks.
  virtual base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message);

  // Override this to receive messages.
  // Your function will normally be called on the IO thread.  However, if your
  // OverrideXForMessage modifies the thread used to dispatch the message,
  // your function will be called on the requested thread.
  virtual bool OnMessageReceived(const IPC::Message& message) = 0;

  // Can be called on any thread, after OnChannelConnected is called.
  base::ProcessHandle PeerHandle();

  // Can be called on any thread, after OnChannelConnected is called.
  base::ProcessId peer_pid() const { return peer_pid_; }

  void set_peer_pid_for_testing(base::ProcessId peer_pid) {
    peer_pid_ = peer_pid;
  }

  // Checks that the given message can be dispatched on the UI thread, depending
  // on the platform.  If not, returns false and an error ot the sender.
  static bool CheckCanDispatchOnUI(const IPC::Message& message,
                                   IPC::Sender* sender);

  // Call this if a message couldn't be deserialized.  This kills the renderer.
  // Can be called on any thread.
  virtual void BadMessageReceived();

  const std::vector<uint32>& message_classes_to_filter() const {
    return message_classes_to_filter_;
  }

 protected:
  virtual ~BrowserMessageFilter();

 private:
  friend class base::RefCountedThreadSafe<BrowserMessageFilter,
                                          BrowserMessageFilterTraits>;

  class Internal;
  friend class BrowserChildProcessHostImpl;
  friend class BrowserPpapiHost;
  friend class RenderProcessHostImpl;

  // This is private because the only classes that need access to it are made
  // friends above. This is only guaranteed to be valid on creation, after that
  // this class could outlive the filter.
  IPC::MessageFilter* GetFilter();

  // This implements IPC::MessageFilter so that we can hide that from child
  // classes. Internal keeps a reference to this class, which is why there's a
  // weak pointer back. This class could outlive Internal based on what the
  // child class does in its OnDestruct method.
  Internal* internal_;

  IPC::Sender* sender_;
  base::ProcessId peer_pid_;

  std::vector<uint32> message_classes_to_filter_;

#if defined(OS_WIN)
  base::Lock peer_handle_lock_;
  base::ProcessHandle peer_handle_;
#endif
};

struct BrowserMessageFilterTraits {
  static void Destruct(const BrowserMessageFilter* filter) {
    filter->OnDestruct();
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_MESSAGE_FILTER_H_
