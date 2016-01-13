// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/broker_process.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/process/process_metrics.h"
#include "base/third_party/valgrind/valgrind.h"
#include "build/build_config.h"
#include "sandbox/linux/services/linux_syscalls.h"

#if defined(OS_ANDROID) && !defined(MSG_CMSG_CLOEXEC)
#define MSG_CMSG_CLOEXEC 0x40000000
#endif

namespace {

bool IsRunningOnValgrind() { return RUNNING_ON_VALGRIND; }

// A little open(2) wrapper to handle some oddities for us. In the general case
// make a direct system call since we want to keep in control of the broker
// process' system calls profile to be able to loosely sandbox it.
int sys_open(const char* pathname, int flags) {
  // Always pass a defined |mode| in case flags mistakenly contains O_CREAT.
  const int mode = 0;
  if (IsRunningOnValgrind()) {
    // Valgrind does not support AT_FDCWD, just use libc's open() in this case.
    return open(pathname, flags, mode);
  } else {
    return syscall(__NR_openat, AT_FDCWD, pathname, flags, mode);
  }
}

static const size_t kMaxMessageLength = 4096;

// Some flags are local to the current process and cannot be sent over a Unix
// socket. They need special treatment from the client.
// O_CLOEXEC is tricky because in theory another thread could call execve()
// before special treatment is made on the client, so a client needs to call
// recvmsg(2) with MSG_CMSG_CLOEXEC.
// To make things worse, there are two CLOEXEC related flags, FD_CLOEXEC (see
// F_GETFD in fcntl(2)) and O_CLOEXEC (see F_GETFL in fcntl(2)). O_CLOEXEC
// doesn't affect the semantics on execve(), it's merely a note that the
// descriptor was originally opened with O_CLOEXEC as a flag. And it is sent
// over unix sockets just fine, so a receiver that would (incorrectly) look at
// O_CLOEXEC instead of FD_CLOEXEC may be tricked in thinking that the file
// descriptor will or won't be closed on execve().
static const int kCurrentProcessOpenFlagsMask = O_CLOEXEC;

// Check whether |requested_filename| is in |allowed_file_names|.
// See GetFileNameIfAllowedToOpen() for an explanation of |file_to_open|.
// async signal safe if |file_to_open| is NULL.
// TODO(jln): assert signal safety.
bool GetFileNameInWhitelist(const std::vector<std::string>& allowed_file_names,
                            const char* requested_filename,
                            const char** file_to_open) {
  if (file_to_open && *file_to_open) {
    // Make sure that callers never pass a non-empty string. In case callers
    // wrongly forget to check the return value and look at the string
    // instead, this could catch bugs.
    RAW_LOG(FATAL, "*file_to_open should be NULL");
    return false;
  }

  // Look for |requested_filename| in |allowed_file_names|.
  // We don't use ::find() because it takes a std::string and
  // the conversion allocates memory.
  std::vector<std::string>::const_iterator it;
  for (it = allowed_file_names.begin(); it != allowed_file_names.end(); it++) {
    if (strcmp(requested_filename, it->c_str()) == 0) {
      if (file_to_open)
        *file_to_open = it->c_str();
      return true;
    }
  }
  return false;
}

// We maintain a list of flags that have been reviewed for "sanity" and that
// we're ok to allow in the broker.
// I.e. here is where we wouldn't add O_RESET_FILE_SYSTEM.
bool IsAllowedOpenFlags(int flags) {
  // First, check the access mode.
  const int access_mode = flags & O_ACCMODE;
  if (access_mode != O_RDONLY && access_mode != O_WRONLY &&
      access_mode != O_RDWR) {
    return false;
  }

  // We only support a 2-parameters open, so we forbid O_CREAT.
  if (flags & O_CREAT) {
    return false;
  }

  // Some flags affect the behavior of the current process. We don't support
  // them and don't allow them for now.
  if (flags & kCurrentProcessOpenFlagsMask)
    return false;

  // Now check that all the flags are known to us.
  const int creation_and_status_flags = flags & ~O_ACCMODE;

  const int known_flags =
    O_APPEND | O_ASYNC | O_CLOEXEC | O_CREAT | O_DIRECT |
    O_DIRECTORY | O_EXCL | O_LARGEFILE | O_NOATIME | O_NOCTTY |
    O_NOFOLLOW | O_NONBLOCK | O_NDELAY | O_SYNC | O_TRUNC;

  const int unknown_flags = ~known_flags;
  const bool has_unknown_flags = creation_and_status_flags & unknown_flags;
  return !has_unknown_flags;
}

}  // namespace

namespace sandbox {

BrokerProcess::BrokerProcess(int denied_errno,
                             const std::vector<std::string>& allowed_r_files,
                             const std::vector<std::string>& allowed_w_files,
                             bool fast_check_in_client,
                             bool quiet_failures_for_tests)
    : denied_errno_(denied_errno),
      initialized_(false),
      is_child_(false),
      fast_check_in_client_(fast_check_in_client),
      quiet_failures_for_tests_(quiet_failures_for_tests),
      broker_pid_(-1),
      allowed_r_files_(allowed_r_files),
      allowed_w_files_(allowed_w_files),
      ipc_socketpair_(-1) {
}

BrokerProcess::~BrokerProcess() {
  if (initialized_ && ipc_socketpair_ != -1) {
    // Closing the socket should be enough to notify the child to die,
    // unless it has been duplicated.
    PCHECK(0 == IGNORE_EINTR(close(ipc_socketpair_)));
    PCHECK(0 == kill(broker_pid_, SIGKILL));
    siginfo_t process_info;
    // Reap the child.
    int ret = HANDLE_EINTR(waitid(P_PID, broker_pid_, &process_info, WEXITED));
    PCHECK(0 == ret);
  }
}

bool BrokerProcess::Init(
    const base::Callback<bool(void)>& broker_process_init_callback) {
  CHECK(!initialized_);
  int socket_pair[2];
  // Use SOCK_SEQPACKET, because we need to preserve message boundaries
  // but we also want to be notified (recvmsg should return and not block)
  // when the connection has been broken (one of the processes died).
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_pair)) {
    LOG(ERROR) << "Failed to create socketpair";
    return false;
  }

#if !defined(THREAD_SANITIZER)
  DCHECK_EQ(1, base::GetNumberOfThreads(base::GetCurrentProcessHandle()));
#endif
  int child_pid = fork();
  if (child_pid == -1) {
    close(socket_pair[0]);
    close(socket_pair[1]);
    return false;
  }
  if (child_pid) {
    // We are the parent and we have just forked our broker process.
    close(socket_pair[0]);
    // We should only be able to write to the IPC channel. We'll always send
    // a new file descriptor to receive the reply on.
    shutdown(socket_pair[1], SHUT_RD);
    ipc_socketpair_ = socket_pair[1];
    is_child_ = false;
    broker_pid_ = child_pid;
    initialized_ = true;
    return true;
  } else {
    // We are the broker.
    close(socket_pair[1]);
    // We should only be able to read from this IPC channel. We will send our
    // replies on a new file descriptor attached to the requests.
    shutdown(socket_pair[0], SHUT_WR);
    ipc_socketpair_ = socket_pair[0];
    is_child_ = true;
    CHECK(broker_process_init_callback.Run());
    initialized_ = true;
    for (;;) {
      HandleRequest();
    }
    _exit(1);
  }
  NOTREACHED();
}

int BrokerProcess::Access(const char* pathname, int mode) const {
  return PathAndFlagsSyscall(kCommandAccess, pathname, mode);
}

int BrokerProcess::Open(const char* pathname, int flags) const {
  return PathAndFlagsSyscall(kCommandOpen, pathname, flags);
}

// Make a remote system call over IPC for syscalls that take a path and flags
// as arguments, currently open() and access().
// Will return -errno like a real system call.
// This function needs to be async signal safe.
int BrokerProcess::PathAndFlagsSyscall(enum IPCCommands syscall_type,
                                       const char* pathname, int flags) const {
  int recvmsg_flags = 0;
  RAW_CHECK(initialized_);  // async signal safe CHECK().
  RAW_CHECK(syscall_type == kCommandOpen || syscall_type == kCommandAccess);
  if (!pathname)
    return -EFAULT;

  // For this "remote system call" to work, we need to handle any flag that
  // cannot be sent over a Unix socket in a special way.
  // See the comments around kCurrentProcessOpenFlagsMask.
  if (syscall_type == kCommandOpen && (flags & kCurrentProcessOpenFlagsMask)) {
    // This implementation only knows about O_CLOEXEC, someone needs to look at
    // this code if other flags are added.
    RAW_CHECK(kCurrentProcessOpenFlagsMask == O_CLOEXEC);
    recvmsg_flags |= MSG_CMSG_CLOEXEC;
    flags &= ~O_CLOEXEC;
  }

  // There is no point in forwarding a request that we know will be denied.
  // Of course, the real security check needs to be on the other side of the
  // IPC.
  if (fast_check_in_client_) {
    if (syscall_type == kCommandOpen &&
        !GetFileNameIfAllowedToOpen(pathname, flags, NULL)) {
      return -denied_errno_;
    }
    if (syscall_type == kCommandAccess &&
        !GetFileNameIfAllowedToAccess(pathname, flags, NULL)) {
      return -denied_errno_;
    }
  }

  Pickle write_pickle;
  write_pickle.WriteInt(syscall_type);
  write_pickle.WriteString(pathname);
  write_pickle.WriteInt(flags);
  RAW_CHECK(write_pickle.size() <= kMaxMessageLength);

  int returned_fd = -1;
  uint8_t reply_buf[kMaxMessageLength];

  // Send a request (in write_pickle) as well that will include a new
  // temporary socketpair (created internally by SendRecvMsg()).
  // Then read the reply on this new socketpair in reply_buf and put an
  // eventual attached file descriptor in |returned_fd|.
  ssize_t msg_len = UnixDomainSocket::SendRecvMsgWithFlags(ipc_socketpair_,
                                                           reply_buf,
                                                           sizeof(reply_buf),
                                                           recvmsg_flags,
                                                           &returned_fd,
                                                           write_pickle);
  if (msg_len <= 0) {
    if (!quiet_failures_for_tests_)
      RAW_LOG(ERROR, "Could not make request to broker process");
    return -ENOMEM;
  }

  Pickle read_pickle(reinterpret_cast<char*>(reply_buf), msg_len);
  PickleIterator iter(read_pickle);
  int return_value = -1;
  // Now deserialize the return value and eventually return the file
  // descriptor.
  if (read_pickle.ReadInt(&iter, &return_value)) {
    switch (syscall_type) {
      case kCommandAccess:
        // We should never have a fd to return.
        RAW_CHECK(returned_fd == -1);
        return return_value;
      case kCommandOpen:
        if (return_value < 0) {
          RAW_CHECK(returned_fd == -1);
          return return_value;
        } else {
          // We have a real file descriptor to return.
          RAW_CHECK(returned_fd >= 0);
          return returned_fd;
        }
      default:
        RAW_LOG(ERROR, "Unsupported command");
        return -ENOSYS;
    }
  } else {
    RAW_LOG(ERROR, "Could not read pickle");
    NOTREACHED();
    return -ENOMEM;
  }
}

// Handle a request on the IPC channel ipc_socketpair_.
// A request should have a file descriptor attached on which we will reply and
// that we will then close.
// A request should start with an int that will be used as the command type.
bool BrokerProcess::HandleRequest() const {
  ScopedVector<base::ScopedFD> fds;
  char buf[kMaxMessageLength];
  errno = 0;
  const ssize_t msg_len = UnixDomainSocket::RecvMsg(ipc_socketpair_, buf,
                                                    sizeof(buf), &fds);

  if (msg_len == 0 || (msg_len == -1 && errno == ECONNRESET)) {
    // EOF from our parent, or our parent died, we should die.
    _exit(0);
  }

  // The parent should send exactly one file descriptor, on which we
  // will write the reply.
  // TODO(mdempsky): ScopedVector doesn't have 'at()', only 'operator[]'.
  if (msg_len < 0 || fds.size() != 1 || fds[0]->get() < 0) {
    PLOG(ERROR) << "Error reading message from the client";
    return false;
  }

  base::ScopedFD temporary_ipc(fds[0]->Pass());

  Pickle pickle(buf, msg_len);
  PickleIterator iter(pickle);
  int command_type;
  if (pickle.ReadInt(&iter, &command_type)) {
    bool r = false;
    // Go through all the possible IPC messages.
    switch (command_type) {
      case kCommandAccess:
      case kCommandOpen:
        // We reply on the file descriptor sent to us via the IPC channel.
        r = HandleRemoteCommand(static_cast<IPCCommands>(command_type),
                                temporary_ipc.get(), pickle, iter);
        break;
      default:
        NOTREACHED();
        r = false;
        break;
    }
    return r;
  }

  LOG(ERROR) << "Error parsing IPC request";
  return false;
}

// Handle a |command_type| request contained in |read_pickle| and send the reply
// on |reply_ipc|.
// Currently kCommandOpen and kCommandAccess are supported.
bool BrokerProcess::HandleRemoteCommand(IPCCommands command_type, int reply_ipc,
                                        const Pickle& read_pickle,
                                        PickleIterator iter) const {
  // Currently all commands have two arguments: filename and flags.
  std::string requested_filename;
  int flags = 0;
  if (!read_pickle.ReadString(&iter, &requested_filename) ||
      !read_pickle.ReadInt(&iter, &flags)) {
    return -1;
  }

  Pickle write_pickle;
  std::vector<int> opened_files;

  switch (command_type) {
    case kCommandAccess:
      AccessFileForIPC(requested_filename, flags, &write_pickle);
      break;
    case kCommandOpen:
      OpenFileForIPC(requested_filename, flags, &write_pickle, &opened_files);
      break;
    default:
      LOG(ERROR) << "Invalid IPC command";
      break;
  }

  CHECK_LE(write_pickle.size(), kMaxMessageLength);
  ssize_t sent = UnixDomainSocket::SendMsg(reply_ipc, write_pickle.data(),
                                           write_pickle.size(), opened_files);

  // Close anything we have opened in this process.
  for (std::vector<int>::iterator it = opened_files.begin();
       it != opened_files.end(); ++it) {
    int ret = IGNORE_EINTR(close(*it));
    DCHECK(!ret) << "Could not close file descriptor";
  }

  if (sent <= 0) {
    LOG(ERROR) << "Could not send IPC reply";
    return false;
  }
  return true;
}

// Perform access(2) on |requested_filename| with mode |mode| if allowed by our
// policy. Write the syscall return value (-errno) to |write_pickle|.
void BrokerProcess::AccessFileForIPC(const std::string& requested_filename,
                                     int mode, Pickle* write_pickle) const {
  DCHECK(write_pickle);
  const char* file_to_access = NULL;
  const bool safe_to_access_file = GetFileNameIfAllowedToAccess(
      requested_filename.c_str(), mode, &file_to_access);

  if (safe_to_access_file) {
    CHECK(file_to_access);
    int access_ret = access(file_to_access, mode);
    int access_errno = errno;
    if (!access_ret)
      write_pickle->WriteInt(0);
    else
      write_pickle->WriteInt(-access_errno);
  } else {
    write_pickle->WriteInt(-denied_errno_);
  }
}

// Open |requested_filename| with |flags| if allowed by our policy.
// Write the syscall return value (-errno) to |write_pickle| and append
// a file descriptor to |opened_files| if relevant.
void BrokerProcess::OpenFileForIPC(const std::string& requested_filename,
                                   int flags, Pickle* write_pickle,
                                   std::vector<int>* opened_files) const {
  DCHECK(write_pickle);
  DCHECK(opened_files);
  const char* file_to_open = NULL;
  const bool safe_to_open_file = GetFileNameIfAllowedToOpen(
      requested_filename.c_str(), flags, &file_to_open);

  if (safe_to_open_file) {
    CHECK(file_to_open);
    int opened_fd = sys_open(file_to_open, flags);
    if (opened_fd < 0) {
      write_pickle->WriteInt(-errno);
    } else {
      // Success.
      opened_files->push_back(opened_fd);
      write_pickle->WriteInt(0);
    }
  } else {
    write_pickle->WriteInt(-denied_errno_);
  }
}


// Check if calling access() should be allowed on |requested_filename| with
// mode |requested_mode|.
// Note: access() being a system call to check permissions, this can get a bit
// confusing. We're checking if calling access() should even be allowed with
// the same policy we would use for open().
// If |file_to_access| is not NULL, we will return the matching pointer from
// the whitelist. For paranoia a caller should then use |file_to_access|. See
// GetFileNameIfAllowedToOpen() fore more explanation.
// return true if calling access() on this file should be allowed, false
// otherwise.
// Async signal safe if and only if |file_to_access| is NULL.
bool BrokerProcess::GetFileNameIfAllowedToAccess(const char* requested_filename,
    int requested_mode, const char** file_to_access) const {
  // First, check if |requested_mode| is existence, ability to read or ability
  // to write. We do not support X_OK.
  if (requested_mode != F_OK &&
      requested_mode & ~(R_OK | W_OK)) {
    return false;
  }
  switch (requested_mode) {
    case F_OK:
      // We allow to check for file existence if we can either read or write.
      return GetFileNameInWhitelist(allowed_r_files_, requested_filename,
                                    file_to_access) ||
             GetFileNameInWhitelist(allowed_w_files_, requested_filename,
                                    file_to_access);
    case R_OK:
      return GetFileNameInWhitelist(allowed_r_files_, requested_filename,
                                    file_to_access);
    case W_OK:
      return GetFileNameInWhitelist(allowed_w_files_, requested_filename,
                                    file_to_access);
    case R_OK | W_OK:
    {
      bool allowed_for_read_and_write =
          GetFileNameInWhitelist(allowed_r_files_, requested_filename, NULL) &&
          GetFileNameInWhitelist(allowed_w_files_, requested_filename,
                                 file_to_access);
      return allowed_for_read_and_write;
    }
    default:
      return false;
  }
}

// Check if |requested_filename| can be opened with flags |requested_flags|.
// If |file_to_open| is not NULL, we will return the matching pointer from the
// whitelist. For paranoia, a caller should then use |file_to_open| rather
// than |requested_filename|, so that it never attempts to open an
// attacker-controlled file name, even if an attacker managed to fool the
// string comparison mechanism.
// Return true if opening should be allowed, false otherwise.
// Async signal safe if and only if |file_to_open| is NULL.
bool BrokerProcess::GetFileNameIfAllowedToOpen(const char* requested_filename,
    int requested_flags, const char** file_to_open) const {
  if (!IsAllowedOpenFlags(requested_flags)) {
    return false;
  }
  switch (requested_flags & O_ACCMODE) {
    case O_RDONLY:
      return GetFileNameInWhitelist(allowed_r_files_, requested_filename,
                                    file_to_open);
    case O_WRONLY:
      return GetFileNameInWhitelist(allowed_w_files_, requested_filename,
                                    file_to_open);
    case O_RDWR:
    {
      bool allowed_for_read_and_write =
          GetFileNameInWhitelist(allowed_r_files_, requested_filename, NULL) &&
          GetFileNameInWhitelist(allowed_w_files_, requested_filename,
                                 file_to_open);
      return allowed_for_read_and_write;
    }
    default:
      return false;
  }
}

}  // namespace sandbox.
