// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_process.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "sandbox/linux/syscall_broker/broker_client.h"
#include "sandbox/linux/tests/scoped_temporary_file.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace syscall_broker {

class BrokerProcessTestHelper {
 public:
  static void CloseChannel(BrokerProcess* broker) { broker->CloseChannel(); }
  // Get the client's IPC descriptor to send IPC requests directly.
  // TODO(jln): refator tests to get rid of this.
  static int GetIPCDescriptor(const BrokerProcess* broker) {
    return broker->broker_client_->GetIPCDescriptor();
  }
};

namespace {

bool NoOpCallback() {
  return true;
}

}  // namespace

TEST(BrokerProcess, CreateAndDestroy) {
  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(BrokerFilePermission::ReadOnly("/proc/cpuinfo"));

  std::unique_ptr<BrokerProcess> open_broker(
      new BrokerProcess(EPERM, permissions));
  ASSERT_TRUE(open_broker->Init(base::Bind(&NoOpCallback)));

  ASSERT_TRUE(TestUtils::CurrentProcessHasChildren());
  // Destroy the broker and check it has exited properly.
  open_broker.reset();
  ASSERT_FALSE(TestUtils::CurrentProcessHasChildren());
}

TEST(BrokerProcess, TestOpenAccessNull) {
  std::vector<BrokerFilePermission> empty;
  BrokerProcess open_broker(EPERM, empty);
  ASSERT_TRUE(open_broker.Init(base::Bind(&NoOpCallback)));

  int fd = open_broker.Open(NULL, O_RDONLY);
  ASSERT_EQ(fd, -EFAULT);

  int ret = open_broker.Access(NULL, F_OK);
  ASSERT_EQ(ret, -EFAULT);
}

void TestOpenFilePerms(bool fast_check_in_client, int denied_errno) {
  const char kR_WhiteListed[] = "/proc/DOESNOTEXIST1";
  // We can't debug the init process, and shouldn't be able to access
  // its auxv file.
  const char kR_WhiteListedButDenied[] = "/proc/1/auxv";
  const char kW_WhiteListed[] = "/proc/DOESNOTEXIST2";
  const char kRW_WhiteListed[] = "/proc/DOESNOTEXIST3";
  const char k_NotWhitelisted[] = "/proc/DOESNOTEXIST4";

  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(BrokerFilePermission::ReadOnly(kR_WhiteListed));
  permissions.push_back(
      BrokerFilePermission::ReadOnly(kR_WhiteListedButDenied));
  permissions.push_back(BrokerFilePermission::WriteOnly(kW_WhiteListed));
  permissions.push_back(BrokerFilePermission::ReadWrite(kRW_WhiteListed));

  BrokerProcess open_broker(denied_errno, permissions, fast_check_in_client);
  ASSERT_TRUE(open_broker.Init(base::Bind(&NoOpCallback)));

  int fd = -1;
  fd = open_broker.Open(kR_WhiteListed, O_RDONLY);
  ASSERT_EQ(fd, -ENOENT);
  fd = open_broker.Open(kR_WhiteListed, O_WRONLY);
  ASSERT_EQ(fd, -denied_errno);
  fd = open_broker.Open(kR_WhiteListed, O_RDWR);
  ASSERT_EQ(fd, -denied_errno);
  int ret = -1;
  ret = open_broker.Access(kR_WhiteListed, F_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.Access(kR_WhiteListed, R_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.Access(kR_WhiteListed, W_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(kR_WhiteListed, R_OK | W_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(kR_WhiteListed, X_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(kR_WhiteListed, R_OK | X_OK);
  ASSERT_EQ(ret, -denied_errno);

  // Android sometimes runs tests as root.
  // This part of the test requires a process that doesn't have
  // CAP_DAC_OVERRIDE. We check against a root euid as a proxy for that.
  if (geteuid()) {
    fd = open_broker.Open(kR_WhiteListedButDenied, O_RDONLY);
    // The broker process will allow this, but the normal permission system
    // won't.
    ASSERT_EQ(fd, -EACCES);
    fd = open_broker.Open(kR_WhiteListedButDenied, O_WRONLY);
    ASSERT_EQ(fd, -denied_errno);
    fd = open_broker.Open(kR_WhiteListedButDenied, O_RDWR);
    ASSERT_EQ(fd, -denied_errno);
    ret = open_broker.Access(kR_WhiteListedButDenied, F_OK);
    // The normal permission system will let us check that the file exists.
    ASSERT_EQ(ret, 0);
    ret = open_broker.Access(kR_WhiteListedButDenied, R_OK);
    ASSERT_EQ(ret, -EACCES);
    ret = open_broker.Access(kR_WhiteListedButDenied, W_OK);
    ASSERT_EQ(ret, -denied_errno);
    ret = open_broker.Access(kR_WhiteListedButDenied, R_OK | W_OK);
    ASSERT_EQ(ret, -denied_errno);
    ret = open_broker.Access(kR_WhiteListedButDenied, X_OK);
    ASSERT_EQ(ret, -denied_errno);
    ret = open_broker.Access(kR_WhiteListedButDenied, R_OK | X_OK);
    ASSERT_EQ(ret, -denied_errno);
  }

  fd = open_broker.Open(kW_WhiteListed, O_RDONLY);
  ASSERT_EQ(fd, -denied_errno);
  fd = open_broker.Open(kW_WhiteListed, O_WRONLY);
  ASSERT_EQ(fd, -ENOENT);
  fd = open_broker.Open(kW_WhiteListed, O_RDWR);
  ASSERT_EQ(fd, -denied_errno);
  ret = open_broker.Access(kW_WhiteListed, F_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.Access(kW_WhiteListed, R_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(kW_WhiteListed, W_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.Access(kW_WhiteListed, R_OK | W_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(kW_WhiteListed, X_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(kW_WhiteListed, R_OK | X_OK);
  ASSERT_EQ(ret, -denied_errno);

  fd = open_broker.Open(kRW_WhiteListed, O_RDONLY);
  ASSERT_EQ(fd, -ENOENT);
  fd = open_broker.Open(kRW_WhiteListed, O_WRONLY);
  ASSERT_EQ(fd, -ENOENT);
  fd = open_broker.Open(kRW_WhiteListed, O_RDWR);
  ASSERT_EQ(fd, -ENOENT);
  ret = open_broker.Access(kRW_WhiteListed, F_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.Access(kRW_WhiteListed, R_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.Access(kRW_WhiteListed, W_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.Access(kRW_WhiteListed, R_OK | W_OK);
  ASSERT_EQ(ret, -ENOENT);
  ret = open_broker.Access(kRW_WhiteListed, X_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(kRW_WhiteListed, R_OK | X_OK);
  ASSERT_EQ(ret, -denied_errno);

  fd = open_broker.Open(k_NotWhitelisted, O_RDONLY);
  ASSERT_EQ(fd, -denied_errno);
  fd = open_broker.Open(k_NotWhitelisted, O_WRONLY);
  ASSERT_EQ(fd, -denied_errno);
  fd = open_broker.Open(k_NotWhitelisted, O_RDWR);
  ASSERT_EQ(fd, -denied_errno);
  ret = open_broker.Access(k_NotWhitelisted, F_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(k_NotWhitelisted, R_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(k_NotWhitelisted, W_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(k_NotWhitelisted, R_OK | W_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(k_NotWhitelisted, X_OK);
  ASSERT_EQ(ret, -denied_errno);
  ret = open_broker.Access(k_NotWhitelisted, R_OK | X_OK);
  ASSERT_EQ(ret, -denied_errno);

  // We have some extra sanity check for clearly wrong values.
  fd = open_broker.Open(kRW_WhiteListed, O_RDONLY | O_WRONLY | O_RDWR);
  ASSERT_EQ(fd, -denied_errno);

  // It makes no sense to allow O_CREAT in a 2-parameters open. Ensure this
  // is denied.
  fd = open_broker.Open(kRW_WhiteListed, O_RDWR | O_CREAT);
  ASSERT_EQ(fd, -denied_errno);
}

// Run the same thing twice. The second time, we make sure that no security
// check is performed on the client.
TEST(BrokerProcess, OpenFilePermsWithClientCheck) {
  TestOpenFilePerms(true /* fast_check_in_client */, EPERM);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenOpenFilePermsNoClientCheck) {
  TestOpenFilePerms(false /* fast_check_in_client */, EPERM);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

// Run the same twice again, but with ENOENT instead of EPERM.
TEST(BrokerProcess, OpenFilePermsWithClientCheckNoEnt) {
  TestOpenFilePerms(true /* fast_check_in_client */, ENOENT);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenOpenFilePermsNoClientCheckNoEnt) {
  TestOpenFilePerms(false /* fast_check_in_client */, ENOENT);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

void TestBadPaths(bool fast_check_in_client) {
  const char kFileCpuInfo[] = "/proc/cpuinfo";
  const char kNotAbsPath[] = "proc/cpuinfo";
  const char kDotDotStart[] = "/../proc/cpuinfo";
  const char kDotDotMiddle[] = "/proc/self/../cpuinfo";
  const char kDotDotEnd[] = "/proc/..";
  const char kTrailingSlash[] = "/proc/";

  std::vector<BrokerFilePermission> permissions;

  permissions.push_back(BrokerFilePermission::ReadOnlyRecursive("/proc/"));
  std::unique_ptr<BrokerProcess> open_broker(
      new BrokerProcess(EPERM, permissions, fast_check_in_client));
  ASSERT_TRUE(open_broker->Init(base::Bind(&NoOpCallback)));
  // Open cpuinfo via the broker.
  int cpuinfo_fd = open_broker->Open(kFileCpuInfo, O_RDONLY);
  base::ScopedFD cpuinfo_fd_closer(cpuinfo_fd);
  ASSERT_GE(cpuinfo_fd, 0);

  int fd = -1;
  int can_access;

  can_access = open_broker->Access(kNotAbsPath, R_OK);
  ASSERT_EQ(can_access, -EPERM);
  fd = open_broker->Open(kNotAbsPath, O_RDONLY);
  ASSERT_EQ(fd, -EPERM);

  can_access = open_broker->Access(kDotDotStart, R_OK);
  ASSERT_EQ(can_access, -EPERM);
  fd = open_broker->Open(kDotDotStart, O_RDONLY);
  ASSERT_EQ(fd, -EPERM);

  can_access = open_broker->Access(kDotDotMiddle, R_OK);
  ASSERT_EQ(can_access, -EPERM);
  fd = open_broker->Open(kDotDotMiddle, O_RDONLY);
  ASSERT_EQ(fd, -EPERM);

  can_access = open_broker->Access(kDotDotEnd, R_OK);
  ASSERT_EQ(can_access, -EPERM);
  fd = open_broker->Open(kDotDotEnd, O_RDONLY);
  ASSERT_EQ(fd, -EPERM);

  can_access = open_broker->Access(kTrailingSlash, R_OK);
  ASSERT_EQ(can_access, -EPERM);
  fd = open_broker->Open(kTrailingSlash, O_RDONLY);
  ASSERT_EQ(fd, -EPERM);
}

TEST(BrokerProcess, BadPathsClientCheck) {
  TestBadPaths(true /* fast_check_in_client */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, BadPathsNoClientCheck) {
  TestBadPaths(false /* fast_check_in_client */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

void TestOpenCpuinfo(bool fast_check_in_client, bool recursive) {
  const char kFileCpuInfo[] = "/proc/cpuinfo";
  const char kDirProc[] = "/proc/";

  std::vector<BrokerFilePermission> permissions;
  if (recursive)
    permissions.push_back(BrokerFilePermission::ReadOnlyRecursive(kDirProc));
  else
    permissions.push_back(BrokerFilePermission::ReadOnly(kFileCpuInfo));

  std::unique_ptr<BrokerProcess> open_broker(
      new BrokerProcess(EPERM, permissions, fast_check_in_client));
  ASSERT_TRUE(open_broker->Init(base::Bind(&NoOpCallback)));

  int fd = -1;
  fd = open_broker->Open(kFileCpuInfo, O_RDWR);
  base::ScopedFD fd_closer(fd);
  ASSERT_EQ(fd, -EPERM);

  // Check we can read /proc/cpuinfo.
  int can_access = open_broker->Access(kFileCpuInfo, R_OK);
  ASSERT_EQ(can_access, 0);
  can_access = open_broker->Access(kFileCpuInfo, W_OK);
  ASSERT_EQ(can_access, -EPERM);
  // Check we can not write /proc/cpuinfo.

  // Open cpuinfo via the broker.
  int cpuinfo_fd = open_broker->Open(kFileCpuInfo, O_RDONLY);
  base::ScopedFD cpuinfo_fd_closer(cpuinfo_fd);
  ASSERT_GE(cpuinfo_fd, 0);
  char buf[3];
  memset(buf, 0, sizeof(buf));
  int read_len1 = read(cpuinfo_fd, buf, sizeof(buf));
  ASSERT_GT(read_len1, 0);

  // Open cpuinfo directly.
  int cpuinfo_fd2 = open(kFileCpuInfo, O_RDONLY);
  base::ScopedFD cpuinfo_fd2_closer(cpuinfo_fd2);
  ASSERT_GE(cpuinfo_fd2, 0);
  char buf2[3];
  memset(buf2, 1, sizeof(buf2));
  int read_len2 = read(cpuinfo_fd2, buf2, sizeof(buf2));
  ASSERT_GT(read_len1, 0);

  // The following is not guaranteed true, but will be in practice.
  ASSERT_EQ(read_len1, read_len2);
  // Compare the cpuinfo as returned by the broker with the one we opened
  // ourselves.
  ASSERT_EQ(memcmp(buf, buf2, read_len1), 0);

  ASSERT_TRUE(TestUtils::CurrentProcessHasChildren());
  open_broker.reset();
  ASSERT_FALSE(TestUtils::CurrentProcessHasChildren());
}

// Run this test 4 times. With and without the check in client
// and using a recursive path.
TEST(BrokerProcess, OpenCpuinfoWithClientCheck) {
  TestOpenCpuinfo(true /* fast_check_in_client */, false /* not recursive */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenCpuinfoNoClientCheck) {
  TestOpenCpuinfo(false /* fast_check_in_client */, false /* not recursive */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenCpuinfoWithClientCheckRecursive) {
  TestOpenCpuinfo(true /* fast_check_in_client */, true /* recursive */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenCpuinfoNoClientCheckRecursive) {
  TestOpenCpuinfo(false /* fast_check_in_client */, true /* recursive */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenFileRW) {
  ScopedTemporaryFile tempfile;
  const char* tempfile_name = tempfile.full_file_name();

  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(BrokerFilePermission::ReadWrite(tempfile_name));

  BrokerProcess open_broker(EPERM, permissions);
  ASSERT_TRUE(open_broker.Init(base::Bind(&NoOpCallback)));

  // Check we can access that file with read or write.
  int can_access = open_broker.Access(tempfile_name, R_OK | W_OK);
  ASSERT_EQ(can_access, 0);

  int tempfile2 = -1;
  tempfile2 = open_broker.Open(tempfile_name, O_RDWR);
  ASSERT_GE(tempfile2, 0);

  // Write to the descriptor opened by the broker.
  char test_text[] = "TESTTESTTEST";
  ssize_t len = write(tempfile2, test_text, sizeof(test_text));
  ASSERT_EQ(len, static_cast<ssize_t>(sizeof(test_text)));

  // Read back from the original file descriptor what we wrote through
  // the descriptor provided by the broker.
  char buf[1024];
  len = read(tempfile.fd(), buf, sizeof(buf));

  ASSERT_EQ(len, static_cast<ssize_t>(sizeof(test_text)));
  ASSERT_EQ(memcmp(test_text, buf, sizeof(test_text)), 0);

  ASSERT_EQ(close(tempfile2), 0);
}

// SANDBOX_TEST because the process could die with a SIGPIPE
// and we want this to happen in a subprocess.
SANDBOX_TEST(BrokerProcess, BrokerDied) {
  const char kCpuInfo[] = "/proc/cpuinfo";
  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(BrokerFilePermission::ReadOnly(kCpuInfo));

  BrokerProcess open_broker(EPERM, permissions, true /* fast_check_in_client */,
                            true /* quiet_failures_for_tests */);
  SANDBOX_ASSERT(open_broker.Init(base::Bind(&NoOpCallback)));
  const pid_t broker_pid = open_broker.broker_pid();
  SANDBOX_ASSERT(kill(broker_pid, SIGKILL) == 0);

  // Now we check that the broker has been signaled, but do not reap it.
  siginfo_t process_info;
  SANDBOX_ASSERT(HANDLE_EINTR(waitid(
                     P_PID, broker_pid, &process_info, WEXITED | WNOWAIT)) ==
                 0);
  SANDBOX_ASSERT(broker_pid == process_info.si_pid);
  SANDBOX_ASSERT(CLD_KILLED == process_info.si_code);
  SANDBOX_ASSERT(SIGKILL == process_info.si_status);

  // Check that doing Open with a dead broker won't SIGPIPE us.
  SANDBOX_ASSERT(open_broker.Open(kCpuInfo, O_RDONLY) == -ENOMEM);
  SANDBOX_ASSERT(open_broker.Access(kCpuInfo, O_RDONLY) == -ENOMEM);
}

void TestOpenComplexFlags(bool fast_check_in_client) {
  const char kCpuInfo[] = "/proc/cpuinfo";
  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(BrokerFilePermission::ReadOnly(kCpuInfo));

  BrokerProcess open_broker(EPERM, permissions, fast_check_in_client);
  ASSERT_TRUE(open_broker.Init(base::Bind(&NoOpCallback)));
  // Test that we do the right thing for O_CLOEXEC and O_NONBLOCK.
  int fd = -1;
  int ret = 0;
  fd = open_broker.Open(kCpuInfo, O_RDONLY);
  ASSERT_GE(fd, 0);
  ret = fcntl(fd, F_GETFL);
  ASSERT_NE(-1, ret);
  // The descriptor shouldn't have the O_CLOEXEC attribute, nor O_NONBLOCK.
  ASSERT_EQ(0, ret & (O_CLOEXEC | O_NONBLOCK));
  ASSERT_EQ(0, close(fd));

  fd = open_broker.Open(kCpuInfo, O_RDONLY | O_CLOEXEC);
  ASSERT_GE(fd, 0);
  ret = fcntl(fd, F_GETFD);
  ASSERT_NE(-1, ret);
  // Important: use F_GETFD, not F_GETFL. The O_CLOEXEC flag in F_GETFL
  // is actually not used by the kernel.
  ASSERT_TRUE(FD_CLOEXEC & ret);
  ASSERT_EQ(0, close(fd));

  fd = open_broker.Open(kCpuInfo, O_RDONLY | O_NONBLOCK);
  ASSERT_GE(fd, 0);
  ret = fcntl(fd, F_GETFL);
  ASSERT_NE(-1, ret);
  ASSERT_TRUE(O_NONBLOCK & ret);
  ASSERT_EQ(0, close(fd));
}

TEST(BrokerProcess, OpenComplexFlagsWithClientCheck) {
  TestOpenComplexFlags(true /* fast_check_in_client */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerProcess, OpenComplexFlagsNoClientCheck) {
  TestOpenComplexFlags(false /* fast_check_in_client */);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

#if defined(OS_LINUX)
// Flaky on Linux NG bots: https://crbug.com/595199.
#define MAYBE_RecvMsgDescriptorLeak DISABLED_RecvMsgDescriptorLeak
#else
#define MAYBE_RecvMsgDescriptorLeak RecvMsgDescriptorLeak
#endif

// We need to allow noise because the broker will log when it receives our
// bogus IPCs.
SANDBOX_TEST_ALLOW_NOISE(BrokerProcess, MAYBE_RecvMsgDescriptorLeak) {
  // Android creates a socket on first use of the LOG call.
  // We need to ensure this socket is open before we
  // begin the test.
  LOG(INFO) << "Ensure Android LOG socket is allocated";

  // Find the four lowest available file descriptors.
  int available_fds[4];
  SANDBOX_ASSERT(0 == pipe(available_fds));
  SANDBOX_ASSERT(0 == pipe(available_fds + 2));

  // Save one FD to send to the broker later, and close the others.
  base::ScopedFD message_fd(available_fds[0]);
  for (size_t i = 1; i < arraysize(available_fds); i++) {
    SANDBOX_ASSERT(0 == IGNORE_EINTR(close(available_fds[i])));
  }

  // Lower our file descriptor limit to just allow three more file descriptors
  // to be allocated.  (N.B., RLIMIT_NOFILE doesn't limit the number of file
  // descriptors a process can have: it only limits the highest value that can
  // be assigned to newly-created descriptors allocated by the process.)
  const rlim_t fd_limit =
      1 +
      *std::max_element(available_fds,
                        available_fds + arraysize(available_fds));

  // Valgrind doesn't allow changing the hard descriptor limit, so we only
  // change the soft descriptor limit here.
  struct rlimit rlim;
  SANDBOX_ASSERT(0 == getrlimit(RLIMIT_NOFILE, &rlim));
  SANDBOX_ASSERT(fd_limit <= rlim.rlim_cur);
  rlim.rlim_cur = fd_limit;
  SANDBOX_ASSERT(0 == setrlimit(RLIMIT_NOFILE, &rlim));

  static const char kCpuInfo[] = "/proc/cpuinfo";
  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(BrokerFilePermission::ReadOnly(kCpuInfo));

  BrokerProcess open_broker(EPERM, permissions);
  SANDBOX_ASSERT(open_broker.Init(base::Bind(&NoOpCallback)));

  const int ipc_fd = BrokerProcessTestHelper::GetIPCDescriptor(&open_broker);
  SANDBOX_ASSERT(ipc_fd >= 0);

  static const char kBogus[] = "not a pickle";
  std::vector<int> fds;
  fds.push_back(message_fd.get());

  // The broker process should only have a couple spare file descriptors
  // available, but for good measure we send it fd_limit bogus IPCs anyway.
  for (rlim_t i = 0; i < fd_limit; ++i) {
    SANDBOX_ASSERT(
        base::UnixDomainSocket::SendMsg(ipc_fd, kBogus, sizeof(kBogus), fds));
  }

  const int fd = open_broker.Open(kCpuInfo, O_RDONLY);
  SANDBOX_ASSERT(fd >= 0);
  SANDBOX_ASSERT(0 == IGNORE_EINTR(close(fd)));
}

bool CloseFD(int fd) {
  PCHECK(0 == IGNORE_EINTR(close(fd)));
  return true;
}

// Return true if the other end of the |reader| pipe was closed,
// false if |timeout_in_seconds| was reached or another event
// or error occured.
bool WaitForClosedPipeWriter(int reader, int timeout_in_ms) {
  struct pollfd poll_fd = {reader, POLLIN | POLLRDHUP, 0};
  const int num_events = HANDLE_EINTR(poll(&poll_fd, 1, timeout_in_ms));
  if (1 == num_events && poll_fd.revents | POLLHUP)
    return true;
  return false;
}

// Closing the broker client's IPC channel should terminate the broker
// process.
TEST(BrokerProcess, BrokerDiesOnClosedChannel) {
  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(BrokerFilePermission::ReadOnly("/proc/cpuinfo"));

  // Get the writing end of a pipe into the broker (child) process so
  // that we can reliably detect when it dies.
  int lifeline_fds[2];
  PCHECK(0 == pipe(lifeline_fds));

  BrokerProcess open_broker(EPERM, permissions, true /* fast_check_in_client */,
                            false /* quiet_failures_for_tests */);
  ASSERT_TRUE(open_broker.Init(base::Bind(&CloseFD, lifeline_fds[0])));
  // Make sure the writing end only exists in the broker process.
  CloseFD(lifeline_fds[1]);
  base::ScopedFD reader(lifeline_fds[0]);

  const pid_t broker_pid = open_broker.broker_pid();

  // This should cause the broker process to exit.
  BrokerProcessTestHelper::CloseChannel(&open_broker);

  const int kTimeoutInMilliseconds = 5000;
  const bool broker_lifeline_closed =
      WaitForClosedPipeWriter(reader.get(), kTimeoutInMilliseconds);
  // If the broker exited, its lifeline fd should be closed.
  ASSERT_TRUE(broker_lifeline_closed);
  // Now check that the broker has exited, but do not reap it.
  siginfo_t process_info;
  ASSERT_EQ(0, HANDLE_EINTR(waitid(P_PID, broker_pid, &process_info,
                                   WEXITED | WNOWAIT)));
  EXPECT_EQ(broker_pid, process_info.si_pid);
  EXPECT_EQ(CLD_EXITED, process_info.si_code);
  EXPECT_EQ(1, process_info.si_status);
}

TEST(BrokerProcess, CreateFile) {
  std::string temp_str;
  {
    ScopedTemporaryFile tmp_file;
    temp_str = tmp_file.full_file_name();
  }
  const char* tempfile_name = temp_str.c_str();

  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(BrokerFilePermission::ReadWriteCreate(tempfile_name));

  BrokerProcess open_broker(EPERM, permissions);
  ASSERT_TRUE(open_broker.Init(base::Bind(&NoOpCallback)));

  int fd = -1;

  // Try without O_EXCL
  fd = open_broker.Open(tempfile_name, O_RDWR | O_CREAT);
  ASSERT_EQ(fd, -EPERM);

  const char kTestText[] = "TESTTESTTEST";
  // Create a file
  fd = open_broker.Open(tempfile_name, O_RDWR | O_CREAT | O_EXCL);
  ASSERT_GE(fd, 0);
  {
    base::ScopedFD scoped_fd(fd);

    // Confirm fail if file exists
    int bad_fd = open_broker.Open(tempfile_name, O_RDWR | O_CREAT | O_EXCL);
    ASSERT_EQ(bad_fd, -EEXIST);

    // Write to the descriptor opened by the broker.

    ssize_t len = HANDLE_EINTR(write(fd, kTestText, sizeof(kTestText)));
    ASSERT_EQ(len, static_cast<ssize_t>(sizeof(kTestText)));
  }

  int fd_check = open(tempfile_name, O_RDONLY);
  ASSERT_GE(fd_check, 0);
  {
    base::ScopedFD scoped_fd(fd_check);
    char buf[1024];
    ssize_t len = HANDLE_EINTR(read(fd_check, buf, sizeof(buf)));

    ASSERT_EQ(len, static_cast<ssize_t>(sizeof(kTestText)));
    ASSERT_EQ(memcmp(kTestText, buf, sizeof(kTestText)), 0);
  }
}

}  // namespace syscall_broker

}  // namespace sandbox
