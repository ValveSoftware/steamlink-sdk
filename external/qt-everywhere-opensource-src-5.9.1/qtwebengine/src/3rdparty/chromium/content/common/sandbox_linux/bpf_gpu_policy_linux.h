// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_BPF_GPU_POLICY_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_BPF_GPU_POLICY_LINUX_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"

namespace sandbox {
namespace syscall_broker {
class BrokerFilePermission;
class BrokerProcess;
}
}

namespace content {

class GpuProcessPolicy : public SandboxBPFBasePolicy {
 public:
  GpuProcessPolicy();
  explicit GpuProcessPolicy(bool allow_mincore);
  ~GpuProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

  bool PreSandboxHook() override;

 protected:
  // Start a broker process to handle open() inside the sandbox.
  // |broker_sandboxer_allocator| is a function pointer which can allocate a
  // suitable sandbox policy for the broker process itself.
  // |permissions_extra| is a list of file permissions
  // that should be whitelisted by the broker process, in addition to
  // the basic ones.
  void InitGpuBrokerProcess(
      sandbox::bpf_dsl::Policy* (*broker_sandboxer_allocator)(void),
      const std::vector<sandbox::syscall_broker::BrokerFilePermission>&
          permissions_extra);

  sandbox::syscall_broker::BrokerProcess* broker_process() {
    return broker_process_;
  }

 private:
  // A BrokerProcess is a helper that is started before the sandbox is engaged
  // and will serve requests to access files over an IPC channel. The client of
  // this runs from a SIGSYS handler triggered by the seccomp-bpf sandbox.
  // This should never be destroyed, as after the sandbox is started it is
  // vital to the process.
  // This is allocated by InitGpuBrokerProcess, called from PreSandboxHook(),
  // which executes iff the sandbox is going to be enabled afterwards.
  sandbox::syscall_broker::BrokerProcess* broker_process_;

  // eglCreateWindowSurface() needs mincore().
  bool allow_mincore_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessPolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_BPF_GPU_POLICY_LINUX_H_
