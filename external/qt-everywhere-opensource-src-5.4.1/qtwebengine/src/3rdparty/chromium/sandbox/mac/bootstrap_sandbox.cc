// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/bootstrap_sandbox.h"

#include <servers/bootstrap.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mach_logging.h"
#include "base/strings/stringprintf.h"
#include "sandbox/mac/launchd_interception_server.h"

namespace sandbox {

const int kNotAPolicy = -1;

// static
scoped_ptr<BootstrapSandbox> BootstrapSandbox::Create() {
  scoped_ptr<BootstrapSandbox> null;  // Used for early returns.
  scoped_ptr<BootstrapSandbox> sandbox(new BootstrapSandbox());
  sandbox->server_.reset(new LaunchdInterceptionServer(sandbox.get()));

  // Check in with launchd to get the receive right for the server that is
  // published in the bootstrap namespace.
  mach_port_t port = MACH_PORT_NULL;
  kern_return_t kr = bootstrap_check_in(bootstrap_port,
      sandbox->server_bootstrap_name().c_str(), &port);
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr)
        << "Failed to bootstrap_check_in the sandbox server.";
    return null.Pass();
  }
  base::mac::ScopedMachReceiveRight scoped_port(port);

  // Start the sandbox server.
  if (sandbox->server_->Initialize(scoped_port.get()))
    ignore_result(scoped_port.release());  // Transferred to server_.
  else
    return null.Pass();

  return sandbox.Pass();
}

BootstrapSandbox::~BootstrapSandbox() {
}

void BootstrapSandbox::RegisterSandboxPolicy(
    int sandbox_policy_id,
    const BootstrapSandboxPolicy& policy) {
  CHECK(IsPolicyValid(policy));
  CHECK_GT(sandbox_policy_id, kNotAPolicy);
  base::AutoLock lock(lock_);
  DCHECK(policies_.find(sandbox_policy_id) == policies_.end());
  policies_.insert(std::make_pair(sandbox_policy_id, policy));
}

void BootstrapSandbox::PrepareToForkWithPolicy(int sandbox_policy_id) {
  base::AutoLock lock(lock_);

  // Verify that this is a real policy.
  CHECK(policies_.find(sandbox_policy_id) != policies_.end());
  CHECK_EQ(kNotAPolicy, effective_policy_id_)
      << "Cannot nest calls to PrepareToForkWithPolicy()";

  // Store the policy for the process we're about to create.
  effective_policy_id_ = sandbox_policy_id;
}

// TODO(rsesek): The |lock_| needs to be taken twice because
// base::LaunchProcess handles both fork+exec, and holding the lock for the
// duration would block servicing of other bootstrap messages. If a better
// LaunchProcess existed (do arbitrary work without layering violations), this
// could be avoided.

void BootstrapSandbox::FinishedFork(base::ProcessHandle handle) {
  base::AutoLock lock(lock_);

  CHECK_NE(kNotAPolicy, effective_policy_id_)
      << "Must PrepareToForkWithPolicy() before FinishedFork()";

  // Apply the policy to the new process.
  if (handle != base::kNullProcessHandle) {
    const auto& existing_process = sandboxed_processes_.find(handle);
    CHECK(existing_process == sandboxed_processes_.end());
    sandboxed_processes_.insert(std::make_pair(handle, effective_policy_id_));
    VLOG(3) << "Bootstrap sandbox enforced for pid " << handle;
  }

  effective_policy_id_ = kNotAPolicy;
}

void BootstrapSandbox::ChildDied(base::ProcessHandle handle) {
  base::AutoLock lock(lock_);
  const auto& it = sandboxed_processes_.find(handle);
  if (it != sandboxed_processes_.end())
    sandboxed_processes_.erase(it);
}

const BootstrapSandboxPolicy* BootstrapSandbox::PolicyForProcess(
    pid_t pid) const {
  base::AutoLock lock(lock_);
  const auto& process = sandboxed_processes_.find(pid);

  // The new child could send bootstrap requests before the parent calls
  // FinishedFork().
  int policy_id = effective_policy_id_;
  if (process != sandboxed_processes_.end()) {
    policy_id = process->second;
  }

  if (policy_id == kNotAPolicy)
    return NULL;

  return &policies_.find(policy_id)->second;
}

BootstrapSandbox::BootstrapSandbox()
    : server_bootstrap_name_(
          base::StringPrintf("%s.sandbox.%d", base::mac::BaseBundleID(),
              getpid())),
      real_bootstrap_port_(MACH_PORT_NULL),
      effective_policy_id_(kNotAPolicy) {
  mach_port_t port = MACH_PORT_NULL;
  kern_return_t kr = task_get_special_port(
      mach_task_self(), TASK_BOOTSTRAP_PORT, &port);
  MACH_CHECK(kr == KERN_SUCCESS, kr);
  real_bootstrap_port_.reset(port);
}

}  // namespace sandbox
