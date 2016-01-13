// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MACH_BROKER_MAC_H_
#define CONTENT_BROWSER_MACH_BROKER_MAC_H_

#include <map>
#include <string>

#include <mach/mach.h>

#include "base/memory/singleton.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {

// On OS X, the mach_port_t of a process is required to collect metrics about
// the process. Running |task_for_pid()| is only allowed for privileged code.
// However, a process has port rights to all its subprocesses, so let the
// browser's child processes send their Mach port to the browser over IPC.
// This way, the brower can at least collect metrics of its child processes,
// which is what it's most interested in anyway.
//
// Mach ports can only be sent over Mach IPC, not over the |socketpair()| that
// the regular IPC system uses. Hence, the child processes open a Mach
// connection shortly after launching and ipc their mach data to the browser
// process. This data is kept in a global |MachBroker| object.
//
// Since this data arrives over a separate channel, it is not available
// immediately after a child process has been started.
class CONTENT_EXPORT MachBroker : public base::ProcessMetrics::PortProvider,
                                  public BrowserChildProcessObserver,
                                  public NotificationObserver {
 public:
  // For use in child processes. This will send the task port of the current
  // process over Mach IPC to the port registered by name (via this class) in
  // the parent process. Returns true if the message was sent successfully
  // and false if otherwise.
  static bool ChildSendTaskPortToParent();

  // Returns the Mach port name to use when sending or receiving messages.
  // Does the Right Thing in the browser and in child processes.
  static std::string GetMachPortName();

  // Returns the global MachBroker.
  static MachBroker* GetInstance();

  // The lock that protects this MachBroker object.  Clients MUST acquire and
  // release this lock around calls to EnsureRunning(), PlaceholderForPid(),
  // and FinalizePid().
  base::Lock& GetLock();

  // Performs any necessary setup that cannot happen in the constructor.
  // Callers MUST acquire the lock given by GetLock() before calling this
  // method (and release the lock afterwards).
  void EnsureRunning();

  // Adds a placeholder to the map for the given pid with MACH_PORT_NULL.
  // Callers are expected to later update the port with FinalizePid().  Callers
  // MUST acquire the lock given by GetLock() before calling this method (and
  // release the lock afterwards).
  void AddPlaceholderForPid(base::ProcessHandle pid);

  // Implement |ProcessMetrics::PortProvider|.
  virtual mach_port_t TaskForPid(base::ProcessHandle process) const OVERRIDE;

  // Implement |BrowserChildProcessObserver|.
  virtual void BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessCrashed(
      const ChildProcessData& data) OVERRIDE;

  // Implement |NotificationObserver|.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;
 private:
  friend class MachBrokerTest;
  friend class MachListenerThreadDelegate;
  friend struct DefaultSingletonTraits<MachBroker>;

  MachBroker();
  virtual ~MachBroker();

  // Updates the mapping for |pid| to include the given |mach_info|.  Does
  // nothing if PlaceholderForPid() has not already been called for the given
  // |pid|.  Callers MUST acquire the lock given by GetLock() before calling
  // this method (and release the lock afterwards).
  void FinalizePid(base::ProcessHandle pid, mach_port_t task_port);

  // Removes all mappings belonging to |pid| from the broker.
  void InvalidatePid(base::ProcessHandle pid);

  // Callback used to register notifications on the UI thread.
  void RegisterNotifications();

  // True if the listener thread has been started.
  bool listener_thread_started_;

  // Used to register for notifications received by NotificationObserver.
  // Accessed only on the UI thread.
  NotificationRegistrar registrar_;

  // Stores mach info for every process in the broker.
  typedef std::map<base::ProcessHandle, mach_port_t> MachMap;
  MachMap mach_map_;

  // Mutex that guards |mach_map_|.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(MachBroker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MACH_BROKER_MAC_H_
