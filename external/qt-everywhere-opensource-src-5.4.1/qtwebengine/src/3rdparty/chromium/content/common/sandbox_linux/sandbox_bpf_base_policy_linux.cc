// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"

#include <errno.h>

#include "base/logging.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"

namespace content {

namespace {

// The errno used for denied file system access system calls, such as open(2).
static const int kFSDeniedErrno = EPERM;

}  // namespace.

SandboxBPFBasePolicy::SandboxBPFBasePolicy()
    : baseline_policy_(new sandbox::BaselinePolicy(kFSDeniedErrno)) {}
SandboxBPFBasePolicy::~SandboxBPFBasePolicy() {}

ErrorCode SandboxBPFBasePolicy::EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                                int system_call_number) const {
  DCHECK(baseline_policy_);
  return baseline_policy_->EvaluateSyscall(sandbox_compiler,
                                           system_call_number);
}

bool SandboxBPFBasePolicy::PreSandboxHook() {
  return true;
}

int SandboxBPFBasePolicy::GetFSDeniedErrno() {
  return kFSDeniedErrno;
}

}  // namespace content.
