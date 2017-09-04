// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_

#include "base/macros.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"

namespace sandbox {

namespace syscall_broker {

class BrokerPolicy;

// The BrokerHost class should be embedded in a (presumably not sandboxed)
// process. It will honor IPC requests from a BrokerClient sent over
// |ipc_channel| according to |broker_policy|.
class BrokerHost {
 public:
  enum class RequestStatus { LOST_CLIENT = 0, SUCCESS, FAILURE };

  BrokerHost(const BrokerPolicy& broker_policy,
             BrokerChannel::EndPoint ipc_channel);
  ~BrokerHost();

  RequestStatus HandleRequest() const;

 private:
  const BrokerPolicy& broker_policy_;
  const BrokerChannel::EndPoint ipc_channel_;

  DISALLOW_COPY_AND_ASSIGN(BrokerHost);
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  //  SANDBOX_LINUX_SYSCALL_BROKER_BROKER_HOST_H_
