// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/message_filter.h"

#if defined(OS_ANDROID)
#include "content/browser/android/child_process_launcher_android.h"
#endif

using content::BrowserMessageFilter;

namespace content {

class BrowserMessageFilter::Internal : public IPC::MessageFilter {
 public:
  explicit Internal(BrowserMessageFilter* filter) : filter_(filter) {}

 private:
  ~Internal() override {}

  // IPC::MessageFilter implementation:
  void OnFilterAdded(IPC::Sender* sender) override {
    filter_->sender_ = sender;
    filter_->OnFilterAdded(sender);
  }

  void OnFilterRemoved() override { filter_->OnFilterRemoved(); }

  void OnChannelClosing() override {
    filter_->sender_ = nullptr;
    filter_->OnChannelClosing();
  }

  void OnChannelConnected(int32_t peer_pid) override {
    filter_->peer_process_ = base::Process::OpenWithExtraPrivileges(peer_pid);
    filter_->OnChannelConnected(peer_pid);
  }

  bool OnMessageReceived(const IPC::Message& message) override {
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

    BrowserThread::PostTask(
        thread, FROM_HERE,
        base::Bind(
            base::IgnoreResult(&Internal::DispatchMessage), this, message));
    return true;
  }

  bool GetSupportedMessageClasses(
      std::vector<uint32_t>* supported_message_classes) const override {
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

BrowserMessageFilter::BrowserMessageFilter(uint32_t message_class_to_filter)
    : internal_(nullptr),
      sender_(nullptr),
      message_classes_to_filter_(1, message_class_to_filter) {}

BrowserMessageFilter::BrowserMessageFilter(
    const uint32_t* message_classes_to_filter,
    size_t num_message_classes_to_filter)
    : internal_(nullptr),
      sender_(nullptr),
      message_classes_to_filter_(
          message_classes_to_filter,
          message_classes_to_filter + num_message_classes_to_filter) {
  DCHECK(num_message_classes_to_filter);
}

base::ProcessHandle BrowserMessageFilter::PeerHandle() {
  return peer_process_.Handle();
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
  return nullptr;
}

void BrowserMessageFilter::ShutdownForBadMessage() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableKillAfterBadIPC))
    return;

#if defined(OS_ANDROID)
  // Android requires a different approach for killing.
  StopChildProcess(peer_process_.Handle());
#else
  peer_process_.Terminate(content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
#endif

  // Report a crash, since none will be generated by the killed renderer.
  base::debug::DumpWithoutCrashing();

  // Log the renderer kill to the histogram tracking all kills.
  BrowserChildProcessHostImpl::HistogramBadMessageTerminated(
      PROCESS_TYPE_RENDERER);
}

BrowserMessageFilter::~BrowserMessageFilter() {
}

IPC::MessageFilter* BrowserMessageFilter::GetFilter() {
  // We create this on demand so that if a filter is used in a unit test but
  // never attached to a channel, we don't leak Internal and this;
  DCHECK(!internal_) << "Should only be called once.";
  internal_ = new Internal(this);
  return internal_;
}

}  // namespace content
