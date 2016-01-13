// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_HELPERS_BASELINE_POLICY_H_
#define SANDBOX_LINUX_SECCOMP_BPF_HELPERS_BASELINE_POLICY_H_

#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

class SandboxBPF;
class SandboxBPFPolicy;

// This is a helper to build seccomp-bpf policies, i.e. policies for a sandbox
// that reduces the Linux kernel's attack surface. Given its nature, it doesn't
// have a clear semantics and is mostly "implementation-defined".
//
// This class implements the SandboxBPFPolicy interface with a "baseline"
// policy for us within Chromium.
// The "baseline" policy is somewhat arbitrary. All Chromium policies are an
// alteration of it, and it represents a reasonable common ground to run most
// code in a sandboxed environment.
// A baseline policy is only valid for the process for which this object was
// instantiated (so do not fork() and use it in a child).
class SANDBOX_EXPORT BaselinePolicy : public SandboxBPFPolicy {
 public:
  BaselinePolicy();
  // |fs_denied_errno| is the errno returned when a filesystem access system
  // call is denied.
  explicit BaselinePolicy(int fs_denied_errno);
  virtual ~BaselinePolicy();

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  int fs_denied_errno_;
  pid_t current_pid_;
  DISALLOW_COPY_AND_ASSIGN(BaselinePolicy);
};

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SECCOMP_BPF_HELPERS_BASELINE_POLICY_H_
