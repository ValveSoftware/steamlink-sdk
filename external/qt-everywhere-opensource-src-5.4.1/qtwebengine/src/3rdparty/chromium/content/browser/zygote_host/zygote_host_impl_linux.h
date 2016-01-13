// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ZYGOTE_HOST_ZYGOTE_HOST_IMPL_LINUX_H_
#define CONTENT_BROWSER_ZYGOTE_HOST_ZYGOTE_HOST_IMPL_LINUX_H_

#include <set>
#include <string>
#include <vector>

#include "base/pickle.h"
#include "base/process/kill.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/file_descriptor_info.h"
#include "content/public/browser/zygote_host_linux.h"

template<typename Type>
struct DefaultSingletonTraits;

namespace content {

class CONTENT_EXPORT ZygoteHostImpl : public ZygoteHost {
 public:
  // Returns the singleton instance.
  static ZygoteHostImpl* GetInstance();

  void Init(const std::string& sandbox_cmd);

  // After the last known Zygote child exits, notify the Zygote to exit.
  void TearDownAfterLastChild();

  // Tries to start a process of type indicated by process_type.
  // Returns its pid on success, otherwise
  // base::kNullProcessHandle;
  pid_t ForkRequest(const std::vector<std::string>& command_line,
                    const std::vector<FileDescriptorInfo>& mapping,
                    const std::string& process_type);
  void EnsureProcessTerminated(pid_t process);

  // Get the termination status (and, optionally, the exit code) of
  // the process. |exit_code| is set to the exit code of the child
  // process. (|exit_code| may be NULL.)
  // Unfortunately the Zygote can not accurately figure out if a process
  // is already dead without waiting synchronously for it.
  // |known_dead| should be set to true when we already know that the process
  // is dead. When |known_dead| is false, processes could be seen as
  // still running, even when they're not. When |known_dead| is true, the
  // process will be SIGKILL-ed first (which should have no effect if it was
  // really dead). This is to prevent a waiting waitpid() from blocking in
  // a single-threaded Zygote. See crbug.com/157458.
  base::TerminationStatus GetTerminationStatus(base::ProcessHandle handle,
                                               bool known_dead,
                                               int* exit_code);

  // ZygoteHost implementation:
  virtual pid_t GetPid() const OVERRIDE;
  virtual int GetSandboxStatus() const OVERRIDE;
  virtual void AdjustRendererOOMScore(base::ProcessHandle process_handle,
                                      int score) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<ZygoteHostImpl>;

  ZygoteHostImpl();
  virtual ~ZygoteHostImpl();

  // Notify the Zygote to exit immediately. This object should not be
  // used afterwards.
  void TearDown();

  // Should be called every time a Zygote child is born.
  void ZygoteChildBorn(pid_t process);

  // Should be called every time a Zygote child died.
  void ZygoteChildDied(pid_t process);

  // Sends |data| to the zygote via |control_fd_|.  If |fds| is non-NULL, the
  // included file descriptors will also be passed.  The caller is responsible
  // for acquiring |control_lock_|.
  bool SendMessage(const Pickle& data, const std::vector<int>* fds);

  ssize_t ReadReply(void* buf, size_t buflen);

  int control_fd_;  // the socket to the zygote
  // A lock protecting all communication with the zygote. This lock must be
  // acquired before sending a command and released after the result has been
  // received.
  base::Lock control_lock_;
  pid_t pid_;
  bool init_;
  bool using_suid_sandbox_;
  std::string sandbox_binary_;
  bool have_read_sandbox_status_word_;
  int sandbox_status_;
  // A lock protecting list_of_running_zygote_children_ and
  // should_teardown_after_last_child_exits_.
  base::Lock child_tracking_lock_;
  std::set<pid_t> list_of_running_zygote_children_;
  bool should_teardown_after_last_child_exits_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ZYGOTE_HOST_ZYGOTE_HOST_IMPL_LINUX_H_
