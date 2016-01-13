// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_SECCOMP_BPF_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_SECCOMP_BPF_LINUX_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace sandbox {
class SandboxBPFPolicy;
}

namespace content {

// This class has two main sets of APIs. One can be used to start the sandbox
// for internal content process types, the other is indirectly exposed as
// a public content/ API and uses a supplied policy.
class SandboxSeccompBPF {
 public:
  // This is the API to enable a seccomp-bpf sandbox for content/
  // process-types:
  // Is the sandbox globally enabled, can anything use it at all ?
  // This looks at global command line flags to see if the sandbox
  // should be enabled at all.
  static bool IsSeccompBPFDesired();
  // Should the sandbox be enabled for process_type ?
  static bool ShouldEnableSeccompBPF(const std::string& process_type);
  // Check if the kernel supports this sandbox. It's useful to "prewarm"
  // this, part of the result will be cached.
  static bool SupportsSandbox();
  // Start the sandbox and apply the policy for process_type, depending on
  // command line switches.
  static bool StartSandbox(const std::string& process_type);

  // This is the API to enable a seccomp-bpf sandbox by using an
  // external policy.
  static bool StartSandboxWithExternalPolicy(
      scoped_ptr<sandbox::SandboxBPFPolicy> policy);
  // The "baseline" policy can be a useful base to build a sandbox policy.
  static scoped_ptr<sandbox::SandboxBPFPolicy> GetBaselinePolicy();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SandboxSeccompBPF);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_SECCOMP_BPF_LINUX_H_

