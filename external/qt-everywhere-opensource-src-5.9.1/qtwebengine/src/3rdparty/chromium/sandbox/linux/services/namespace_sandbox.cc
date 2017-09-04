// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/namespace_sandbox.h"

#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/namespace_utils.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/linux_signal.h"

namespace sandbox {

namespace {

const char kSandboxUSERNSEnvironmentVarName[] = "SBX_USER_NS";
const char kSandboxPIDNSEnvironmentVarName[] = "SBX_PID_NS";
const char kSandboxNETNSEnvironmentVarName[] = "SBX_NET_NS";

#if !defined(OS_NACL_NONSFI)
class WriteUidGidMapDelegate : public base::LaunchOptions::PreExecDelegate {
 public:
  WriteUidGidMapDelegate()
      : uid_(getuid()),
        gid_(getgid()),
        supports_deny_setgroups_(
            NamespaceUtils::KernelSupportsDenySetgroups()) {}

  ~WriteUidGidMapDelegate() override {}

  void RunAsyncSafe() override {
    if (supports_deny_setgroups_) {
      RAW_CHECK(NamespaceUtils::DenySetgroups());
    }
    RAW_CHECK(NamespaceUtils::WriteToIdMapFile("/proc/self/uid_map", uid_));
    RAW_CHECK(NamespaceUtils::WriteToIdMapFile("/proc/self/gid_map", gid_));
  }

 private:
  const uid_t uid_;
  const gid_t gid_;
  const bool supports_deny_setgroups_;
  DISALLOW_COPY_AND_ASSIGN(WriteUidGidMapDelegate);
};

void SetEnvironForNamespaceType(base::EnvironmentMap* environ,
                                base::NativeEnvironmentString env_var,
                                bool value) {
  // An empty string causes the env var to be unset in the child process.
  (*environ)[env_var] = value ? "1" : "";
}
#endif  // !defined(OS_NACL_NONSFI)

// Linux supports up to 64 signals. This should be updated if that ever changes.
int g_signal_exit_codes[64];

void TerminationSignalHandler(int sig) {
  // Return a special exit code so that the process is detected as terminated by
  // a signal.
  const size_t sig_idx = static_cast<size_t>(sig);
  if (sig_idx < arraysize(g_signal_exit_codes)) {
    _exit(g_signal_exit_codes[sig_idx]);
  }

  _exit(NamespaceSandbox::SignalExitCode(sig));
}

}  // namespace

#if !defined(OS_NACL_NONSFI)
NamespaceSandbox::Options::Options()
    : ns_types(CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNET),
      fail_on_unsupported_ns_type(false) {}

NamespaceSandbox::Options::~Options() {}

// static
base::Process NamespaceSandbox::LaunchProcess(
    const base::CommandLine& cmdline,
    const base::LaunchOptions& launch_options) {
  return LaunchProcessWithOptions(cmdline.argv(), launch_options, Options());
}

// static
base::Process NamespaceSandbox::LaunchProcess(
    const std::vector<std::string>& argv,
    const base::LaunchOptions& launch_options) {
  return LaunchProcessWithOptions(argv, launch_options, Options());
}

// static
base::Process NamespaceSandbox::LaunchProcessWithOptions(
    const base::CommandLine& cmdline,
    const base::LaunchOptions& launch_options,
    const Options& ns_sandbox_options) {
  return LaunchProcessWithOptions(cmdline.argv(), launch_options,
                                  ns_sandbox_options);
}

// static
base::Process NamespaceSandbox::LaunchProcessWithOptions(
    const std::vector<std::string>& argv,
    const base::LaunchOptions& launch_options,
    const Options& ns_sandbox_options) {
  // These fields may not be set by the caller.
  CHECK(launch_options.pre_exec_delegate == nullptr);
  CHECK_EQ(0, launch_options.clone_flags);

  int clone_flags = 0;
  const int kSupportedTypes[] = {CLONE_NEWUSER, CLONE_NEWPID, CLONE_NEWNET};
  for (const int ns_type : kSupportedTypes) {
    if ((ns_type & ns_sandbox_options.ns_types) == 0) {
      continue;
    }

    if (NamespaceUtils::KernelSupportsUnprivilegedNamespace(ns_type)) {
      clone_flags |= ns_type;
    } else if (ns_sandbox_options.fail_on_unsupported_ns_type) {
      return base::Process();
    }
  }
  CHECK(clone_flags & CLONE_NEWUSER);

  WriteUidGidMapDelegate write_uid_gid_map_delegate;

  base::LaunchOptions launch_options_copy = launch_options;
  launch_options_copy.pre_exec_delegate = &write_uid_gid_map_delegate;
  launch_options_copy.clone_flags = clone_flags;

  const std::pair<int, const char*> clone_flag_environ[] = {
      std::make_pair(CLONE_NEWUSER, kSandboxUSERNSEnvironmentVarName),
      std::make_pair(CLONE_NEWPID, kSandboxPIDNSEnvironmentVarName),
      std::make_pair(CLONE_NEWNET, kSandboxNETNSEnvironmentVarName),
  };

  base::EnvironmentMap* environ = &launch_options_copy.environ;
  for (const auto& entry : clone_flag_environ) {
    const int flag = entry.first;
    const char* environ_name = entry.second;
    SetEnvironForNamespaceType(environ, environ_name, clone_flags & flag);
  }

  return base::LaunchProcess(argv, launch_options_copy);
}
#endif  // !defined(OS_NACL_NONSFI)

// static
pid_t NamespaceSandbox::ForkInNewPidNamespace(bool drop_capabilities_in_child) {
  const pid_t pid =
      base::ForkWithFlags(CLONE_NEWPID | LINUX_SIGCHLD, nullptr, nullptr);
  if (pid < 0) {
    return pid;
  }

  if (pid == 0) {
    DCHECK_EQ(1, getpid());
    if (drop_capabilities_in_child) {
      // Since we just forked, we are single-threaded, so this should be safe.
      CHECK(Credentials::DropAllCapabilitiesOnCurrentThread());
    }
    return 0;
  }

  return pid;
}

// static
void NamespaceSandbox::InstallDefaultTerminationSignalHandlers() {
  static const int kDefaultTermSignals[] = {
      LINUX_SIGHUP,  LINUX_SIGINT,  LINUX_SIGABRT, LINUX_SIGQUIT,
      LINUX_SIGPIPE, LINUX_SIGTERM, LINUX_SIGUSR1, LINUX_SIGUSR2,
  };

  for (const int sig : kDefaultTermSignals) {
    InstallTerminationSignalHandler(sig, SignalExitCode(sig));
  }
}

// static
bool NamespaceSandbox::InstallTerminationSignalHandler(
    int sig,
    int exit_code) {
  struct sigaction old_action;
  PCHECK(sys_sigaction(sig, nullptr, &old_action) == 0);

#if !defined(OS_NACL_NONSFI)
  if (old_action.sa_flags & SA_SIGINFO &&
      old_action.sa_sigaction != nullptr) {
    return false;
  }
#endif

  if (old_action.sa_handler != LINUX_SIG_DFL) {
    return false;
  }

  const size_t sig_idx = static_cast<size_t>(sig);
  CHECK_LT(sig_idx, arraysize(g_signal_exit_codes));

  DCHECK_GE(exit_code, 0);
  DCHECK_LT(exit_code, 256);

  g_signal_exit_codes[sig_idx] = exit_code;

  struct sigaction action = {};
  action.sa_handler = &TerminationSignalHandler;
  PCHECK(sys_sigaction(sig, &action, nullptr) == 0);
  return true;
}

// static
bool NamespaceSandbox::InNewUserNamespace() {
  return getenv(kSandboxUSERNSEnvironmentVarName) != nullptr;
}

// static
bool NamespaceSandbox::InNewPidNamespace() {
  return getenv(kSandboxPIDNSEnvironmentVarName) != nullptr;
}

// static
bool NamespaceSandbox::InNewNetNamespace() {
  return getenv(kSandboxNETNSEnvironmentVarName) != nullptr;
}

}  // namespace sandbox
