// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_BPF_GPU_POLICY_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_BPF_GPU_POLICY_LINUX_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"

namespace sandbox {
class BrokerProcess;
}

namespace content {

class GpuProcessPolicy : public SandboxBPFBasePolicy {
 public:
  GpuProcessPolicy();
  virtual ~GpuProcessPolicy();

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

  virtual bool PreSandboxHook() OVERRIDE;

 protected:
  // Start a broker process to handle open() inside the sandbox.
  // |broker_sandboxer_allocator| is a function pointer which can allocate a
  // suitable sandbox policy for the broker process itself.
  // |read_whitelist_extra| and |write_whitelist_extra| are lists of file
  // names that should be whitelisted by the broker process, in addition to
  // the basic ones.
  void InitGpuBrokerProcess(
      sandbox::SandboxBPFPolicy* (*broker_sandboxer_allocator)(void),
      const std::vector<std::string>& read_whitelist_extra,
      const std::vector<std::string>& write_whitelist_extra);

  sandbox::BrokerProcess* broker_process() { return broker_process_; }

 private:
  // A BrokerProcess is a helper that is started before the sandbox is engaged
  // and will serve requests to access files over an IPC channel. The client of
  // this runs from a SIGSYS handler triggered by the seccomp-bpf sandbox.
  // This should never be destroyed, as after the sandbox is started it is
  // vital to the process.
  // This is allocated by InitGpuBrokerProcess, called from PreSandboxHook(),
  // which executes iff the sandbox is going to be enabled afterwards.
  sandbox::BrokerProcess* broker_process_;
  DISALLOW_COPY_AND_ASSIGN(GpuProcessPolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_BPF_GPU_POLICY_LINUX_H_
