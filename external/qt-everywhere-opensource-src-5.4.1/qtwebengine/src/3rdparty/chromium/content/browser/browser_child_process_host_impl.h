// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_

#include <list>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/power_monitor_message_broadcaster.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/child_process_host_delegate.h"

namespace base {
class CommandLine;
}

namespace content {

class BrowserChildProcessHostIterator;
class BrowserChildProcessObserver;
class BrowserMessageFilter;

// Plugins/workers and other child processes that live on the IO thread use this
// class. RenderProcessHostImpl is the main exception that doesn't use this
/// class because it lives on the UI thread.
class CONTENT_EXPORT BrowserChildProcessHostImpl
    : public BrowserChildProcessHost,
      public NON_EXPORTED_BASE(ChildProcessHostDelegate),
      public ChildProcessLauncher::Client {
 public:
  BrowserChildProcessHostImpl(
      int process_type,
      BrowserChildProcessHostDelegate* delegate);
  virtual ~BrowserChildProcessHostImpl();

  // Terminates all child processes and deletes each BrowserChildProcessHost
  // instance.
  static void TerminateAll();

  // BrowserChildProcessHost implementation:
  virtual bool Send(IPC::Message* message) OVERRIDE;
  virtual void Launch(
      SandboxedProcessLauncherDelegate* delegate,
      base::CommandLine* cmd_line) OVERRIDE;
  virtual const ChildProcessData& GetData() const OVERRIDE;
  virtual ChildProcessHost* GetHost() const OVERRIDE;
  virtual base::TerminationStatus GetTerminationStatus(
      bool known_dead, int* exit_code) OVERRIDE;
  virtual void SetName(const base::string16& name) OVERRIDE;
  virtual void SetHandle(base::ProcessHandle handle) OVERRIDE;

  // ChildProcessHostDelegate implementation:
  virtual bool CanShutdown() OVERRIDE;
  virtual void OnChildDisconnected() OVERRIDE;
  virtual base::ProcessHandle GetHandle() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;
  virtual void OnBadMessageReceived(const IPC::Message& message) OVERRIDE;

  // Removes this host from the host list. Calls ChildProcessHost::ForceShutdown
  void ForceShutdown();

  // Callers can reduce the BrowserChildProcess' priority.
  void SetBackgrounded(bool backgrounded);

  // Controls whether the child process should be terminated on browser
  // shutdown. Default is to always terminate.
  void SetTerminateChildOnShutdown(bool terminate_on_shutdown);

  // Adds an IPC message filter.
  void AddFilter(BrowserMessageFilter* filter);

  // Called when an instance of a particular child is created in a page.
  static void NotifyProcessInstanceCreated(const ChildProcessData& data);

  static void HistogramBadMessageTerminated(int process_type);

  BrowserChildProcessHostDelegate* delegate() const { return delegate_; }

  typedef std::list<BrowserChildProcessHostImpl*> BrowserChildProcessList;
 private:
  friend class BrowserChildProcessHostIterator;
  friend class BrowserChildProcessObserver;

  static BrowserChildProcessList* GetIterator();

  static void AddObserver(BrowserChildProcessObserver* observer);
  static void RemoveObserver(BrowserChildProcessObserver* observer);

  // ChildProcessLauncher::Client implementation.
  virtual void OnProcessLaunched() OVERRIDE;
  virtual void OnProcessLaunchFailed() OVERRIDE;

#if defined(OS_WIN)
  void DeleteProcessWaitableEvent(base::WaitableEvent* event);
  void OnProcessExitedEarly(base::WaitableEvent* event);
#endif

  ChildProcessData data_;
  BrowserChildProcessHostDelegate* delegate_;
  scoped_ptr<ChildProcessHost> child_process_host_;

  scoped_ptr<ChildProcessLauncher> child_process_;

  PowerMonitorMessageBroadcaster power_monitor_message_broadcaster_;

#if defined(OS_WIN)
  // Watches to see if the child process exits before the IPC channel has
  // been connected. Thereafter, its exit is determined by an error on the
  // IPC channel.
  base::WaitableEventWatcher early_exit_watcher_;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_
