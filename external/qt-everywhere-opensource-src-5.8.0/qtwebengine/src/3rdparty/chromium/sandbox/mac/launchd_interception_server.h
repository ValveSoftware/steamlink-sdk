// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_LAUNCHD_INTERCEPTION_SERVER_H_
#define SANDBOX_MAC_LAUNCHD_INTERCEPTION_SERVER_H_

#include <memory>

#include <dispatch/dispatch.h>

#include "base/mac/scoped_mach_port.h"
#include "sandbox/mac/message_server.h"

namespace sandbox {

class BootstrapSandbox;
struct BootstrapSandboxPolicy;
class OSCompatibility;

// This class is used to run a Mach IPC message server. This server can
// hold the receive right for a bootstrap_port of a process, and it filters
// a subset of the launchd/bootstrap IPC call set for sandboxing. It permits
// or rejects requests based on the per-process policy specified in the
// BootstrapSandbox.
class LaunchdInterceptionServer : public MessageDemuxer {
 public:
  explicit LaunchdInterceptionServer(const BootstrapSandbox* sandbox);
  ~LaunchdInterceptionServer() override;

  // Initializes the class and starts running the message server. If the
  // |server_receive_right| is non-NULL, this class will take ownership of
  // the receive right and intercept messages sent to that port.
  bool Initialize(mach_port_t server_receive_right);

  // MessageDemuxer:
  void DemuxMessage(IPCMessage request) override;

  mach_port_t server_port() const { return message_server_->GetServerPort(); }

 private:
  // Given a look_up2 request message, this looks up the appropriate sandbox
  // policy for the service name then formulates and sends the reply message.
  void HandleLookUp(IPCMessage request, const BootstrapSandboxPolicy* policy);

  // Given a swap_integer request message, this verifies that it is safe, and
  // if so, forwards it on to launchd for servicing. If the request is unsafe,
  // it replies with an error.
  void HandleSwapInteger(IPCMessage request);

  // Forwards the original |request| on to real bootstrap server for handling.
  void ForwardMessage(IPCMessage request);

  // The sandbox for which this message server is running.
  const BootstrapSandbox* sandbox_;

  // The Mach IPC server.
  std::unique_ptr<MessageServer> message_server_;

  // Whether or not the system is using an XPC-based launchd.
  bool xpc_launchd_;

  // The Mach port handed out in reply to denied look up requests. All denied
  // requests share the same port, though nothing reads messages from it.
  base::mac::ScopedMachReceiveRight sandbox_port_;
  // The send right for the above |sandbox_port_|, used with
  // MACH_MSG_TYPE_COPY_SEND when handing out references to the dummy port.
  base::mac::ScopedMachSendRight sandbox_send_port_;

  // The compatibility shim that handles differences in message header IDs and
  // request/reply structures between different OS X versions.
  std::unique_ptr<OSCompatibility> compat_shim_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_LAUNCHD_INTERCEPTION_SERVER_H_
