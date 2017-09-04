// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOX_LINUX_H_
#define CONTENT_PUBLIC_COMMON_SANDBOX_LINUX_H_

namespace content {

// These form a bitmask which describes the conditions of the Linux sandbox.
// Note: this doesn't strictly give you the current status, it states
// what will be enabled when the relevant processes are initialized.
enum LinuxSandboxStatus {
  // SUID sandbox active.
  kSandboxLinuxSUID = 1 << 0,

  // Sandbox is using a new PID namespace.
  kSandboxLinuxPIDNS = 1 << 1,

  // Sandbox is using a new network namespace.
  kSandboxLinuxNetNS = 1 << 2,

  // seccomp-bpf sandbox active.
  kSandboxLinuxSeccompBPF = 1 << 3,

  // The Yama LSM module is present and enforcing.
  kSandboxLinuxYama = 1 << 4,

  // seccomp-bpf sandbox is active and the kernel supports TSYNC.
  kSandboxLinuxSeccompTSYNC = 1 << 5,

  // User namespace sandbox active.
  kSandboxLinuxUserNS = 1 << 6,

  // A flag that denotes an invalid sandbox status.
  kSandboxLinuxInvalid = 1 << 31,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOX_LINUX_H_
