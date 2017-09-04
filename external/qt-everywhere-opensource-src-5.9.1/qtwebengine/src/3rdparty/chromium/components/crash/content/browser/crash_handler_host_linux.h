// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_HANDLER_HOST_LINUX_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_HANDLER_HOST_LINUX_H_

#include <sys/types.h>

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/sequence_checker.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/crash/content/app/breakpad_linux_impl.h"

namespace base {
class Thread;
}

namespace breakpad {

struct BreakpadInfo;

// This is the host for processes which run breakpad inside the sandbox on
// Linux or Android. We perform the crash dump from the browser because it
// allows us to be outside the sandbox.
//
// Processes signal that they need to be dumped by sending a datagram over a
// UNIX domain socket. All processes of the same type share the client end of
// this socket which is installed in their descriptor table before exec.
class CrashHandlerHostLinux : public base::MessageLoopForIO::Watcher,
                              public base::MessageLoop::DestructionObserver {
 public:
  CrashHandlerHostLinux(const std::string& process_type,
                        const base::FilePath& dumps_path,
                        bool upload);
  ~CrashHandlerHostLinux() override;

  // Starts the uploader thread. Must be called immediately after creating the
  // class.
  void StartUploaderThread();

  // Get the file descriptor which processes should be given in order to signal
  // crashes to the browser.
  int GetDeathSignalSocket() const {
    return process_socket_;
  }

  // MessagePumbLibevent::Watcher impl:
  void OnFileCanWriteWithoutBlocking(int fd) override;
  void OnFileCanReadWithoutBlocking(int fd) override;

  // MessageLoop::DestructionObserver impl:
  void WillDestroyCurrentMessageLoop() override;

  // Whether we are shutting down or not.
  bool IsShuttingDown() const;

 private:
  void Init();

  // Do work in a sequenced worker pool for OnFileCanReadWithoutBlocking().
  void WriteDumpFile(std::unique_ptr<BreakpadInfo> info,
                     std::unique_ptr<char[]> crash_context,
                     pid_t crashing_pid,
                     int signal_fd);

  // Continue OnFileCanReadWithoutBlocking()'s work on the IO thread.
  void QueueCrashDumpTask(std::unique_ptr<BreakpadInfo> info, int signal_fd);

  // Find crashing thread (may delay and retry) and dump on IPC thread.
  void FindCrashingThreadAndDump(pid_t crashing_pid,
                                 const std::string& expected_syscall_data,
                                 std::unique_ptr<char[]> crash_context,
                                 std::unique_ptr<CrashKeyStorage> crash_keys,
#if defined(ADDRESS_SANITIZER)
                                 std::unique_ptr<char[]> asan_report,
#endif
                                 uint64_t uptime,
                                 size_t oom_size,
                                 int signal_fd,
                                 int attempt);

  std::string process_type_;
  base::FilePath dumps_path_;
#if !defined(OS_ANDROID)
  bool upload_;
#endif

  int process_socket_;
  int browser_socket_;

  base::MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;
  std::unique_ptr<base::Thread> uploader_thread_;
  bool shutting_down_;

  // Unique sequence token so that writing crash dump won't be blocked
  // by other tasks.
  base::SequencedWorkerPool::SequenceToken worker_pool_token_;

  // Used to verify that calls to WriteDumpFile() are sequenced.
  base::SequenceChecker write_dump_file_sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(CrashHandlerHostLinux);
};

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_HANDLER_HOST_LINUX_H_
