// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CLIENT_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CLIENT_H_

#include "base/macros.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_common.h"

namespace sandbox {

namespace syscall_broker {

class BrokerPolicy;

// This class can be embedded in a sandboxed process and can be
// used to perform certain system calls in another, presumably
// non-sandboxed process (which embeds BrokerHost).
// A key feature of this class is the ability to use some of its methods in a
// thread-safe and async-signal safe way. The goal is to be able to use it to
// replace the open() or access() system calls happening anywhere in a process
// (as allowed for instance by seccomp-bpf's SIGSYS mechanism).
class BrokerClient {
 public:
  // |policy| needs to match the policy used by BrokerHost. This
  // allows to predict some of the requests which will be denied
  // and save an IPC round trip.
  // |ipc_channel| needs to be a suitable SOCK_SEQPACKET unix socket.
  // |fast_check_in_client| should be set to true and
  // |quiet_failures_for_tests| to false unless you are writing tests.
  BrokerClient(const BrokerPolicy& policy,
               BrokerChannel::EndPoint ipc_channel,
               bool fast_check_in_client,
               bool quiet_failures_for_tests);
  ~BrokerClient();

  // Can be used in place of access().
  // X_OK will always return an error in practice since the broker process
  // doesn't support execute permissions.
  // It's similar to the access() system call and will return -errno on errors.
  // This is async signal safe.
  int Access(const char* pathname, int mode) const;
  // Can be used in place of open().
  // The implementation only supports certain white listed flags and will
  // return -EPERM on other flags.
  // It's similar to the open() system call and will return -errno on errors.
  // This is async signal safe.
  int Open(const char* pathname, int flags) const;

  // Get the file descriptor used for IPC. This is used for tests.
  int GetIPCDescriptor() const { return ipc_channel_.get(); }

 private:
  const BrokerPolicy& broker_policy_;
  const BrokerChannel::EndPoint ipc_channel_;
  const bool fast_check_in_client_;  // Whether to forward a request that we
                                     // know will be denied to the broker. (Used
                                     // for tests).
  const bool quiet_failures_for_tests_;  // Disable certain error message when
                                         // testing for failures.

  int PathAndFlagsSyscall(IPCCommand syscall_type,
                          const char* pathname,
                          int flags) const;

  DISALLOW_COPY_AND_ASSIGN(BrokerClient);
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  //  SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CLIENT_H_
