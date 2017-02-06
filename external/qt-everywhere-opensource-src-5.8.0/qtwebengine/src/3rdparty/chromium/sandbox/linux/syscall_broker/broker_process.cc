// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_process.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_client.h"
#include "sandbox/linux/syscall_broker/broker_host.h"

namespace sandbox {

namespace syscall_broker {

BrokerProcess::BrokerProcess(
    int denied_errno,
    const std::vector<syscall_broker::BrokerFilePermission>& permissions,
    bool fast_check_in_client,
    bool quiet_failures_for_tests)
    : initialized_(false),
      fast_check_in_client_(fast_check_in_client),
      quiet_failures_for_tests_(quiet_failures_for_tests),
      broker_pid_(-1),
      policy_(denied_errno, permissions) {
}

BrokerProcess::~BrokerProcess() {
  if (initialized_) {
    if (broker_client_.get()) {
      // Closing the socket should be enough to notify the child to die,
      // unless it has been duplicated.
      CloseChannel();
    }
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
  BrokerChannel::EndPoint ipc_reader;
  BrokerChannel::EndPoint ipc_writer;
  BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

#if !defined(THREAD_SANITIZER)
  DCHECK_EQ(1, base::GetNumberOfThreads(base::GetCurrentProcessHandle()));
#endif
  int child_pid = fork();
  if (child_pid == -1) {
    return false;
  }
  if (child_pid) {
    // We are the parent and we have just forked our broker process.
    ipc_reader.reset();
    broker_pid_ = child_pid;
    broker_client_.reset(new BrokerClient(policy_, std::move(ipc_writer),
                                          fast_check_in_client_,
                                          quiet_failures_for_tests_));
    initialized_ = true;
    return true;
  } else {
    // We are the broker process. Make sure to close the writer's end so that
    // we get notified if the client disappears.
    ipc_writer.reset();
    CHECK(broker_process_init_callback.Run());
    BrokerHost broker_host(policy_, std::move(ipc_reader));
    for (;;) {
      switch (broker_host.HandleRequest()) {
        case BrokerHost::RequestStatus::LOST_CLIENT:
          _exit(1);
        case BrokerHost::RequestStatus::SUCCESS:
        case BrokerHost::RequestStatus::FAILURE:
          continue;
      }
    }
    _exit(1);
  }
  NOTREACHED();
  return false;
}

void BrokerProcess::CloseChannel() {
  broker_client_.reset();
}

int BrokerProcess::Access(const char* pathname, int mode) const {
  RAW_CHECK(initialized_);
  return broker_client_->Access(pathname, mode);
}

int BrokerProcess::Open(const char* pathname, int flags) const {
  RAW_CHECK(initialized_);
  return broker_client_->Open(pathname, flags);
}

}  // namespace syscall_broker

}  // namespace sandbox.
