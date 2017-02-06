// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_BROKER_PROCESS_H_
#define SANDBOX_LINUX_SERVICES_BROKER_PROCESS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "base/process/process.h"
#include "sandbox/linux/syscall_broker/broker_policy.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

namespace syscall_broker {

class BrokerClient;
class BrokerFilePermission;

// Create a new "broker" process to which we can send requests via an IPC
// channel by forking the current process.
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

  BrokerProcess(
      int denied_errno,
      const std::vector<syscall_broker::BrokerFilePermission>& permissions,
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
  friend class BrokerProcessTestHelper;

  // Close the IPC channel with the other party. This should only be used
  // by tests an none of the class methods should be used afterwards.
  void CloseChannel();

  bool initialized_;  // Whether we've been through Init() yet.
  const bool fast_check_in_client_;
  const bool quiet_failures_for_tests_;
  pid_t broker_pid_;                     // The PID of the broker (child).
  syscall_broker::BrokerPolicy policy_;  // The sandboxing policy.
  std::unique_ptr<syscall_broker::BrokerClient> broker_client_;

  DISALLOW_COPY_AND_ASSIGN(BrokerProcess);
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_BROKER_PROCESS_H_
