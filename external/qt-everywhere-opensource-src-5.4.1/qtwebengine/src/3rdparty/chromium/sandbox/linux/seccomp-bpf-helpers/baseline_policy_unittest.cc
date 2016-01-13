// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"

#include <errno.h>
#include <linux/futex.h>
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/android_futex.h"
#include "sandbox/linux/services/linux_syscalls.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

// |pid| is the return value of a fork()-like call. This
// makes sure that if fork() succeeded the child exits
// and the parent waits for it.
void HandlePostForkReturn(pid_t pid) {
  const int kChildExitCode = 1;
  if (pid > 0) {
    int status = 0;
    PCHECK(pid == HANDLE_EINTR(waitpid(pid, &status, 0)));
    CHECK(WIFEXITED(status));
    CHECK_EQ(kChildExitCode, WEXITSTATUS(status));
  } else if (pid == 0) {
    _exit(kChildExitCode);
  }
}

// Check that HandlePostForkReturn works.
TEST(BaselinePolicy, HandlePostForkReturn) {
  pid_t pid = fork();
  HandlePostForkReturn(pid);
}

// This also tests that read(), write() and fstat() are allowed.
void TestPipeOrSocketPair(base::ScopedFD read_end, base::ScopedFD write_end) {
  BPF_ASSERT_LE(0, read_end.get());
  BPF_ASSERT_LE(0, write_end.get());
  struct stat stat_buf;
  int sys_ret = fstat(read_end.get(), &stat_buf);
  BPF_ASSERT_EQ(0, sys_ret);
  BPF_ASSERT(S_ISFIFO(stat_buf.st_mode) || S_ISSOCK(stat_buf.st_mode));

  const ssize_t kTestTransferSize = 4;
  static const char kTestString[kTestTransferSize] = {'T', 'E', 'S', 'T'};
  ssize_t transfered = 0;

  transfered =
      HANDLE_EINTR(write(write_end.get(), kTestString, kTestTransferSize));
  BPF_ASSERT_EQ(kTestTransferSize, transfered);
  char read_buf[kTestTransferSize + 1] = {0};
  transfered = HANDLE_EINTR(read(read_end.get(), read_buf, sizeof(read_buf)));
  BPF_ASSERT_EQ(kTestTransferSize, transfered);
  BPF_ASSERT_EQ(0, memcmp(kTestString, read_buf, kTestTransferSize));
}

// Test that a few easy-to-test system calls are allowed.
BPF_TEST_C(BaselinePolicy, BaselinePolicyBasicAllowed, BaselinePolicy) {
  BPF_ASSERT_EQ(0, sched_yield());

  int pipefd[2];
  int sys_ret = pipe(pipefd);
  BPF_ASSERT_EQ(0, sys_ret);
  TestPipeOrSocketPair(base::ScopedFD(pipefd[0]), base::ScopedFD(pipefd[1]));

  BPF_ASSERT_LE(1, getpid());
  BPF_ASSERT_LE(0, getuid());
}

BPF_TEST_C(BaselinePolicy, FchmodErrno, BaselinePolicy) {
  int ret = fchmod(-1, 07777);
  BPF_ASSERT_EQ(-1, ret);
  // Without the sandbox, this would EBADF instead.
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST_C(BaselinePolicy, ForkErrno, BaselinePolicy) {
  errno = 0;
  pid_t pid = fork();
  const int fork_errno = errno;
  HandlePostForkReturn(pid);

  BPF_ASSERT_EQ(-1, pid);
  BPF_ASSERT_EQ(EPERM, fork_errno);
}

pid_t ForkX86Glibc() {
  return syscall(__NR_clone, CLONE_PARENT_SETTID | SIGCHLD);
}

BPF_TEST_C(BaselinePolicy, ForkX86Eperm, BaselinePolicy) {
  errno = 0;
  pid_t pid = ForkX86Glibc();
  const int fork_errno = errno;
  HandlePostForkReturn(pid);

  BPF_ASSERT_EQ(-1, pid);
  BPF_ASSERT_EQ(EPERM, fork_errno);
}

pid_t ForkARMGlibc() {
  return syscall(__NR_clone,
                 CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD);
}

BPF_TEST_C(BaselinePolicy, ForkArmEperm, BaselinePolicy) {
  errno = 0;
  pid_t pid = ForkARMGlibc();
  const int fork_errno = errno;
  HandlePostForkReturn(pid);

  BPF_ASSERT_EQ(-1, pid);
  BPF_ASSERT_EQ(EPERM, fork_errno);
}

BPF_TEST_C(BaselinePolicy, CreateThread, BaselinePolicy) {
  base::Thread thread("sandbox_tests");
  BPF_ASSERT(thread.Start());
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 DisallowedCloneFlagCrashes,
                 DEATH_MESSAGE(GetCloneErrorMessageContentForTests()),
                 BaselinePolicy) {
  pid_t pid = syscall(__NR_clone, CLONE_THREAD | SIGCHLD);
  HandlePostForkReturn(pid);
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 DisallowedKillCrashes,
                 DEATH_MESSAGE(GetKillErrorMessageContentForTests()),
                 BaselinePolicy) {
  BPF_ASSERT_NE(1, getpid());
  kill(1, 0);
  _exit(1);
}

BPF_TEST_C(BaselinePolicy, CanKillSelf, BaselinePolicy) {
  int sys_ret = kill(getpid(), 0);
  BPF_ASSERT_EQ(0, sys_ret);
}

BPF_TEST_C(BaselinePolicy, Socketpair, BaselinePolicy) {
  int sv[2];
  int sys_ret = socketpair(AF_UNIX, SOCK_DGRAM, 0, sv);
  BPF_ASSERT_EQ(0, sys_ret);
  TestPipeOrSocketPair(base::ScopedFD(sv[0]), base::ScopedFD(sv[1]));

  sys_ret = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv);
  BPF_ASSERT_EQ(0, sys_ret);
  TestPipeOrSocketPair(base::ScopedFD(sv[0]), base::ScopedFD(sv[1]));
}

// Not all architectures can restrict the domain for socketpair().
#if defined(__x86_64__) || defined(__arm__)
BPF_DEATH_TEST_C(BaselinePolicy,
                 SocketpairWrongDomain,
                 DEATH_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  int sv[2];
  ignore_result(socketpair(AF_INET, SOCK_STREAM, 0, sv));
  _exit(1);
}
#endif  // defined(__x86_64__) || defined(__arm__)

BPF_TEST_C(BaselinePolicy, EPERM_open, BaselinePolicy) {
  errno = 0;
  int sys_ret = open("/proc/cpuinfo", O_RDONLY);
  BPF_ASSERT_EQ(-1, sys_ret);
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST_C(BaselinePolicy, EPERM_access, BaselinePolicy) {
  errno = 0;
  int sys_ret = access("/proc/cpuinfo", R_OK);
  BPF_ASSERT_EQ(-1, sys_ret);
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST_C(BaselinePolicy, EPERM_getcwd, BaselinePolicy) {
  errno = 0;
  char buf[1024];
  char* cwd = getcwd(buf, sizeof(buf));
  BPF_ASSERT_EQ(NULL, cwd);
  BPF_ASSERT_EQ(EPERM, errno);
}

// A failing test using this macro could be problematic since we perform
// system calls by passing "0" as every argument.
// The kernel could SIGSEGV the process or the system call itself could reboot
// the machine. Some thoughts have been given when hand-picking the system
// calls below to limit any potential side effects outside of the current
// process.
#define TEST_BASELINE_SIGSYS(sysno)                                 \
  BPF_DEATH_TEST_C(BaselinePolicy,                                  \
                   SIGSYS_##sysno,                                  \
                   DEATH_MESSAGE(GetErrorMessageContentForTests()), \
                   BaselinePolicy) {                                \
    syscall(sysno, 0, 0, 0, 0, 0, 0);                               \
    _exit(1);                                                       \
  }

TEST_BASELINE_SIGSYS(__NR_syslog);
TEST_BASELINE_SIGSYS(__NR_sched_setaffinity);
TEST_BASELINE_SIGSYS(__NR_timer_create);
TEST_BASELINE_SIGSYS(__NR_io_cancel);
TEST_BASELINE_SIGSYS(__NR_ptrace);
TEST_BASELINE_SIGSYS(__NR_eventfd);
TEST_BASELINE_SIGSYS(__NR_fgetxattr);
TEST_BASELINE_SIGSYS(__NR_fanotify_init);
TEST_BASELINE_SIGSYS(__NR_swapon);
TEST_BASELINE_SIGSYS(__NR_chroot);
TEST_BASELINE_SIGSYS(__NR_acct);
TEST_BASELINE_SIGSYS(__NR_sysinfo);
TEST_BASELINE_SIGSYS(__NR_inotify_init);
TEST_BASELINE_SIGSYS(__NR_init_module);
TEST_BASELINE_SIGSYS(__NR_keyctl);
TEST_BASELINE_SIGSYS(__NR_mq_open);
TEST_BASELINE_SIGSYS(__NR_vserver);
TEST_BASELINE_SIGSYS(__NR_getcpu);
TEST_BASELINE_SIGSYS(__NR_setpgid);
TEST_BASELINE_SIGSYS(__NR_getitimer);

#if !defined(OS_ANDROID)
BPF_DEATH_TEST_C(BaselinePolicy,
                 FutexWithRequeuePriorityInheritence,
                 DEATH_MESSAGE(GetFutexErrorMessageContentForTests()),
                 BaselinePolicy) {
  syscall(__NR_futex, NULL, FUTEX_CMP_REQUEUE_PI, 0, NULL, NULL, 0);
  _exit(1);
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 FutexWithRequeuePriorityInheritencePrivate,
                 DEATH_MESSAGE(GetFutexErrorMessageContentForTests()),
                 BaselinePolicy) {
  syscall(__NR_futex, NULL, FUTEX_CMP_REQUEUE_PI_PRIVATE, 0, NULL, NULL, 0);
  _exit(1);
}
#endif  // !defined(OS_ANDROID)

BPF_TEST_C(BaselinePolicy, PrctlDumpable, BaselinePolicy) {
  const int is_dumpable = prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
  BPF_ASSERT(is_dumpable == 1 || is_dumpable == 0);
  const int prctl_ret = prctl(PR_SET_DUMPABLE, is_dumpable, 0, 0, 0, 0);
  BPF_ASSERT_EQ(0, prctl_ret);
}

// Workaround incomplete Android headers.
#if !defined(PR_CAPBSET_READ)
#define PR_CAPBSET_READ 23
#endif

BPF_DEATH_TEST_C(BaselinePolicy,
                 PrctlSigsys,
                 DEATH_MESSAGE(GetPrctlErrorMessageContentForTests()),
                 BaselinePolicy) {
  prctl(PR_CAPBSET_READ, 0, 0, 0, 0);
  _exit(1);
}

}  // namespace

}  // namespace sandbox
