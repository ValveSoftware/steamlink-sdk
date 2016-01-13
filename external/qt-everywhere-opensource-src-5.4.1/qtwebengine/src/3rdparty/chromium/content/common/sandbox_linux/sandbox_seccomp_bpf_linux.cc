// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"

#if defined(USE_SECCOMP_BPF)

#include "base/posix/eintr_wrapper.h"
#include "content/common/sandbox_linux/bpf_cros_arm_gpu_policy_linux.h"
#include "content/common/sandbox_linux/bpf_gpu_policy_linux.h"
#include "content/common/sandbox_linux/bpf_ppapi_policy_linux.h"
#include "content/common/sandbox_linux/bpf_renderer_policy_linux.h"
#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/linux_syscalls.h"

using sandbox::BaselinePolicy;
using sandbox::SyscallSets;

#else

// Make sure that seccomp-bpf does not get disabled by mistake. Also make sure
// that we think twice about this when adding a new architecture.
#if !defined(ARCH_CPU_MIPS_FAMILY) && !defined(ARCH_CPU_ARM64)
#error "Seccomp-bpf disabled on supported architecture!"
#endif  // !defined(ARCH_CPU_MIPS_FAMILY) && !defined(ARCH_CPU_ARM64)

#endif  //

namespace content {

#if defined(USE_SECCOMP_BPF)
namespace {

void StartSandboxWithPolicy(sandbox::SandboxBPFPolicy* policy);

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureArm() {
#if defined(__arm__)
  return true;
#else
  return false;
#endif
}

class BlacklistDebugAndNumaPolicy : public SandboxBPFBasePolicy {
 public:
  BlacklistDebugAndNumaPolicy() {}
  virtual ~BlacklistDebugAndNumaPolicy() {}

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlacklistDebugAndNumaPolicy);
};

ErrorCode BlacklistDebugAndNumaPolicy::EvaluateSyscall(SandboxBPF* sandbox,
                                                       int sysno) const {
  if (!SandboxBPF::IsValidSyscallNumber(sysno)) {
    // TODO(jln) we should not have to do that in a trivial policy.
    return ErrorCode(ENOSYS);
  }
  if (SyscallSets::IsDebug(sysno) || SyscallSets::IsNuma(sysno))
    return sandbox->Trap(sandbox::CrashSIGSYS_Handler, NULL);

  return ErrorCode(ErrorCode::ERR_ALLOWED);
}

class AllowAllPolicy : public SandboxBPFBasePolicy {
 public:
  AllowAllPolicy() {}
  virtual ~AllowAllPolicy() {}

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AllowAllPolicy);
};

// Allow all syscalls.
// This will still deny x32 or IA32 calls in 64 bits mode or
// 64 bits system calls in compatibility mode.
ErrorCode AllowAllPolicy::EvaluateSyscall(SandboxBPF*, int sysno) const {
  if (!SandboxBPF::IsValidSyscallNumber(sysno)) {
    // TODO(jln) we should not have to do that in a trivial policy.
    return ErrorCode(ENOSYS);
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

// If a BPF policy is engaged for |process_type|, run a few sanity checks.
void RunSandboxSanityChecks(const std::string& process_type) {
  if (process_type == switches::kRendererProcess ||
      process_type == switches::kWorkerProcess ||
      process_type == switches::kGpuProcess ||
      process_type == switches::kPpapiPluginProcess) {
    int syscall_ret;
    errno = 0;

    // Without the sandbox, this would EBADF.
    syscall_ret = fchmod(-1, 07777);
    CHECK_EQ(-1, syscall_ret);
    CHECK_EQ(EPERM, errno);

    // Run most of the sanity checks only in DEBUG mode to avoid a perf.
    // impact.
#if !defined(NDEBUG)
    // open() must be restricted.
    syscall_ret = open("/etc/passwd", O_RDONLY);
    CHECK_EQ(-1, syscall_ret);
    CHECK_EQ(SandboxBPFBasePolicy::GetFSDeniedErrno(), errno);

    // We should never allow the creation of netlink sockets.
    syscall_ret = socket(AF_NETLINK, SOCK_DGRAM, 0);
    CHECK_EQ(-1, syscall_ret);
    CHECK_EQ(EPERM, errno);
#endif  // !defined(NDEBUG)
  }
}


// This function takes ownership of |policy|.
void StartSandboxWithPolicy(sandbox::SandboxBPFPolicy* policy) {
  // Starting the sandbox is a one-way operation. The kernel doesn't allow
  // us to unload a sandbox policy after it has been started. Nonetheless,
  // in order to make the use of the "Sandbox" object easier, we allow for
  // the object to be destroyed after the sandbox has been started. Note that
  // doing so does not stop the sandbox.
  SandboxBPF sandbox;
  sandbox.SetSandboxPolicy(policy);
  CHECK(sandbox.StartSandbox(SandboxBPF::PROCESS_SINGLE_THREADED));
}

// nacl_helper needs to be tiny and includes only part of content/
// in its dependencies. Make sure to not link things that are not needed.
#if !defined(IN_NACL_HELPER)
scoped_ptr<SandboxBPFBasePolicy> GetGpuProcessSandbox() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool allow_sysv_shm = false;
  if (command_line.HasSwitch(switches::kGpuSandboxAllowSysVShm)) {
    DCHECK(IsArchitectureArm());
    allow_sysv_shm = true;
  }

  if (IsChromeOS() && IsArchitectureArm()) {
    return scoped_ptr<SandboxBPFBasePolicy>(
        new CrosArmGpuProcessPolicy(allow_sysv_shm));
  } else {
    return scoped_ptr<SandboxBPFBasePolicy>(new GpuProcessPolicy);
  }
}

// Initialize the seccomp-bpf sandbox.
bool StartBPFSandbox(const CommandLine& command_line,
                     const std::string& process_type) {
  scoped_ptr<SandboxBPFBasePolicy> policy;

  if (process_type == switches::kGpuProcess) {
    policy.reset(GetGpuProcessSandbox().release());
  } else if (process_type == switches::kRendererProcess ||
             process_type == switches::kWorkerProcess) {
    policy.reset(new RendererProcessPolicy);
  } else if (process_type == switches::kPpapiPluginProcess) {
    policy.reset(new PpapiProcessPolicy);
  } else if (process_type == switches::kUtilityProcess) {
    policy.reset(new BlacklistDebugAndNumaPolicy);
  } else {
    NOTREACHED();
    policy.reset(new AllowAllPolicy);
  }

  CHECK(policy->PreSandboxHook());
  StartSandboxWithPolicy(policy.release());

  RunSandboxSanityChecks(process_type);
  return true;
}
#else  // defined(IN_NACL_HELPER)
bool StartBPFSandbox(const CommandLine& command_line,
                     const std::string& process_type) {
  NOTREACHED();
  // Avoid -Wunused-function with no-op code.
  ignore_result(IsChromeOS);
  ignore_result(IsArchitectureArm);
  ignore_result(RunSandboxSanityChecks);
  return false;
}
#endif  // !defined(IN_NACL_HELPER)

}  // namespace

#endif  // USE_SECCOMP_BPF

// Is seccomp BPF globally enabled?
bool SandboxSeccompBPF::IsSeccompBPFDesired() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kNoSandbox) &&
      !command_line.HasSwitch(switches::kDisableSeccompFilterSandbox)) {
    return true;
  } else {
    return false;
  }
}

bool SandboxSeccompBPF::ShouldEnableSeccompBPF(
    const std::string& process_type) {
#if defined(USE_SECCOMP_BPF)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (process_type == switches::kGpuProcess)
    return !command_line.HasSwitch(switches::kDisableGpuSandbox);

  return true;
#endif  // USE_SECCOMP_BPF
  return false;
}

bool SandboxSeccompBPF::SupportsSandbox() {
#if defined(USE_SECCOMP_BPF)
  // TODO(jln): pass the saved proc_fd_ from the LinuxSandbox singleton
  // here.
  SandboxBPF::SandboxStatus bpf_sandbox_status =
      SandboxBPF::SupportsSeccompSandbox(-1);
  // Kernel support is what we are interested in here. Other status
  // such as STATUS_UNAVAILABLE (has threads) still indicate kernel support.
  // We make this a negative check, since if there is a bug, we would rather
  // "fail closed" (expect a sandbox to be available and try to start it).
  if (bpf_sandbox_status != SandboxBPF::STATUS_UNSUPPORTED) {
    return true;
  }
#endif
  return false;
}

bool SandboxSeccompBPF::StartSandbox(const std::string& process_type) {
#if defined(USE_SECCOMP_BPF)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (IsSeccompBPFDesired() &&  // Global switches policy.
      ShouldEnableSeccompBPF(process_type) &&  // Process-specific policy.
      SupportsSandbox()) {
    // If the kernel supports the sandbox, and if the command line says we
    // should enable it, enable it or die.
    bool started_sandbox = StartBPFSandbox(command_line, process_type);
    CHECK(started_sandbox);
    return true;
  }
#endif
  return false;
}

bool SandboxSeccompBPF::StartSandboxWithExternalPolicy(
    scoped_ptr<sandbox::SandboxBPFPolicy> policy) {
#if defined(USE_SECCOMP_BPF)
  if (IsSeccompBPFDesired() && SupportsSandbox()) {
    CHECK(policy);
    StartSandboxWithPolicy(policy.release());
    return true;
  }
#endif  // defined(USE_SECCOMP_BPF)
  return false;
}

scoped_ptr<sandbox::SandboxBPFPolicy>
SandboxSeccompBPF::GetBaselinePolicy() {
#if defined(USE_SECCOMP_BPF)
  return scoped_ptr<sandbox::SandboxBPFPolicy>(new BaselinePolicy);
#else
  return scoped_ptr<sandbox::SandboxBPFPolicy>();
#endif  // defined(USE_SECCOMP_BPF)
}

}  // namespace content
