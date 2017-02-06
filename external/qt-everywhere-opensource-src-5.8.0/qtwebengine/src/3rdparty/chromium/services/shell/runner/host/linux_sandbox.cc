// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/runner/host/linux_sandbox.h"

#include <fcntl.h>
#include <sys/syscall.h>
#include <utility>

#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/services/thread_helpers.h"

using sandbox::syscall_broker::BrokerFilePermission;

namespace shell {

namespace {

intptr_t SandboxSIGSYSHandler(const struct sandbox::arch_seccomp_data& args,
                              void* aux) {
  RAW_CHECK(aux);
  const sandbox::syscall_broker::BrokerProcess* broker_process =
      static_cast<const sandbox::syscall_broker::BrokerProcess*>(aux);
  switch (args.nr) {
#if !defined(__aarch64__)
    case __NR_access:
      return broker_process->Access(reinterpret_cast<const char*>(args.args[0]),
                                    static_cast<int>(args.args[1]));
    case __NR_open:
      return broker_process->Open(reinterpret_cast<const char*>(args.args[0]),
                                  static_cast<int>(args.args[1]));
#endif
    case __NR_faccessat:
      if (static_cast<int>(args.args[0]) == AT_FDCWD) {
        return broker_process->Access(
            reinterpret_cast<const char*>(args.args[1]),
            static_cast<int>(args.args[2]));
      } else {
        return -EPERM;
      }
    case __NR_openat:
      // Allow using openat() as open().
      if (static_cast<int>(args.args[0]) == AT_FDCWD) {
        return broker_process->Open(reinterpret_cast<const char*>(args.args[1]),
                                    static_cast<int>(args.args[2]));
      } else {
        return -EPERM;
      }
    default:
      RAW_CHECK(false);
      return -ENOSYS;
  }
}

class SandboxPolicy : public sandbox::BaselinePolicy {
 public:
  explicit SandboxPolicy(sandbox::syscall_broker::BrokerProcess* broker_process)
      : broker_process_(broker_process) {}
  ~SandboxPolicy() override {}

  // Overridden from sandbox::bpf_dsl::Policy:
  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(int sysno) const override {
    // This policy is only advisory/for noticing FS access for the moment.
    switch (sysno) {
#if !defined(__aarch64__)
      case __NR_access:
      case __NR_open:
#endif
      case __NR_faccessat:
      case __NR_openat:
        return sandbox::bpf_dsl::Trap(SandboxSIGSYSHandler, broker_process_);
      case __NR_sched_getaffinity:
        return sandbox::RestrictSchedTarget(policy_pid(), sysno);
      case __NR_ftruncate:
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__) || \
    defined(__aarch64__)
      // Per #ifdefs in
      // content/common/sandbox_linux/bpf_renderer_policy_linux.cc
      case __NR_getrlimit:
#endif
#if defined(__i386__) || defined(__arm__)
      case __NR_ugetrlimit:
#endif
      case __NR_uname:
#if defined(__arm__) || defined(__x86_64__) || defined(__mips__)
      case __NR_getsockopt:
      case __NR_setsockopt:
#endif
        return sandbox::bpf_dsl::Allow();
    }

    return BaselinePolicy::EvaluateSyscall(sysno);
  }

 private:
  // Not owned.
  const sandbox::syscall_broker::BrokerProcess* broker_process_;
  DISALLOW_COPY_AND_ASSIGN(SandboxPolicy);
};

}  // namespace

LinuxSandbox::LinuxSandbox(const std::vector<BrokerFilePermission>& permissions)
    : broker_(new sandbox::syscall_broker::BrokerProcess(EPERM, permissions)) {
  CHECK(broker_->Init(
      base::Bind<bool (*)()>(&sandbox::Credentials::DropAllCapabilities)));
  policy_.reset(new SandboxPolicy(broker_.get()));
}

LinuxSandbox::~LinuxSandbox() {}

void LinuxSandbox::Warmup() {
  proc_fd_ = sandbox::ProcUtil::OpenProc();
  warmed_up_ = true;

  // Verify that we haven't started threads or grabbed directory file
  // descriptors.
  sandbox::ThreadHelpers::AssertSingleThreaded(proc_fd_.get());
  CHECK(!sandbox::ProcUtil::HasOpenDirectory(proc_fd_.get()));
}

void LinuxSandbox::EngageNamespaceSandbox() {
  CHECK(warmed_up_);
  CHECK_EQ(1, getpid());
  CHECK(sandbox::NamespaceSandbox::InNewPidNamespace());
  CHECK(sandbox::Credentials::MoveToNewUserNS());
  CHECK(sandbox::Credentials::DropFileSystemAccess(proc_fd_.get()));
  CHECK(sandbox::Credentials::DropAllCapabilities(proc_fd_.get()));
}

void LinuxSandbox::EngageSeccompSandbox() {
  CHECK(warmed_up_);
  sandbox::SandboxBPF sandbox(policy_.release());
  base::ScopedFD proc_fd(HANDLE_EINTR(
      openat(proc_fd_.get(), ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC)));
  CHECK(proc_fd.is_valid());
  sandbox.SetProcFd(std::move(proc_fd));
  CHECK(
      sandbox.StartSandbox(sandbox::SandboxBPF::SeccompLevel::SINGLE_THREADED))
      << "Starting the process with a sandbox failed. Missing kernel support.";

  // The Broker is now bound to this process and should only be destroyed when
  // the process exits or is killed.
  sandbox::syscall_broker::BrokerProcess* leaked_broker = broker_.release();
  ALLOW_UNUSED_LOCAL(leaked_broker);
  ANNOTATE_LEAKING_OBJECT_PTR(leaked_broker);
}

void LinuxSandbox::Seal() {
  proc_fd_.reset();
}

}  // namespace shell
