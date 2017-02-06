// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_BOOTSTRAP_SANDBOX_H_
#define SANDBOX_MAC_BOOTSTRAP_SANDBOX_H_

#include <mach/mach.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/mac/dispatch_source_mach.h"
#include "base/mac/scoped_mach_port.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "sandbox/mac/policy.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

class LaunchdInterceptionServer;
class PreExecDelegate;

// The BootstrapSandbox is a second-layer sandbox for Mac. It is used to limit
// the bootstrap namespace attack surface of child processes. The parent
// process creates an instance of this class and registers policies that it
// can enforce on its children.
//
// With this sandbox, the parent process must create the client using the
// sandbox's PreExecDelegate, which will replace the bootstrap port of the
// child process. Requests from the child that would normally go to launchd
// are filtered based on the specified per-process policies. If a request is
// permitted by the policy, it is forwarded on to launchd for servicing. If it
// is not, then the sandbox will reply with a primitive that does not grant
// additional capabilities to the receiver.
//
// When the parent is ready to fork a new child process with this sandbox
// being enforced, it should use NewClient() to create a PreExecDelegate for
// a sandbox policy ID and set it to the base::LaunchOptions.pre_exec_delegate.
//
// When a child process exits, the parent should call InvalidateClient() to
// clean up any mappings in this class.
//
// All methods of this class may be called from any thread.
class SANDBOX_EXPORT BootstrapSandbox {
 public:
  // Creates a new sandbox manager. Returns NULL on failure.
  static std::unique_ptr<BootstrapSandbox> Create();

  // For use in newly created child processes. Checks in with the bootstrap
  // sandbox manager running in the parent process. |sandbox_server_port| is
  // the Mach send right to the sandbox |check_in_server_| (in the child).
  // |sandbox_token| is the assigned token. On return, |bootstrap_port| is set
  // to a new Mach send right to be used in the child as the task's bootstrap
  // port.
  static bool ClientCheckIn(mach_port_t sandbox_server_port,
                            uint64_t sandbox_token,
                            mach_port_t* bootstrap_port);

  ~BootstrapSandbox();

  // Registers a bootstrap policy associated it with an identifier. The
  // |sandbox_policy_id| must be greater than 0.
  void RegisterSandboxPolicy(int sandbox_policy_id,
                             const BootstrapSandboxPolicy& policy);

  // Creates a new PreExecDelegate to pass to base::LaunchOptions. This will
  // enforce the policy with |sandbox_policy_id| on the new process.
  std::unique_ptr<PreExecDelegate> NewClient(int sandbox_policy_id);

  // If a client did not launch properly, the sandbox provided to the
  // PreExecDelegate should be invalidated using this method.
  void RevokeToken(uint64_t token);

  // Called in the parent when a process has died. It cleans up the references
  // to the process.
  void InvalidateClient(base::ProcessHandle handle);

  // Looks up the policy for a given process ID. If no policy is associated
  // with the |pid|, this returns NULL.
  const BootstrapSandboxPolicy* PolicyForProcess(pid_t pid) const;

  std::string server_bootstrap_name() const { return server_bootstrap_name_; }
  mach_port_t real_bootstrap_port() const { return real_bootstrap_port_.get(); }

 private:
  BootstrapSandbox();

  // Dispatch callout for when a client sends a message on the
  // |check_in_port_|. If the client message is valid, it will assign the
  // client from |awaiting_processes_| to |sandboxed_processes_|.
  void HandleChildCheckIn();

  // The name in the system bootstrap server by which the |server_|'s port
  // is known.
  const std::string server_bootstrap_name_;

  // The original bootstrap port of the process, which is connected to the
  // real launchd server.
  base::mac::ScopedMachSendRight real_bootstrap_port_;

  // The |lock_| protects all the following variables.
  mutable base::Lock lock_;

  // All the policies that have been registered with this sandbox manager.
  std::map<int, const BootstrapSandboxPolicy> policies_;

  // The association between process ID and sandbox policy ID.
  std::map<base::ProcessHandle, int> sandboxed_processes_;

  // The association between a new process' sandbox token and its policy ID.
  // The entry is removed after the process checks in, and the mapping moves
  // to |sandboxed_processes_|.
  std::map<uint64_t, int> awaiting_processes_;

  // A Mach IPC message server that is used to intercept and filter bootstrap
  // requests.
  std::unique_ptr<LaunchdInterceptionServer> launchd_server_;

  // The port and dispatch source for receiving client check in messages sent
  // via ClientCheckIn().
  base::mac::ScopedMachReceiveRight check_in_port_;
  std::unique_ptr<base::DispatchSourceMach> check_in_server_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_BOOTSTRAP_SANDBOX_H_
