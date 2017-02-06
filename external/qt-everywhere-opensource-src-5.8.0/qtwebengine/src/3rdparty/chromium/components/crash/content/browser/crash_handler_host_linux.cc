// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_handler_host_linux.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/linux_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "breakpad/src/client/linux/handler/exception_handler.h"
#include "breakpad/src/client/linux/minidump_writer/linux_dumper.h"
#include "breakpad/src/client/linux/minidump_writer/minidump_writer.h"
#include "build/build_config.h"
#include "components/crash/content/app/breakpad_linux_impl.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_ANDROID) && !defined(__LP64__)
#include <sys/linux-syscalls.h>

#define SYS_read __NR_read
#endif

using content::BrowserThread;
using google_breakpad::ExceptionHandler;

namespace breakpad {

namespace {

const size_t kNumFDs = 1;
// The length of the control message:
const size_t kControlMsgSize =
    CMSG_SPACE(kNumFDs * sizeof(int)) + CMSG_SPACE(sizeof(struct ucred));
// The length of the regular payload:
const size_t kCrashContextSize = sizeof(ExceptionHandler::CrashContext);

// Handles the crash dump and frees the allocated BreakpadInfo struct.
void CrashDumpTask(CrashHandlerHostLinux* handler,
                   std::unique_ptr<BreakpadInfo> info) {
  if (handler->IsShuttingDown() && info->upload) {
    base::DeleteFile(base::FilePath(info->filename), false);
#if defined(ADDRESS_SANITIZER)
    base::DeleteFile(base::FilePath(info->log_filename), false);
#endif
    return;
  }

  HandleCrashDump(*info);
  delete[] info->filename;
#if defined(ADDRESS_SANITIZER)
  delete[] info->log_filename;
  delete[] info->asan_report_str;
#endif
  delete[] info->process_type;
  delete[] info->distro;
  delete info->crash_keys;
}

}  // namespace

// Since instances of CrashHandlerHostLinux are leaked, they are only destroyed
// at the end of the processes lifetime, which is greater in span than the
// lifetime of the IO message loop. Thus, all calls to base::Bind() use
// non-refcounted pointers.

CrashHandlerHostLinux::CrashHandlerHostLinux(const std::string& process_type,
                                             const base::FilePath& dumps_path,
                                             bool upload)
    : process_type_(process_type),
      dumps_path_(dumps_path),
#if !defined(OS_ANDROID)
      upload_(upload),
#endif
      shutting_down_(false),
      worker_pool_token_(base::SequencedWorkerPool::GetSequenceToken()) {
  int fds[2];
  // We use SOCK_SEQPACKET rather than SOCK_DGRAM to prevent the process from
  // sending datagrams to other sockets on the system. The sandbox may prevent
  // the process from calling socket() to create new sockets, but it'll still
  // inherit some sockets. With PF_UNIX+SOCK_DGRAM, it can call sendmsg to send
  // a datagram to any (abstract) socket on the same system. With
  // SOCK_SEQPACKET, this is prevented.
  CHECK_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  static const int on = 1;

  // Enable passcred on the server end of the socket
  CHECK_EQ(0, setsockopt(fds[1], SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)));

  process_socket_ = fds[0];
  browser_socket_ = fds[1];

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CrashHandlerHostLinux::Init, base::Unretained(this)));
}

CrashHandlerHostLinux::~CrashHandlerHostLinux() {
  close(process_socket_);
  close(browser_socket_);
}

void CrashHandlerHostLinux::StartUploaderThread() {
  uploader_thread_.reset(
      new base::Thread(process_type_ + "_crash_uploader"));
  uploader_thread_->Start();
}

void CrashHandlerHostLinux::Init() {
  base::MessageLoopForIO* ml = base::MessageLoopForIO::current();
  CHECK(ml->WatchFileDescriptor(
      browser_socket_, true /* persistent */,
      base::MessageLoopForIO::WATCH_READ,
      &file_descriptor_watcher_, this));
  ml->AddDestructionObserver(this);
}

void CrashHandlerHostLinux::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void CrashHandlerHostLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(browser_socket_, fd);

  // A process has crashed and has signaled us by writing a datagram
  // to the death signal socket. The datagram contains the crash context needed
  // for writing the minidump as well as a file descriptor and a credentials
  // block so that they can't lie about their pid.
  //
  // The message sender is in components/crash/content/app/breakpad_linux.cc.

  struct msghdr msg = {0};
  struct iovec iov[kCrashIovSize];

  std::unique_ptr<char[]> crash_context(new char[kCrashContextSize]);
#if defined(ADDRESS_SANITIZER)
  std::unique_ptr<char[]> asan_report(new char[kMaxAsanReportSize + 1]);
#endif

  std::unique_ptr<CrashKeyStorage> crash_keys(new CrashKeyStorage);
  google_breakpad::SerializedNonAllocatingMap* serialized_crash_keys;
  size_t crash_keys_size = crash_keys->Serialize(
      const_cast<const google_breakpad::SerializedNonAllocatingMap**>(
          &serialized_crash_keys));

  char* tid_buf_addr = NULL;
  int tid_fd = -1;
  uint64_t uptime;
  size_t oom_size;
  char control[kControlMsgSize];
  const ssize_t expected_msg_size =
      kCrashContextSize +
      sizeof(tid_buf_addr) + sizeof(tid_fd) +
      sizeof(uptime) +
#if defined(ADDRESS_SANITIZER)
      kMaxAsanReportSize + 1 +
#endif
      sizeof(oom_size) +
      crash_keys_size;
  iov[0].iov_base = crash_context.get();
  iov[0].iov_len = kCrashContextSize;
  iov[1].iov_base = &tid_buf_addr;
  iov[1].iov_len = sizeof(tid_buf_addr);
  iov[2].iov_base = &tid_fd;
  iov[2].iov_len = sizeof(tid_fd);
  iov[3].iov_base = &uptime;
  iov[3].iov_len = sizeof(uptime);
  iov[4].iov_base = &oom_size;
  iov[4].iov_len = sizeof(oom_size);
  iov[5].iov_base = serialized_crash_keys;
  iov[5].iov_len = crash_keys_size;
#if !defined(ADDRESS_SANITIZER)
  static_assert(5 == kCrashIovSize - 1, "kCrashIovSize should equal 6");
#else
  iov[6].iov_base = asan_report.get();
  iov[6].iov_len = kMaxAsanReportSize + 1;
  static_assert(6 == kCrashIovSize - 1, "kCrashIovSize should equal 7");
#endif
  msg.msg_iov = iov;
  msg.msg_iovlen = kCrashIovSize;
  msg.msg_control = control;
  msg.msg_controllen = kControlMsgSize;

  const ssize_t msg_size = HANDLE_EINTR(recvmsg(browser_socket_, &msg, 0));
  if (msg_size < 0) {
    LOG(ERROR) << "Error reading from death signal socket. Crash dumping"
               << " is disabled."
               << " msg_size:" << msg_size
               << " errno:" << errno;
    file_descriptor_watcher_.StopWatchingFileDescriptor();
    return;
  }
  const bool bad_message = (msg_size != expected_msg_size ||
                            msg.msg_controllen != kControlMsgSize ||
                            msg.msg_flags & ~MSG_TRUNC);
  base::ScopedFD signal_fd;
  pid_t crashing_pid = -1;
  if (msg.msg_controllen > 0) {
    // Walk the control payload and extract the file descriptor and
    // validated pid.
    for (struct cmsghdr *hdr = CMSG_FIRSTHDR(&msg); hdr;
         hdr = CMSG_NXTHDR(&msg, hdr)) {
      if (hdr->cmsg_level != SOL_SOCKET)
        continue;
      if (hdr->cmsg_type == SCM_RIGHTS) {
        const size_t len = hdr->cmsg_len -
            (((uint8_t*)CMSG_DATA(hdr)) - (uint8_t*)hdr);
        DCHECK_EQ(0U, len % sizeof(int));
        const size_t num_fds = len / sizeof(int);
        if (num_fds != kNumFDs) {
          // A nasty process could try and send us too many descriptors and
          // force a leak.
          LOG(ERROR) << "Death signal contained wrong number of descriptors;"
                     << " num_fds:" << num_fds;
          for (size_t i = 0; i < num_fds; ++i)
            close(reinterpret_cast<int*>(CMSG_DATA(hdr))[i]);
          return;
        }
        DCHECK(!signal_fd.is_valid());
        int fd = reinterpret_cast<int*>(CMSG_DATA(hdr))[0];
        DCHECK_GE(fd, 0);  // The kernel should never send a negative fd.
        signal_fd.reset(fd);
      } else if (hdr->cmsg_type == SCM_CREDENTIALS) {
        DCHECK_EQ(-1, crashing_pid);
        const struct ucred *cred =
            reinterpret_cast<struct ucred*>(CMSG_DATA(hdr));
        crashing_pid = cred->pid;
      }
    }
  }

  if (bad_message) {
    LOG(ERROR) << "Received death signal message with the wrong size;"
               << " msg.msg_controllen:" << msg.msg_controllen
               << " msg.msg_flags:" << msg.msg_flags
               << " kCrashContextSize:" << kCrashContextSize
               << " kControlMsgSize:" << kControlMsgSize;
    return;
  }
  if (crashing_pid == -1 || !signal_fd.is_valid()) {
    LOG(ERROR) << "Death signal message didn't contain all expected control"
               << " messages";
    return;
  }

  // The crashing TID set inside the compromised context via
  // sys_gettid() in ExceptionHandler::HandleSignal might be wrong (if
  // the kernel supports PID namespacing) and may need to be
  // translated.
  //
  // We expect the crashing thread to be in sys_read(), waiting for us to
  // write to |signal_fd|. Most newer kernels where we have the different pid
  // namespaces also have /proc/[pid]/syscall, so we can look through
  // |actual_crashing_pid|'s thread group and find the thread that's in the
  // read syscall with the right arguments.

  std::string expected_syscall_data;
  // /proc/[pid]/syscall is formatted as follows:
  // syscall_number arg1 ... arg6 sp pc
  // but we just check syscall_number through arg3.
  base::StringAppendF(&expected_syscall_data, "%d 0x%x %p 0x1 ",
                      SYS_read, tid_fd, tid_buf_addr);
  bool syscall_supported = false;
  pid_t crashing_tid =
      base::FindThreadIDWithSyscall(crashing_pid,
                                    expected_syscall_data,
                                    &syscall_supported);
  if (crashing_tid == -1) {
    // We didn't find the thread we want. Maybe it didn't reach
    // sys_read() yet or the thread went away.  We'll just take a
    // guess here and assume the crashing thread is the thread group
    // leader.  If procfs syscall is not supported by the kernel, then
    // we assume the kernel also does not support TID namespacing and
    // trust the TID passed by the crashing process.
    LOG(WARNING) << "Could not translate tid - assuming crashing thread is "
        "thread group leader; syscall_supported=" << syscall_supported;
    crashing_tid = crashing_pid;
  }

  ExceptionHandler::CrashContext* bad_context =
      reinterpret_cast<ExceptionHandler::CrashContext*>(crash_context.get());
  bad_context->tid = crashing_tid;

  std::unique_ptr<BreakpadInfo> info(new BreakpadInfo);

  info->fd = -1;
  info->process_type_length = process_type_.length();
  // Freed in CrashDumpTask().
  char* process_type_str = new char[info->process_type_length + 1];
  process_type_.copy(process_type_str, info->process_type_length);
  process_type_str[info->process_type_length] = '\0';
  info->process_type = process_type_str;

  // Memory released from std::unique_ptrs below are also freed in
  // CrashDumpTask().
  info->crash_keys = crash_keys.release();
#if defined(ADDRESS_SANITIZER)
  asan_report[kMaxAsanReportSize] = '\0';
  info->asan_report_str = asan_report.release();
  info->asan_report_length = strlen(info->asan_report_str);
#endif

  info->process_start_time = uptime;
  info->oom_size = oom_size;
#if defined(OS_ANDROID)
  // Nothing gets uploaded in android.
  info->upload = false;
#else
  info->upload = upload_;
#endif


  BrowserThread::GetBlockingPool()->PostSequencedWorkerTask(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&CrashHandlerHostLinux::WriteDumpFile,
                 base::Unretained(this),
                 base::Passed(&info),
                 base::Passed(&crash_context),
                 crashing_pid,
                 signal_fd.release()));
}

void CrashHandlerHostLinux::WriteDumpFile(std::unique_ptr<BreakpadInfo> info,
                                          std::unique_ptr<char[]> crash_context,
                                          pid_t crashing_pid,
                                          int signal_fd) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      worker_pool_token_));

  // Set |info->distro| here because base::GetLinuxDistro() needs to run on a
  // blocking thread.
  std::string distro = base::GetLinuxDistro();
  info->distro_length = distro.length();
  // Freed in CrashDumpTask().
  char* distro_str = new char[info->distro_length + 1];
  distro.copy(distro_str, info->distro_length);
  distro_str[info->distro_length] = '\0';
  info->distro = distro_str;

  base::FilePath dumps_path("/tmp");
  PathService::Get(base::DIR_TEMP, &dumps_path);
  if (!info->upload)
    dumps_path = dumps_path_;
  const std::string minidump_filename =
      base::StringPrintf("%s/chromium-%s-minidump-%016" PRIx64 ".dmp",
                         dumps_path.value().c_str(),
                         process_type_.c_str(),
                         base::RandUint64());

  if (!google_breakpad::WriteMinidump(minidump_filename.c_str(),
                                      kMaxMinidumpFileSize,
                                      crashing_pid,
                                      crash_context.get(),
                                      kCrashContextSize,
                                      google_breakpad::MappingList(),
                                      google_breakpad::AppMemoryList())) {
    LOG(ERROR) << "Failed to write crash dump for pid " << crashing_pid;
  }
#if defined(ADDRESS_SANITIZER)
  // Create a temporary file holding the AddressSanitizer report.
  const base::FilePath log_path =
      base::FilePath(minidump_filename).ReplaceExtension("log");
  base::WriteFile(log_path, info->asan_report_str, info->asan_report_length);
#endif

  // Freed in CrashDumpTask().
  char* minidump_filename_str = new char[minidump_filename.length() + 1];
  minidump_filename.copy(minidump_filename_str, minidump_filename.length());
  minidump_filename_str[minidump_filename.length()] = '\0';
  info->filename = minidump_filename_str;
#if defined(ADDRESS_SANITIZER)
  // Freed in CrashDumpTask().
  char* minidump_log_filename_str = new char[minidump_filename.length() + 1];
  minidump_filename.copy(minidump_log_filename_str, minidump_filename.length());
  memcpy(minidump_log_filename_str + minidump_filename.length() - 3, "log", 3);
  minidump_log_filename_str[minidump_filename.length()] = '\0';
  info->log_filename = minidump_log_filename_str;
#endif
  info->pid = crashing_pid;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CrashHandlerHostLinux::QueueCrashDumpTask,
                 base::Unretained(this),
                 base::Passed(&info),
                 signal_fd));
}

void CrashHandlerHostLinux::QueueCrashDumpTask(
    std::unique_ptr<BreakpadInfo> info,
    int signal_fd) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Send the done signal to the process: it can exit now.
  struct msghdr msg = {0};
  struct iovec done_iov;
  done_iov.iov_base = const_cast<char*>("\x42");
  done_iov.iov_len = 1;
  msg.msg_iov = &done_iov;
  msg.msg_iovlen = 1;

  HANDLE_EINTR(sendmsg(signal_fd, &msg, MSG_DONTWAIT | MSG_NOSIGNAL));
  close(signal_fd);

  uploader_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CrashDumpTask, base::Unretained(this), base::Passed(&info)));
}

void CrashHandlerHostLinux::WillDestroyCurrentMessageLoop() {
  file_descriptor_watcher_.StopWatchingFileDescriptor();

  // If we are quitting and there are crash dumps in the queue, turn them into
  // no-ops.
  shutting_down_ = true;
  uploader_thread_->Stop();
}

bool CrashHandlerHostLinux::IsShuttingDown() const {
  return shutting_down_;
}

}  // namespace breakpad
