// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"

#include <errno.h>
#include <fcntl.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <linux/net.h>
#include <sched.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/android_futex.h"

#if defined(OS_ANDROID)
#if !defined(F_DUPFD_CLOEXEC)
#define F_DUPFD_CLOEXEC (F_LINUX_SPECIFIC_BASE + 6)
#endif
#endif

#if defined(__arm__) && !defined(MAP_STACK)
#define MAP_STACK 0x20000  // Daisy build environment has old headers.
#endif

namespace {

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

inline bool IsAndroid() {
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

}  // namespace.

namespace sandbox {

// Allow Glibc's and Android pthread creation flags, crash on any other
// thread creation attempts and EPERM attempts to use neither
// CLONE_VM, nor CLONE_THREAD, which includes all fork() implementations.
ErrorCode RestrictCloneToThreadsAndEPERMFork(SandboxBPF* sandbox) {
  if (!IsAndroid()) {
    const uint64_t kGlibcPthreadFlags =
        CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD |
        CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID |
        CLONE_CHILD_CLEARTID;

    return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                         kGlibcPthreadFlags,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                         CLONE_VM | CLONE_THREAD,
                         sandbox->Trap(SIGSYSCloneFailure, NULL),
                         ErrorCode(EPERM)));
  } else {
    const uint64_t kAndroidCloneMask = CLONE_VM | CLONE_FS | CLONE_FILES |
                                       CLONE_SIGHAND | CLONE_THREAD |
                                       CLONE_SYSVSEM;
    const uint64_t kObsoleteAndroidCloneMask =
        kAndroidCloneMask | CLONE_DETACHED;

    return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                         kAndroidCloneMask,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                         kObsoleteAndroidCloneMask,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                         CLONE_VM | CLONE_THREAD,
                         sandbox->Trap(SIGSYSCloneFailure, NULL),
                         ErrorCode(EPERM))));
  }
}

ErrorCode RestrictPrctl(SandboxBPF* sandbox) {
  // Will need to add seccomp compositing in the future. PR_SET_PTRACER is
  // used by breakpad but not needed anymore.
  return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       PR_SET_NAME, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       PR_SET_DUMPABLE, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       PR_GET_DUMPABLE, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Trap(SIGSYSPrctlFailure, NULL))));
}

ErrorCode RestrictIoctl(SandboxBPF* sandbox) {
  return sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, TCGETS,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, FIONREAD,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
                       sandbox->Trap(SIGSYSIoctlFailure, NULL)));
}

ErrorCode RestrictMmapFlags(SandboxBPF* sandbox) {
  // The flags you see are actually the allowed ones, and the variable is a
  // "denied" mask because of the negation operator.
  // Significantly, we don't permit MAP_HUGETLB, or the newer flags such as
  // MAP_POPULATE.
  // TODO(davidung), remove MAP_DENYWRITE with updated Tegra libraries.
  uint32_t denied_mask = ~(MAP_SHARED | MAP_PRIVATE | MAP_ANONYMOUS |
                           MAP_STACK | MAP_NORESERVE | MAP_FIXED |
                           MAP_DENYWRITE);
  return sandbox->Cond(3, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                       denied_mask,
                       sandbox->Trap(CrashSIGSYS_Handler, NULL),
                       ErrorCode(ErrorCode::ERR_ALLOWED));
}

ErrorCode RestrictMprotectFlags(SandboxBPF* sandbox) {
  // The flags you see are actually the allowed ones, and the variable is a
  // "denied" mask because of the negation operator.
  // Significantly, we don't permit weird undocumented flags such as
  // PROT_GROWSDOWN.
  uint32_t denied_mask = ~(PROT_READ | PROT_WRITE | PROT_EXEC);
  return sandbox->Cond(2, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                       denied_mask,
                       sandbox->Trap(CrashSIGSYS_Handler, NULL),
                       ErrorCode(ErrorCode::ERR_ALLOWED));
}

ErrorCode RestrictFcntlCommands(SandboxBPF* sandbox) {
  // We also restrict the flags in F_SETFL. We don't want to permit flags with
  // a history of trouble such as O_DIRECT. The flags you see are actually the
  // allowed ones, and the variable is a "denied" mask because of the negation
  // operator.
  // Glibc overrides the kernel's O_LARGEFILE value. Account for this.
  int kOLargeFileFlag = O_LARGEFILE;
  if (IsArchitectureX86_64() || IsArchitectureI386())
    kOLargeFileFlag = 0100000;

  // TODO(jln): add TP_LONG/TP_SIZET types.
  ErrorCode::ArgType mask_long_type;
  if (sizeof(long) == 8)
    mask_long_type = ErrorCode::TP_64BIT;
  else if (sizeof(long) == 4)
    mask_long_type = ErrorCode::TP_32BIT;
  else
    NOTREACHED();

  unsigned long denied_mask = ~(O_ACCMODE | O_APPEND | O_NONBLOCK | O_SYNC |
                                kOLargeFileFlag | O_CLOEXEC | O_NOATIME);
  return sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_GETFL,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_SETFL,
                       sandbox->Cond(2, mask_long_type,
                                     ErrorCode::OP_HAS_ANY_BITS, denied_mask,
                                     sandbox->Trap(CrashSIGSYS_Handler, NULL),
                                     ErrorCode(ErrorCode::ERR_ALLOWED)),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_GETFD,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_SETFD,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_DUPFD,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_SETLK,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_SETLKW,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_GETLK,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_DUPFD_CLOEXEC,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Trap(CrashSIGSYS_Handler, NULL))))))))));
}

#if defined(__i386__)
ErrorCode RestrictSocketcallCommand(SandboxBPF* sandbox) {
  // Unfortunately, we are unable to restrict the first parameter to
  // socketpair(2). Whilst initially sounding bad, it's noteworthy that very
  // few protocols actually support socketpair(2). The scary call that we're
  // worried about, socket(2), remains blocked.
  return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_SOCKETPAIR, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_SEND, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_RECV, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_SENDTO, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_RECVFROM, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_SHUTDOWN, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_SENDMSG, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_RECVMSG, ErrorCode(ErrorCode::ERR_ALLOWED),
         ErrorCode(EPERM)))))))));
}
#endif

ErrorCode RestrictKillTarget(pid_t target_pid, SandboxBPF* sandbox, int sysno) {
  switch (sysno) {
    case __NR_kill:
    case __NR_tgkill:
      return sandbox->Cond(0,
                           ErrorCode::TP_32BIT,
                           ErrorCode::OP_EQUAL,
                           target_pid,
                           ErrorCode(ErrorCode::ERR_ALLOWED),
                           sandbox->Trap(SIGSYSKillFailure, NULL));
    case __NR_tkill:
      return sandbox->Trap(SIGSYSKillFailure, NULL);
    default:
      NOTREACHED();
      return sandbox->Trap(CrashSIGSYS_Handler, NULL);
  }
}

ErrorCode RestrictFutex(SandboxBPF* sandbox) {
  // In futex.c, the kernel does "int cmd = op & FUTEX_CMD_MASK;". We need to
  // make sure that the combination below will cover every way to get
  // FUTEX_CMP_REQUEUE_PI.
  const int kBannedFutexBits =
      ~(FUTEX_CMD_MASK | FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME);
  COMPILE_ASSERT(0 == kBannedFutexBits,
                 need_to_explicitly_blacklist_more_bits);

  return sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       FUTEX_CMP_REQUEUE_PI,
                       sandbox->Trap(SIGSYSFutexFailure, NULL),
         sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       FUTEX_CMP_REQUEUE_PI_PRIVATE,
                       sandbox->Trap(SIGSYSFutexFailure, NULL),
         sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       FUTEX_CMP_REQUEUE_PI | FUTEX_CLOCK_REALTIME,
                       sandbox->Trap(SIGSYSFutexFailure, NULL),
         sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       FUTEX_CMP_REQUEUE_PI_PRIVATE | FUTEX_CLOCK_REALTIME,
                       sandbox->Trap(SIGSYSFutexFailure, NULL),
         ErrorCode(ErrorCode::ERR_ALLOWED)))));
}

}  // namespace sandbox.
