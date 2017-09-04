// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"

#if defined(USE_SECCOMP_BPF)

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "content/common/sandbox_linux/bpf_cros_arm_gpu_policy_linux.h"
#include "content/common/sandbox_linux/bpf_gpu_policy_linux.h"
#include "content/common/sandbox_linux/bpf_ppapi_policy_linux.h"
#include "content/common/sandbox_linux/bpf_renderer_policy_linux.h"
#include "content/common/sandbox_linux/bpf_utility_policy_linux.h"
#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

#if !defined(IN_NACL_HELPER)
#include "ui/gl/gl_switches.h"
#endif  // !defined(IN_NACL_HELPER)

using sandbox::BaselinePolicy;
using sandbox::SandboxBPF;
using sandbox::SyscallSets;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

#else

// Make sure that seccomp-bpf does not get disabled by mistake. Also make sure
// that we think twice about this when adding a new architecture.
#if !defined(ARCH_CPU_ARM64)
#error "Seccomp-bpf disabled on supported architecture!"
#endif  // !defined(ARCH_CPU_ARM64)

#endif  //

namespace content {

#if defined(USE_SECCOMP_BPF)
namespace {

// This function takes ownership of |policy|.
void StartSandboxWithPolicy(sandbox::bpf_dsl::Policy* policy,
                            base::ScopedFD proc_fd) {
  // Starting the sandbox is a one-way operation. The kernel doesn't allow
  // us to unload a sandbox policy after it has been started. Nonetheless,
  // in order to make the use of the "Sandbox" object easier, we allow for
  // the object to be destroyed after the sandbox has been started. Note that
  // doing so does not stop the sandbox.
  SandboxBPF sandbox(policy);

  sandbox.SetProcFd(std::move(proc_fd));
  CHECK(sandbox.StartSandbox(SandboxBPF::SeccompLevel::SINGLE_THREADED));
}

#if !defined(OS_NACL_NONSFI)

class BlacklistDebugAndNumaPolicy : public SandboxBPFBasePolicy {
 public:
  BlacklistDebugAndNumaPolicy() {}
  ~BlacklistDebugAndNumaPolicy() override {}

  ResultExpr EvaluateSyscall(int system_call_number) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlacklistDebugAndNumaPolicy);
};

ResultExpr BlacklistDebugAndNumaPolicy::EvaluateSyscall(int sysno) const {
  if (SyscallSets::IsDebug(sysno) || SyscallSets::IsNuma(sysno))
    return sandbox::CrashSIGSYS();

  return Allow();
}

class AllowAllPolicy : public SandboxBPFBasePolicy {
 public:
  AllowAllPolicy() {}
  ~AllowAllPolicy() override {}

  ResultExpr EvaluateSyscall(int system_call_number) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AllowAllPolicy);
};

// Allow all syscalls.
// This will still deny x32 or IA32 calls in 64 bits mode or
// 64 bits system calls in compatibility mode.
ResultExpr AllowAllPolicy::EvaluateSyscall(int sysno) const {
  return Allow();
}

// nacl_helper needs to be tiny and includes only part of content/
// in its dependencies. Make sure to not link things that are not needed.
#if !defined(IN_NACL_HELPER)
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

// If a BPF policy is engaged for |process_type|, run a few sanity checks.
void RunSandboxSanityChecks(const std::string& process_type) {
  if (process_type == switches::kRendererProcess ||
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

std::unique_ptr<SandboxBPFBasePolicy> GetGpuProcessSandbox() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (IsChromeOS() && IsArchitectureArm()) {
    bool allow_sysv_shm =
        command_line.HasSwitch(switches::kGpuSandboxAllowSysVShm);
    return std::unique_ptr<SandboxBPFBasePolicy>(
        new CrosArmGpuProcessPolicy(allow_sysv_shm));
  } else {
    bool allow_mincore = command_line.HasSwitch(switches::kUseGL) &&
                         command_line.GetSwitchValueASCII(switches::kUseGL) ==
                             gl::kGLImplementationEGLName;
    return std::unique_ptr<SandboxBPFBasePolicy>(
        new GpuProcessPolicy(allow_mincore));
  }
}

// Initialize the seccomp-bpf sandbox.
bool StartBPFSandbox(const base::CommandLine& command_line,
                     const std::string& process_type,
                     base::ScopedFD proc_fd) {
  std::unique_ptr<SandboxBPFBasePolicy> policy;

  if (process_type == switches::kGpuProcess) {
    policy.reset(GetGpuProcessSandbox().release());
  } else if (process_type == switches::kRendererProcess) {
    policy.reset(new RendererProcessPolicy);
  } else if (process_type == switches::kPpapiPluginProcess) {
    policy.reset(new PpapiProcessPolicy);
  } else if (process_type == switches::kUtilityProcess) {
    policy.reset(new UtilityProcessPolicy);
  } else {
    NOTREACHED();
    policy.reset(new AllowAllPolicy);
  }

  CHECK(policy->PreSandboxHook());
  StartSandboxWithPolicy(policy.release(), std::move(proc_fd));

  RunSandboxSanityChecks(process_type);
  return true;
}
#else  // defined(IN_NACL_HELPER)
bool StartBPFSandbox(const base::CommandLine& command_line,
                     const std::string& process_type) {
  NOTREACHED();
  return false;
}
#endif  // !defined(IN_NACL_HELPER)
#endif  // !defined(OS_NACL_NONSFI)

}  // namespace

#endif  // USE_SECCOMP_BPF

// Is seccomp BPF globally enabled?
bool SandboxSeccompBPF::IsSeccompBPFDesired() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kNoSandbox) &&
      !command_line.HasSwitch(switches::kDisableSeccompFilterSandbox)) {
    return true;
  } else {
    return false;
  }
}

#if !defined(OS_NACL_NONSFI)
bool SandboxSeccompBPF::ShouldEnableSeccompBPF(
    const std::string& process_type) {
#if defined(USE_SECCOMP_BPF)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (process_type == switches::kGpuProcess)
    return !command_line.HasSwitch(switches::kDisableGpuSandbox);

  return true;
#endif  // USE_SECCOMP_BPF
  return false;
}
#endif  // !defined(OS_NACL_NONSFI)

bool SandboxSeccompBPF::SupportsSandbox() {
#if defined(USE_SECCOMP_BPF)
  return SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::SINGLE_THREADED);
#endif
  return false;
}

#if !defined(OS_NACL_NONSFI)
bool SandboxSeccompBPF::SupportsSandboxWithTsync() {
#if defined(USE_SECCOMP_BPF)
  return SandboxBPF::SupportsSeccompSandbox(
      SandboxBPF::SeccompLevel::MULTI_THREADED);
#endif
  return false;
}

bool SandboxSeccompBPF::StartSandbox(const std::string& process_type,
                                     base::ScopedFD proc_fd) {
#if defined(USE_SECCOMP_BPF)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (IsSeccompBPFDesired() &&  // Global switches policy.
      ShouldEnableSeccompBPF(process_type) &&  // Process-specific policy.
      SupportsSandbox()) {
    // If the kernel supports the sandbox, and if the command line says we
    // should enable it, enable it or die.
    bool started_sandbox =
        StartBPFSandbox(command_line, process_type, std::move(proc_fd));
    CHECK(started_sandbox);
    return true;
  }
#endif
  return false;
}
#endif  // !defined(OS_NACL_NONSFI)

bool SandboxSeccompBPF::StartSandboxWithExternalPolicy(
    std::unique_ptr<sandbox::bpf_dsl::Policy> policy,
    base::ScopedFD proc_fd) {
#if defined(USE_SECCOMP_BPF)
  if (IsSeccompBPFDesired() && SupportsSandbox()) {
    CHECK(policy);
    StartSandboxWithPolicy(policy.release(), std::move(proc_fd));
    return true;
  }
#endif  // defined(USE_SECCOMP_BPF)
  return false;
}

#if !defined(OS_NACL_NONSFI)
std::unique_ptr<sandbox::bpf_dsl::Policy>
SandboxSeccompBPF::GetBaselinePolicy() {
#if defined(USE_SECCOMP_BPF)
  return std::unique_ptr<sandbox::bpf_dsl::Policy>(new BaselinePolicy);
#else
  return std::unique_ptr<sandbox::bpf_dsl::Policy>();
#endif  // defined(USE_SECCOMP_BPF)
}
#endif  // !defined(OS_NACL_NONSFI)

}  // namespace content
