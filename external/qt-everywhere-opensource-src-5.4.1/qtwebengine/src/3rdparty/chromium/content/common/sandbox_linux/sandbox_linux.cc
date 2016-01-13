// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <limits>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_linux.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/services/yama.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
     defined(LEAK_SANITIZER)
#include <sanitizer/common_interface_defs.h>
#endif

using sandbox::Yama;

namespace {

struct FDCloser {
  inline void operator()(int* fd) const {
    DCHECK(fd);
    PCHECK(0 == IGNORE_EINTR(close(*fd)));
    *fd = -1;
  }
};

void LogSandboxStarted(const std::string& sandbox_name) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  const std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  const std::string activated_sandbox =
      "Activated " + sandbox_name + " sandbox for process type: " +
      process_type + ".";
#if defined(OS_CHROMEOS)
  LOG(WARNING) << activated_sandbox;
#else
  VLOG(1) << activated_sandbox;
#endif
}

#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER)
bool AddResourceLimit(int resource, rlim_t limit) {
  struct rlimit old_rlimit;
  if (getrlimit(resource, &old_rlimit))
    return false;
  // Make sure we don't raise the existing limit.
  const struct rlimit new_rlimit = {
      std::min(old_rlimit.rlim_cur, limit),
      std::min(old_rlimit.rlim_max, limit)
      };
  int rc = setrlimit(resource, &new_rlimit);
  return rc == 0;
}
#endif

bool IsRunningTSAN() {
#if defined(THREAD_SANITIZER)
  return true;
#else
  return false;
#endif
}

// Try to open /proc/self/task/ with the help of |proc_fd|. |proc_fd| can be
// -1. Will return -1 on error and set errno like open(2).
int OpenProcTaskFd(int proc_fd) {
  int proc_self_task = -1;
  if (proc_fd >= 0) {
    // If a handle to /proc is available, use it. This allows to bypass file
    // system restrictions.
    proc_self_task = openat(proc_fd, "self/task/", O_RDONLY | O_DIRECTORY);
  } else {
    // Otherwise, make an attempt to access the file system directly.
    proc_self_task = open("/proc/self/task/", O_RDONLY | O_DIRECTORY);
  }
  return proc_self_task;
}

}  // namespace

namespace content {

LinuxSandbox::LinuxSandbox()
    : proc_fd_(-1),
      seccomp_bpf_started_(false),
      sandbox_status_flags_(kSandboxLinuxInvalid),
      pre_initialized_(false),
      seccomp_bpf_supported_(false),
      yama_is_enforcing_(false),
      setuid_sandbox_client_(sandbox::SetuidSandboxClient::Create())
{
  if (setuid_sandbox_client_ == NULL) {
    LOG(FATAL) << "Failed to instantiate the setuid sandbox client.";
  }
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(LEAK_SANITIZER)
  sanitizer_args_ = make_scoped_ptr(new __sanitizer_sandbox_arguments);
  *sanitizer_args_ = {0};
#endif
}

LinuxSandbox::~LinuxSandbox() {
}

LinuxSandbox* LinuxSandbox::GetInstance() {
  LinuxSandbox* instance = Singleton<LinuxSandbox>::get();
  CHECK(instance);
  return instance;
}

void LinuxSandbox::PreinitializeSandbox() {
  CHECK(!pre_initialized_);
  seccomp_bpf_supported_ = false;
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
     defined(LEAK_SANITIZER)
  // Sanitizers need to open some resources before the sandbox is enabled.
  // This should not fork, not launch threads, not open a directory.
  __sanitizer_sandbox_on_notify(sanitizer_args());
  sanitizer_args_.reset();
#endif

#if !defined(NDEBUG)
  // The in-process stack dumping needs to open /proc/self/maps and cache
  // its contents before the sandbox is enabled.  It also pre-opens the
  // object files that are already loaded in the process address space.
  base::debug::EnableInProcessStackDumpingForSandbox();

  // Open proc_fd_ only in Debug mode so that forgetting to close it doesn't
  // produce a sandbox escape in Release mode.
  proc_fd_ = open("/proc", O_DIRECTORY | O_RDONLY | O_CLOEXEC);
  CHECK_GE(proc_fd_, 0);
#endif  // !defined(NDEBUG)
  // We "pre-warm" the code that detects supports for seccomp BPF.
  if (SandboxSeccompBPF::IsSeccompBPFDesired()) {
    if (!SandboxSeccompBPF::SupportsSandbox()) {
      VLOG(1) << "Lacking support for seccomp-bpf sandbox.";
    } else {
      seccomp_bpf_supported_ = true;
    }
  }

  // Yama is a "global", system-level status. We assume it will not regress
  // after startup.
  const int yama_status = Yama::GetStatus();
  yama_is_enforcing_ = (yama_status & Yama::STATUS_PRESENT) &&
                       (yama_status & Yama::STATUS_ENFORCING);
  pre_initialized_ = true;
}

bool LinuxSandbox::InitializeSandbox() {
  LinuxSandbox* linux_sandbox = LinuxSandbox::GetInstance();
  return linux_sandbox->InitializeSandboxImpl();
}

void LinuxSandbox::StopThread(base::Thread* thread) {
  LinuxSandbox* linux_sandbox = LinuxSandbox::GetInstance();
  linux_sandbox->StopThreadImpl(thread);
}

int LinuxSandbox::GetStatus() {
  CHECK(pre_initialized_);
  if (kSandboxLinuxInvalid == sandbox_status_flags_) {
    // Initialize sandbox_status_flags_.
    sandbox_status_flags_ = 0;
    if (setuid_sandbox_client_->IsSandboxed()) {
      sandbox_status_flags_ |= kSandboxLinuxSUID;
      if (setuid_sandbox_client_->IsInNewPIDNamespace())
        sandbox_status_flags_ |= kSandboxLinuxPIDNS;
      if (setuid_sandbox_client_->IsInNewNETNamespace())
        sandbox_status_flags_ |= kSandboxLinuxNetNS;
    }

    // We report whether the sandbox will be activated when renderers, workers
    // and PPAPI plugins go through sandbox initialization.
    if (seccomp_bpf_supported() &&
        SandboxSeccompBPF::ShouldEnableSeccompBPF(switches::kRendererProcess)) {
      sandbox_status_flags_ |= kSandboxLinuxSeccompBPF;
    }

    if (yama_is_enforcing_) {
      sandbox_status_flags_ |= kSandboxLinuxYama;
    }
  }

  return sandbox_status_flags_;
}

// Threads are counted via /proc/self/task. This is a little hairy because of
// PID namespaces and existing sandboxes, so "self" must really be used instead
// of using the pid.
bool LinuxSandbox::IsSingleThreaded() const {
  bool is_single_threaded = false;
  base::ScopedFD proc_self_task(OpenProcTaskFd(proc_fd_));

// In Debug mode, it's mandatory to be able to count threads to catch bugs.
#if !defined(NDEBUG)
  // Using CHECK here since we want to check all the cases where
  // !defined(NDEBUG)
  // gets built.
  CHECK(proc_self_task.is_valid())
      << "Could not count threads, the sandbox was not "
      << "pre-initialized properly.";
#endif  // !defined(NDEBUG)

  if (!proc_self_task.is_valid()) {
    // Pretend to be monothreaded if it can't be determined (for instance the
    // setuid sandbox is already engaged but no proc_fd_ is available).
    is_single_threaded = true;
  } else {
    is_single_threaded =
        sandbox::ThreadHelpers::IsSingleThreaded(proc_self_task.get());
  }

  return is_single_threaded;
}

bool LinuxSandbox::seccomp_bpf_started() const {
  return seccomp_bpf_started_;
}

sandbox::SetuidSandboxClient*
    LinuxSandbox::setuid_sandbox_client() const {
  return setuid_sandbox_client_.get();
}

// For seccomp-bpf, we use the SandboxSeccompBPF class.
bool LinuxSandbox::StartSeccompBPF(const std::string& process_type) {
  CHECK(!seccomp_bpf_started_);
  CHECK(pre_initialized_);
  if (seccomp_bpf_supported())
    seccomp_bpf_started_ = SandboxSeccompBPF::StartSandbox(process_type);

  if (seccomp_bpf_started_)
    LogSandboxStarted("seccomp-bpf");

  return seccomp_bpf_started_;
}

bool LinuxSandbox::InitializeSandboxImpl() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  // We need to make absolutely sure that our sandbox is "sealed" before
  // returning.
  // Unretained() since the current object is a Singleton.
  base::ScopedClosureRunner sandbox_sealer(
      base::Bind(&LinuxSandbox::SealSandbox, base::Unretained(this)));
  // Make sure that this function enables sandboxes as promised by GetStatus().
  // Unretained() since the current object is a Singleton.
  base::ScopedClosureRunner sandbox_promise_keeper(
      base::Bind(&LinuxSandbox::CheckForBrokenPromises,
                 base::Unretained(this),
                 process_type));

  // No matter what, it's always an error to call InitializeSandbox() after
  // threads have been created.
  if (!IsSingleThreaded()) {
    std::string error_message = "InitializeSandbox() called with multiple "
                                "threads in process " + process_type;
    // TSAN starts a helper thread, so we don't start the sandbox and don't
    // even report an error about it.
    if (IsRunningTSAN())
      return false;

    // The GPU process is allowed to call InitializeSandbox() with threads.
    bool sandbox_failure_fatal = process_type != switches::kGpuProcess;
    // This can be disabled with the '--gpu-sandbox-failures-fatal' flag.
    // Setting the flag with no value or any value different than 'yes' or 'no'
    // is equal to setting '--gpu-sandbox-failures-fatal=yes'.
    if (process_type == switches::kGpuProcess &&
        command_line->HasSwitch(switches::kGpuSandboxFailuresFatal)) {
      const std::string switch_value =
          command_line->GetSwitchValueASCII(switches::kGpuSandboxFailuresFatal);
      sandbox_failure_fatal = switch_value != "no";
    }

    if (sandbox_failure_fatal)
      LOG(FATAL) << error_message;

    LOG(ERROR) << error_message;
    return false;
  }

  // Only one thread is running, pre-initialize if not already done.
  if (!pre_initialized_)
    PreinitializeSandbox();

  DCHECK(!HasOpenDirectories()) <<
      "InitializeSandbox() called after unexpected directories have been " <<
      "opened. This breaks the security of the setuid sandbox.";

  // Attempt to limit the future size of the address space of the process.
  LimitAddressSpace(process_type);

  // Try to enable seccomp-bpf.
  bool seccomp_bpf_started = StartSeccompBPF(process_type);

  return seccomp_bpf_started;
}

void LinuxSandbox::StopThreadImpl(base::Thread* thread) {
  DCHECK(thread);
  StopThreadAndEnsureNotCounted(thread);
}

bool LinuxSandbox::seccomp_bpf_supported() const {
  CHECK(pre_initialized_);
  return seccomp_bpf_supported_;
}

bool LinuxSandbox::LimitAddressSpace(const std::string& process_type) {
  (void) process_type;
#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoSandbox)) {
    return false;
  }

  // Limit the address space to 4GB.
  // This is in the hope of making some kernel exploits more complex and less
  // reliable. It also limits sprays a little on 64-bit.
  rlim_t address_space_limit = std::numeric_limits<uint32_t>::max();
#if defined(__LP64__)
  // On 64 bits, V8 and possibly others will reserve massive memory ranges and
  // rely on on-demand paging for allocation.  Unfortunately, even
  // MADV_DONTNEED ranges  count towards RLIMIT_AS so this is not an option.
  // See crbug.com/169327 for a discussion.
  // On the GPU process, irrespective of V8, we can exhaust a 4GB address space
  // under normal usage, see crbug.com/271119
  // For now, increase limit to 16GB for renderer and worker and gpu processes
  // to accomodate.
  if (process_type == switches::kRendererProcess ||
      process_type == switches::kWorkerProcess ||
      process_type == switches::kGpuProcess) {
    address_space_limit = 1L << 34;
  }
#endif  // defined(__LP64__)

  // On all platforms, add a limit to the brk() heap that would prevent
  // allocations that can't be index by an int.
  const rlim_t kNewDataSegmentMaxSize = std::numeric_limits<int>::max();

  bool limited_as = AddResourceLimit(RLIMIT_AS, address_space_limit);
  bool limited_data = AddResourceLimit(RLIMIT_DATA, kNewDataSegmentMaxSize);

  // Cache the resource limit before turning on the sandbox.
  base::SysInfo::AmountOfVirtualMemory();

  return limited_as && limited_data;
#else
  base::SysInfo::AmountOfVirtualMemory();
  return false;
#endif  // !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER)
}

bool LinuxSandbox::HasOpenDirectories() const {
  return sandbox::Credentials().HasOpenDirectory(proc_fd_);
}

void LinuxSandbox::SealSandbox() {
  if (proc_fd_ >= 0) {
    int ret = IGNORE_EINTR(close(proc_fd_));
    CHECK_EQ(0, ret);
    proc_fd_ = -1;
  }
}

void LinuxSandbox::CheckForBrokenPromises(const std::string& process_type) {
  // Make sure that any promise made with GetStatus() wasn't broken.
  bool promised_seccomp_bpf_would_start = false;
  if (process_type == switches::kRendererProcess ||
      process_type == switches::kWorkerProcess ||
      process_type == switches::kPpapiPluginProcess) {
    promised_seccomp_bpf_would_start =
        (sandbox_status_flags_ != kSandboxLinuxInvalid) &&
        (GetStatus() & kSandboxLinuxSeccompBPF);
  }
  if (promised_seccomp_bpf_would_start) {
    CHECK(seccomp_bpf_started_);
  }
}

void LinuxSandbox::StopThreadAndEnsureNotCounted(base::Thread* thread) const {
  DCHECK(thread);
  base::ScopedFD proc_self_task(OpenProcTaskFd(proc_fd_));
  PCHECK(proc_self_task.is_valid());
  CHECK(sandbox::ThreadHelpers::StopThreadAndWatchProcFS(proc_self_task.get(),
                                                         thread));
}

}  // namespace content
