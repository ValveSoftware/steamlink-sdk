// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/bpf_ppapi_policy_linux.h"

#include <errno.h>

#include "base/basictypes.h"
#include "build/build_config.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/linux/services/linux_syscalls.h"

using sandbox::SyscallSets;

namespace content {

PpapiProcessPolicy::PpapiProcessPolicy() {}
PpapiProcessPolicy::~PpapiProcessPolicy() {}

ErrorCode PpapiProcessPolicy::EvaluateSyscall(SandboxBPF* sandbox,
                                              int sysno) const {
  switch (sysno) {
    // TODO(jln): restrict prctl.
    case __NR_prctl:
    case __NR_pread64:
    case __NR_pwrite64:
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
    case __NR_sched_getaffinity:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler:
    case __NR_times:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    case __NR_ioctl:
      return ErrorCode(ENOTTY);  // Flash Access.
    default:
      // Default on the baseline policy.
      return SandboxBPFBasePolicy::EvaluateSyscall(sandbox, sysno);
  }
}

}  // namespace content
