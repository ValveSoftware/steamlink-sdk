// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_BPF_PPAPI_POLICY_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_BPF_PPAPI_POLICY_LINUX_H_

#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"

namespace content {

// Policy for Pepper plugins such as Flash.
class PpapiProcessPolicy : public SandboxBPFBasePolicy {
 public:
  PpapiProcessPolicy();
  virtual ~PpapiProcessPolicy();

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PpapiProcessPolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_BPF_PPAPI_POLICY_LINUX_H_
