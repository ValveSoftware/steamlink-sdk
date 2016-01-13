// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/task_runner.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/message_filter.h"

using content::BrowserMessageFilter;

namespace content {

class BrowserMessageFilter::Internal : public IPC::MessageFilter {
 public:
  explicit Internal(BrowserMessageFilter* filter) : filter_(filter) {}

 private:
  virtual ~Internal() {}

  // IPC::MessageFilter implementation:
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE {
    filter_->sender_ = sender;
    filter_->OnFilterAdded(sender);
  }

  virtual void OnFilterRemoved() OVERRIDE {
    filter_->OnFilterRemoved();
  }

  virtual void OnChannelClosing() OVERRIDE {
    filter_->sender_ = NULL;
    filter_->OnChannelClosing();
  }

  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE {
    filter_->peer_pid_ = peer_pid;
    filter_->OnChannelConnected(peer_pid);
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    BrowserThread::ID thread = BrowserThread::IO;
    filter_->OverrideThreadForMessage(message, &thread);

    if (thread == BrowserThread::IO) {
      scoped_refptr<base::TaskRunner> runner =
          filter_->OverrideTaskRunnerForMessage(message);
      if (runner.get()) {
        runner->PostTask(
            FROM_HERE,
            base::Bind(
                base::IgnoreResult(&Internal::DispatchMessage), this, message));
        return true;
      }
      return DispatchMessage(message);
    }

    if (thread == BrowserThread::UI &&
        !BrowserMessageFilter::CheckCanDispatchOnUI(message, filter_)) {
      return true;
    }

    BrowserThread::PostTask(
        thread, FROM_HERE,
        base::Bind(
            base::IgnoreResult(&Internal::DispatchMessage), this, message));
    return true;
  }

  virtual bool GetSupportedMessageClasses(
      std::vector<uint32>* supported_message_classes) const OVERRIDE {
    supported_message_classes->assign(
        filter_->message_classes_to_filter().begin(),
        filter_->message_classes_to_filter().end());
    return true;
  }

  // Dispatches a message to the derived class.
  bool DispatchMessage(const IPC::Message& message) {
    bool rv = filter_->OnMessageReceived(message);
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO) || rv) <<
        "Must handle messages that were dispatched to another thread!";
    return rv;
  }

  scoped_refptr<BrowserMessageFilter> filter_;

  DISALLOW_COPY_AND_ASSIGN(Internal);
};

BrowserMessageFilter::BrowserMessageFilter(uint32 message_class_to_filter)
    : internal_(NULL),
      sender_(NULL),
#if defined(OS_WIN)
      peer_handle_(base::kNullProcessHandle),
#endif
      peer_pid_(base::kNullProcessId),
      message_classes_to_filter_(1, message_class_to_filter) {}

BrowserMessageFilter::BrowserMessageFilter(
    const uint32* message_classes_to_filter,
    size_t num_message_classes_to_filter)
    : internal_(NULL),
      sender_(NULL),
#if defined(OS_WIN)
      peer_handle_(base::kNullProcessHandle),
#endif
      peer_pid_(base::kNullProcessId),
      message_classes_to_filter_(
          message_classes_to_filter,
          message_classes_to_filter + num_message_classes_to_filter) {
  DCHECK(num_message_classes_to_filter);
}

base::ProcessHandle BrowserMessageFilter::PeerHandle() {
#if defined(OS_WIN)
  base::AutoLock lock(peer_handle_lock_);
  if (peer_handle_ == base::kNullProcessHandle)
    base::OpenPrivilegedProcessHandle(peer_pid_, &peer_handle_);

  return peer_handle_;
#else
  base::ProcessHandle result = base::kNullProcessHandle;
  base::OpenPrivilegedProcessHandle(peer_pid_, &result);
  return result;
#endif
}


void BrowserMessageFilter::OnDestruct() const {
  delete this;
}

bool BrowserMessageFilter::Send(IPC::Message* message) {
  if (message->is_sync()) {
    // We don't support sending synchronous messages from the browser.  If we
    // really needed it, we can make this class derive from SyncMessageFilter
    // but it seems better to not allow sending synchronous messages from the
    // browser, since it might allow a corrupt/malicious renderer to hang us.
    NOTREACHED() << "Can't send sync message through BrowserMessageFilter!";
    return false;
  }

  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(base::IgnoreResult(&BrowserMessageFilter::Send), this,
                   message));
    return true;
  }

  if (sender_)
    return sender_->Send(message);

  delete message;
  return false;
}

base::TaskRunner* BrowserMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  return NULL;
}

bool BrowserMessageFilter::CheckCanDispatchOnUI(const IPC::Message& message,
                                                IPC::Sender* sender) {
#if defined(OS_WIN)
  // On Windows there's a potential deadlock with sync messsages going in
  // a circle from browser -> plugin -> renderer -> browser.
  // On Linux we can avoid this by avoiding sync messages from browser->plugin.
  // On Mac we avoid this by not supporting windowed plugins.
  if (message.is_sync() && !message.is_caller_pumping_messages()) {
    // NOTE: IF YOU HIT THIS ASSERT, THE SOLUTION IS ALMOST NEVER TO RUN A
    // NESTED MESSAGE LOOP IN THE RENDERER!!!
    // That introduces reentrancy which causes hard to track bugs.  You should
    // find a way to either turn this into an asynchronous message, or one
    // that can be answered on the IO thread.
    NOTREACHED() << "Can't send sync messages to UI thread without pumping "
        "messages in the renderer or else deadlocks can occur if the page "
        "has windowed plugins! (message type " << message.type() << ")";
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    reply->set_reply_error();
    sender->Send(reply);
    return false;
  }
#endif
  return true;
}

void BrowserMessageFilter::BadMessageReceived() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableKillAfterBadIPC))
    return;

  BrowserChildProcessHostImpl::HistogramBadMessageTerminated(
      PROCESS_TYPE_RENDERER);
  base::KillProcess(PeerHandle(), content::RESULT_CODE_KILLED_BAD_MESSAGE,
                    false);
}

BrowserMessageFilter::~BrowserMessageFilter() {
#if defined(OS_WIN)
  if (peer_handle_ != base::kNullProcessHandle)
    base::CloseProcessHandle(peer_handle_);
#endif
}

IPC::MessageFilter* BrowserMessageFilter::GetFilter() {
  // We create this on demand so that if a filter is used in a unit test but
  // never attached to a channel, we don't leak Internal and this;
  DCHECK(!internal_) << "Should only be called once.";
  internal_ = new Internal(this);
  return internal_;
}

}  // namespace content
