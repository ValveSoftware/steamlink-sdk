// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SYSCALL_PARAMETERS_RESTRICTIONS_H_
#define SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SYSCALL_PARAMETERS_RESTRICTIONS_H_

#include <unistd.h>

#include "build/build_config.h"
#include "sandbox/sandbox_export.h"

// These are helpers to build seccomp-bpf policies, i.e. policies for a
// sandbox that reduces the Linux kernel's attack surface. They return an
// SANDBOX_EXPORT ErrorCode suitable to restrict certain system call parameters.

namespace sandbox {

class ErrorCode;
class SandboxBPF;

// Allow clone(2) for threads.
// Reject fork(2) attempts with EPERM.
// Don't restrict on ASAN.
// Crash if anything else is attempted.
SANDBOX_EXPORT ErrorCode
    RestrictCloneToThreadsAndEPERMFork(SandboxBPF* sandbox);

// Allow PR_SET_NAME, PR_SET_DUMPABLE, PR_GET_DUMPABLE.
// Crash if anything else is attempted.
SANDBOX_EXPORT ErrorCode RestrictPrctl(SandboxBPF* sandbox);

// Allow TCGETS and FIONREAD.
// Crash if anything else is attempted.
SANDBOX_EXPORT ErrorCode RestrictIoctl(SandboxBPF* sandbox);

// Restrict the flags argument in mmap(2).
// Only allow: MAP_SHARED | MAP_PRIVATE | MAP_ANONYMOUS |
// MAP_STACK | MAP_NORESERVE | MAP_FIXED | MAP_DENYWRITE.
// Crash if any other flag is used.
SANDBOX_EXPORT ErrorCode RestrictMmapFlags(SandboxBPF* sandbox);

// Restrict the prot argument in mprotect(2).
// Only allow: PROT_READ | PROT_WRITE | PROT_EXEC.
SANDBOX_EXPORT ErrorCode RestrictMprotectFlags(SandboxBPF* sandbox);

// Restrict fcntl(2) cmd argument to:
// We allow F_GETFL, F_SETFL, F_GETFD, F_SETFD, F_DUPFD, F_DUPFD_CLOEXEC,
// F_SETLK, F_SETLKW and F_GETLK.
// Also, in F_SETFL, restrict the allowed flags to: O_ACCMODE | O_APPEND |
// O_NONBLOCK | O_SYNC | O_LARGEFILE | O_CLOEXEC | O_NOATIME.
SANDBOX_EXPORT ErrorCode RestrictFcntlCommands(SandboxBPF* sandbox);

#if defined(__i386__)
// Restrict socketcall(2) to only allow socketpair(2), send(2), recv(2),
// sendto(2), recvfrom(2), shutdown(2), sendmsg(2) and recvmsg(2).
SANDBOX_EXPORT ErrorCode RestrictSocketcallCommand(SandboxBPF* sandbox);
#endif

// Restrict |sysno| (which must be kill, tkill or tgkill) by allowing tgkill or
// kill iff the first parameter is |target_pid|, crashing otherwise or if
// |sysno| is tkill.
ErrorCode RestrictKillTarget(pid_t target_pid, SandboxBPF* sandbox, int sysno);

// Crash if FUTEX_CMP_REQUEUE_PI is used in the second argument of futex(2).
ErrorCode RestrictFutex(SandboxBPF* sandbox);

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SYSCALL_PARAMETERS_RESTRICTIONS_H_
