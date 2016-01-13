// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_BOOTSTRAP_SANDBOX_H_
#define SANDBOX_MAC_BOOTSTRAP_SANDBOX_H_

#include <mach/mach.h>

#include <map>
#include <string>

#include "base/mac/scoped_mach_port.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "sandbox/mac/policy.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

class LaunchdInterceptionServer;

// The BootstrapSandbox is a second-layer sandbox for Mac. It is used to limit
// the bootstrap namespace attack surface of child processes. The parent
// process creates an instance of this class and registers policies that it
// can enforce on its children.
//
// With this sandbox, the parent process must replace the bootstrap port prior
// to the sandboxed target's execution. This should be done by setting the
// base::LaunchOptions.replacement_bootstrap_name to the
// server_bootstrap_name() of this class. Requests from the child that would
// normally go to launchd are filtered based on the specified per-process
// policies. If a request is permitted by the policy, it is forwarded on to
// launchd for servicing. If it is not, then the sandbox will reply with a
// primitive that does not grant additional capabilities to the receiver.
//
// Clients that which to use the sandbox must inform it of the creation and
// death of child processes for which the sandbox should be enforced. The
// client of the sandbox is intended to be an unsandboxed parent process that
// fork()s sandboxed (and other unsandboxed) child processes.
//
// When the parent is ready to fork a new child process with this sandbox
// being enforced, it should use the pair of methods PrepareToForkWithPolicy()
// and FinishedFork(), and call fork() between them. The first method will
// set the policy for the new process, and the second will finialize the
// association between the process ID and sandbox policy ID.
//
// All methods of this class may be called from any thread, but
// PrepareToForkWithPolicy() and FinishedFork() must be non-nested and balanced.
class SANDBOX_EXPORT BootstrapSandbox {
 public:
  // Creates a new sandbox manager. Returns NULL on failure.
  static scoped_ptr<BootstrapSandbox> Create();

  ~BootstrapSandbox();

  // Registers a bootstrap policy associated it with an identifier. The
  // |sandbox_policy_id| must be greater than 0.
  void RegisterSandboxPolicy(int sandbox_policy_id,
                             const BootstrapSandboxPolicy& policy);

  // Called in the parent prior to fork()ing a child. The policy registered
  // to |sandbox_policy_id| will be enforced on the new child. This must be
  // followed by a call to FinishedFork().
  void PrepareToForkWithPolicy(int sandbox_policy_id);

  // Called in the parent after fork()ing a child. It records the |handle|
  // and associates it with the specified-above |sandbox_policy_id|.
  // If fork() failed and a new child was not created, pass kNullProcessHandle.
  void FinishedFork(base::ProcessHandle handle);

  // Called in the parent when a process has died. It cleans up the references
  // to the process.
  void ChildDied(base::ProcessHandle handle);

  // Looks up the policy for a given process ID. If no policy is associated
  // with the |pid|, this returns NULL.
  const BootstrapSandboxPolicy* PolicyForProcess(pid_t pid) const;

  std::string server_bootstrap_name() const { return server_bootstrap_name_; }
  mach_port_t real_bootstrap_port() const { return real_bootstrap_port_; }

 private:
  BootstrapSandbox();

  // A Mach IPC message server that is used to intercept and filter bootstrap
  // requests.
  scoped_ptr<LaunchdInterceptionServer> server_;

  // The name in the system bootstrap server by which the |server_|'s port
  // is known.
  const std::string server_bootstrap_name_;

  // The original bootstrap port of the process, which is connected to the
  // real launchd server.
  base::mac::ScopedMachSendRight real_bootstrap_port_;

  // The |lock_| protects all the following variables.
  mutable base::Lock lock_;

  // The sandbox_policy_id that will be enforced for the new child.
  int effective_policy_id_;

  // All the policies that have been registered with this sandbox manager.
  std::map<int, const BootstrapSandboxPolicy> policies_;

  // The association between process ID and sandbox policy ID.
  std::map<base::ProcessHandle, int> sandboxed_processes_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_BOOTSTRAP_SANDBOX_H_
