// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/bpf_gpu_policy_linux.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"
#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"
#include "content/common/set_process_title.h"
#include "content/public/common/content_switches.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/broker_process.h"
#include "sandbox/linux/services/linux_syscalls.h"

using sandbox::BrokerProcess;
using sandbox::ErrorCode;
using sandbox::SandboxBPF;
using sandbox::SyscallSets;
using sandbox::arch_seccomp_data;

namespace content {

namespace {

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureX86_64() {
#if defined(__x86_64__)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureI386() {
#if defined(__i386__)
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

bool IsAcceleratedVideoDecodeEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  return !command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode);
}

intptr_t GpuSIGSYS_Handler(const struct arch_seccomp_data& args,
                           void* aux_broker_process) {
  RAW_CHECK(aux_broker_process);
  BrokerProcess* broker_process =
      static_cast<BrokerProcess*>(aux_broker_process);
  switch (args.nr) {
    case __NR_access:
      return broker_process->Access(reinterpret_cast<const char*>(args.args[0]),
                                    static_cast<int>(args.args[1]));
    case __NR_open:
#if defined(MEMORY_SANITIZER)
      // http://crbug.com/372840
      __msan_unpoison_string(reinterpret_cast<const char*>(args.args[0]));
#endif
      return broker_process->Open(reinterpret_cast<const char*>(args.args[0]),
                                  static_cast<int>(args.args[1]));
    case __NR_openat:
      // Allow using openat() as open().
      if (static_cast<int>(args.args[0]) == AT_FDCWD) {
        return
            broker_process->Open(reinterpret_cast<const char*>(args.args[1]),
                                 static_cast<int>(args.args[2]));
      } else {
        return -EPERM;
      }
    default:
      RAW_CHECK(false);
      return -ENOSYS;
  }
}

class GpuBrokerProcessPolicy : public GpuProcessPolicy {
 public:
  static sandbox::SandboxBPFPolicy* Create() {
    return new GpuBrokerProcessPolicy();
  }
  virtual ~GpuBrokerProcessPolicy() {}

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  GpuBrokerProcessPolicy() {}
  DISALLOW_COPY_AND_ASSIGN(GpuBrokerProcessPolicy);
};

// x86_64/i386 or desktop ARM.
// A GPU broker policy is the same as a GPU policy with open and
// openat allowed.
ErrorCode GpuBrokerProcessPolicy::EvaluateSyscall(SandboxBPF* sandbox,
                                                  int sysno) const {
  switch (sysno) {
    case __NR_access:
    case __NR_open:
    case __NR_openat:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    default:
      return GpuProcessPolicy::EvaluateSyscall(sandbox, sysno);
  }
}

void UpdateProcessTypeToGpuBroker() {
  CommandLine::StringVector exec = CommandLine::ForCurrentProcess()->GetArgs();
  CommandLine::Reset();
  CommandLine::Init(0, NULL);
  CommandLine::ForCurrentProcess()->InitFromArgv(exec);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kProcessType,
                                                      "gpu-broker");

  // Update the process title. The argv was already cached by the call to
  // SetProcessTitleFromCommandLine in content_main_runner.cc, so we can pass
  // NULL here (we don't have the original argv at this point).
  SetProcessTitleFromCommandLine(NULL);
}

bool UpdateProcessTypeAndEnableSandbox(
    sandbox::SandboxBPFPolicy* (*broker_sandboxer_allocator)(void)) {
  DCHECK(broker_sandboxer_allocator);
  UpdateProcessTypeToGpuBroker();
  return SandboxSeccompBPF::StartSandboxWithExternalPolicy(
      make_scoped_ptr(broker_sandboxer_allocator()));
}

}  // namespace

GpuProcessPolicy::GpuProcessPolicy() : broker_process_(NULL) {}

GpuProcessPolicy::~GpuProcessPolicy() {}

// Main policy for x86_64/i386. Extended by CrosArmGpuProcessPolicy.
ErrorCode GpuProcessPolicy::EvaluateSyscall(SandboxBPF* sandbox,
                                            int sysno) const {
  switch (sysno) {
    case __NR_ioctl:
#if defined(__i386__) || defined(__x86_64__)
    // The Nvidia driver uses flags not in the baseline policy
    // (MAP_LOCKED | MAP_EXECUTABLE | MAP_32BIT)
    case __NR_mmap:
#endif
    // We also hit this on the linux_chromeos bot but don't yet know what
    // weird flags were involved.
    case __NR_mprotect:
    // TODO(jln): restrict prctl.
    case __NR_prctl:
    case __NR_sched_getaffinity:
    case __NR_sched_setaffinity:
    case __NR_setpriority:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    case __NR_access:
    case __NR_open:
    case __NR_openat:
      DCHECK(broker_process_);
      return sandbox->Trap(GpuSIGSYS_Handler, broker_process_);
    default:
      if (SyscallSets::IsEventFd(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);

      // Default on the baseline policy.
      return SandboxBPFBasePolicy::EvaluateSyscall(sandbox, sysno);
  }
}

bool GpuProcessPolicy::PreSandboxHook() {
  // Warm up resources needed by the policy we're about to enable and
  // eventually start a broker process.
  const bool chromeos_arm_gpu = IsChromeOS() && IsArchitectureArm();
  // This policy is for x86 or Desktop.
  DCHECK(!chromeos_arm_gpu);

  DCHECK(!broker_process());
  // Create a new broker process.
  InitGpuBrokerProcess(
      GpuBrokerProcessPolicy::Create,
      std::vector<std::string>(),  // No extra files in whitelist.
      std::vector<std::string>());

  if (IsArchitectureX86_64() || IsArchitectureI386()) {
    // Accelerated video decode dlopen()'s some shared objects
    // inside the sandbox, so preload them now.
    if (IsAcceleratedVideoDecodeEnabled()) {
      const char* I965DrvVideoPath = NULL;

      if (IsArchitectureX86_64()) {
        I965DrvVideoPath = "/usr/lib64/va/drivers/i965_drv_video.so";
      } else if (IsArchitectureI386()) {
        I965DrvVideoPath = "/usr/lib/va/drivers/i965_drv_video.so";
      }

      dlopen(I965DrvVideoPath, RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
      dlopen("libva.so.1", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
      dlopen("libva-x11.so.1", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
    }
  }

  return true;
}

void GpuProcessPolicy::InitGpuBrokerProcess(
    sandbox::SandboxBPFPolicy* (*broker_sandboxer_allocator)(void),
    const std::vector<std::string>& read_whitelist_extra,
    const std::vector<std::string>& write_whitelist_extra) {
  static const char kDriRcPath[] = "/etc/drirc";
  static const char kDriCard0Path[] = "/dev/dri/card0";

  CHECK(broker_process_ == NULL);

  // All GPU process policies need these files brokered out.
  std::vector<std::string> read_whitelist;
  read_whitelist.push_back(kDriCard0Path);
  read_whitelist.push_back(kDriRcPath);
  // Add eventual extra files from read_whitelist_extra.
  read_whitelist.insert(read_whitelist.end(),
                        read_whitelist_extra.begin(),
                        read_whitelist_extra.end());

  std::vector<std::string> write_whitelist;
  write_whitelist.push_back(kDriCard0Path);
  // Add eventual extra files from write_whitelist_extra.
  write_whitelist.insert(write_whitelist.end(),
                         write_whitelist_extra.begin(),
                         write_whitelist_extra.end());

  broker_process_ = new BrokerProcess(GetFSDeniedErrno(),
                                      read_whitelist,
                                      write_whitelist);
  // The initialization callback will perform generic initialization and then
  // call broker_sandboxer_callback.
  CHECK(broker_process_->Init(base::Bind(&UpdateProcessTypeAndEnableSandbox,
                                         broker_sandboxer_allocator)));
}

}  // namespace content
