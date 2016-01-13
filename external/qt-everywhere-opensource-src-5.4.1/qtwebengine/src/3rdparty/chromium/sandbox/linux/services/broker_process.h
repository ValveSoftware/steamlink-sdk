// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_BROKER_PROCESS_H_
#define SANDBOX_LINUX_SERVICES_BROKER_PROCESS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/pickle.h"
#include "base/process/process.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// Create a new "broker" process to which we can send requests via an IPC
// channel.
// This is a low level IPC mechanism that is suitable to be called from a
// signal handler.
// A process would typically create a broker process before entering
// sandboxing.
// 1. BrokerProcess open_broker(read_whitelist, write_whitelist);
// 2. CHECK(open_broker.Init(NULL));
// 3. Enable sandbox.
// 4. Use open_broker.Open() to open files.
class SANDBOX_EXPORT BrokerProcess {
 public:
  // |denied_errno| is the error code returned when methods such as Open()
  // or Access() are invoked on a file which is not in the whitelist. EACCESS
  // would be a typical value.
  // |allowed_r_files| and |allowed_w_files| are white lists of files that can
  // be opened later via the Open() API, respectively for reading and writing.
  // A file available read-write should be listed in both.
  // |fast_check_in_client| and |quiet_failures_for_tests| are reserved for
  // unit tests, don't use it.
  explicit BrokerProcess(int denied_errno,
                         const std::vector<std::string>& allowed_r_files,
                         const std::vector<std::string>& allowed_w_files,
                         bool fast_check_in_client = true,
                         bool quiet_failures_for_tests = false);
  ~BrokerProcess();
  // Will initialize the broker process. There should be no threads at this
  // point, since we need to fork().
  // broker_process_init_callback will be called in the new broker process,
  // after fork() returns.
  bool Init(const base::Callback<bool(void)>& broker_process_init_callback);

  // Can be used in place of access(). Will be async signal safe.
  // X_OK will always return an error in practice since the broker process
  // doesn't support execute permissions.
  // It's similar to the access() system call and will return -errno on errors.
  int Access(const char* pathname, int mode) const;
  // Can be used in place of open(). Will be async signal safe.
  // The implementation only supports certain white listed flags and will
  // return -EPERM on other flags.
  // It's similar to the open() system call and will return -errno on errors.
  int Open(const char* pathname, int flags) const;

  int broker_pid() const { return broker_pid_; }

 private:
  enum IPCCommands {
    kCommandInvalid = 0,
    kCommandOpen,
    kCommandAccess,
  };
  int PathAndFlagsSyscall(enum IPCCommands command_type,
                          const char* pathname,
                          int flags) const;
  bool HandleRequest() const;
  bool HandleRemoteCommand(IPCCommands command_type,
                           int reply_ipc,
                           const Pickle& read_pickle,
                           PickleIterator iter) const;

  void AccessFileForIPC(const std::string& requested_filename,
                        int mode,
                        Pickle* write_pickle) const;
  void OpenFileForIPC(const std::string& requested_filename,
                      int flags,
                      Pickle* write_pickle,
                      std::vector<int>* opened_files) const;
  bool GetFileNameIfAllowedToAccess(const char*, int, const char**) const;
  bool GetFileNameIfAllowedToOpen(const char*, int, const char**) const;
  const int denied_errno_;
  bool initialized_;               // Whether we've been through Init() yet.
  bool is_child_;                  // Whether we're the child (broker process).
  bool fast_check_in_client_;      // Whether to forward a request that we know
                                   // will be denied to the broker.
  bool quiet_failures_for_tests_;  // Disable certain error message when
                                   // testing for failures.
  pid_t broker_pid_;               // The PID of the broker (child).
  const std::vector<std::string> allowed_r_files_;  // Files allowed for read.
  const std::vector<std::string> allowed_w_files_;  // Files allowed for write.
  int ipc_socketpair_;  // Our communication channel to parent or child.
  DISALLOW_IMPLICIT_CONSTRUCTORS(BrokerProcess);

  friend class BrokerProcessTestHelper;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_BROKER_PROCESS_H_
