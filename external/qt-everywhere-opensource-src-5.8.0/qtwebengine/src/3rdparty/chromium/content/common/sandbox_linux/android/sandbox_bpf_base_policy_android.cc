// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/android/sandbox_bpf_base_policy_android.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/net.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"

using sandbox::bpf_dsl::AllOf;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::AnyOf;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::BoolExpr;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::ResultExpr;

namespace content {

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC O_CLOEXEC
#endif

#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK O_NONBLOCK
#endif

namespace {

// Restricts the arguments to sys_socket() to AF_UNIX. Returns a BoolExpr that
// evaluates to true if the syscall should be allowed.
BoolExpr RestrictSocketArguments(const Arg<int>& domain,
                                 const Arg<int>& type,
                                 const Arg<int>& protocol) {
  const int kSockFlags = SOCK_CLOEXEC | SOCK_NONBLOCK;
  return AllOf(domain == AF_UNIX,
               AnyOf((type & ~kSockFlags) == SOCK_DGRAM,
                     (type & ~kSockFlags) == SOCK_STREAM),
               protocol == 0);
}

}  // namespace

SandboxBPFBasePolicyAndroid::SandboxBPFBasePolicyAndroid()
    : SandboxBPFBasePolicy() {}

SandboxBPFBasePolicyAndroid::~SandboxBPFBasePolicyAndroid() {}

ResultExpr SandboxBPFBasePolicyAndroid::EvaluateSyscall(int sysno) const {
  bool override_and_allow = false;

  switch (sysno) {
    // TODO(rsesek): restrict clone parameters.
    case __NR_clone:
    case __NR_epoll_pwait:
    case __NR_fdatasync:
    case __NR_flock:
    case __NR_fsync:
    case __NR_ftruncate:
#if defined(__i386__) || defined(__arm__) || defined(__mips__)
    case __NR_ftruncate64:
#endif
#if defined(__x86_64__) || defined(__aarch64__)
    case __NR_newfstatat:
    case __NR_getdents64:
#elif defined(__i386__) || defined(__arm__) || defined(__mips__)
    case __NR_fstatat64:
    case __NR_getdents:
#endif
    case __NR_getpriority:
    case __NR_ioctl:
    case __NR_mremap:
    case __NR_msync:
    // File system access cannot be restricted with seccomp-bpf on Android,
    // since the JVM classloader and other Framework features require file
    // access. It may be possible to restrict the filesystem with SELinux.
    // Currently we rely on the app/service UID isolation to create a
    // filesystem "sandbox".
#if !defined(ARCH_CPU_ARM64)
    case __NR_open:
#endif
    case __NR_openat:
    case __NR_pread64:
    case __NR_pwrite64:
    case __NR_rt_sigtimedwait:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler:
    case __NR_setpriority:
    case __NR_set_tid_address:
    case __NR_sigaltstack:
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#else
    case __NR_getrlimit:
#endif
    case __NR_uname:

    // Permit socket operations so that renderers can connect to logd and
    // debuggerd. The arguments to socket() are further restricted below.
    // Note that on i386, both of these calls map to __NR_socketcall, which
    // is demultiplexed below.
#if defined(__x86_64__) || defined(__arm__) || defined(__aarch64__) || \
      defined(__mips__)
    case __NR_getsockopt:
    case __NR_connect:
    case __NR_socket:
#endif

    // Ptrace is allowed so the Breakpad Microdumper can fork in a renderer
    // and then ptrace the parent.
    case __NR_ptrace:
      override_and_allow = true;
      break;
  }

#if defined(__x86_64__) || defined(__arm__) || defined(__aarch64__) || \
      defined(__mips__)
  if (sysno == __NR_socket) {
    const Arg<int> domain(0);
    const Arg<int> type(1);
    const Arg<int> protocol(2);
    return If(RestrictSocketArguments(domain, type, protocol), Allow())
           .Else(Error(EPERM));
  }
#elif defined(__i386__)
  if (sysno == __NR_socketcall) {
    const Arg<int> socketcall(0);
    const Arg<int> domain(1);
    const Arg<int> type(2);
    const Arg<int> protocol(3);
    return If(socketcall == SYS_CONNECT, Allow())
           .ElseIf(AllOf(socketcall == SYS_SOCKET,
                         RestrictSocketArguments(domain, type, protocol)),
                   Allow())
           .ElseIf(socketcall == SYS_GETSOCKOPT, Allow())
           .Else(Error(EPERM));
  }
#endif

  if (override_and_allow)
    return Allow();

  return SandboxBPFBasePolicy::EvaluateSyscall(sysno);
}

}  // namespace content
