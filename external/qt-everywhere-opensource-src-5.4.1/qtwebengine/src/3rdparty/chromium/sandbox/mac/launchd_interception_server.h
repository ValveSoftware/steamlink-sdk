// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_LAUNCHD_INTERCEPTION_SERVER_H_
#define SANDBOX_MAC_LAUNCHD_INTERCEPTION_SERVER_H_

#include <dispatch/dispatch.h>
#include <mach/mach.h>

#include "base/mac/scoped_mach_port.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/mac/mach_message_server.h"
#include "sandbox/mac/os_compatibility.h"

namespace sandbox {

class BootstrapSandbox;
struct BootstrapSandboxPolicy;

// This class is used to run a Mach IPC message server. This server can
// hold the receive right for a bootstrap_port of a process, and it filters
// a subset of the launchd/bootstrap IPC call set for sandboxing. It permits
// or rejects requests based on the per-process policy specified in the
// BootstrapSandbox.
class LaunchdInterceptionServer : public MessageDemuxer {
 public:
  explicit LaunchdInterceptionServer(const BootstrapSandbox* sandbox);
  virtual ~LaunchdInterceptionServer();

  // Initializes the class and starts running the message server. If the
  // |server_receive_right| is non-NULL, this class will take ownership of
  // the receive right and intercept messages sent to that port.
  bool Initialize(mach_port_t server_receive_right);

  // MessageDemuxer:
  virtual void DemuxMessage(mach_msg_header_t* request,
                            mach_msg_header_t* reply) OVERRIDE;

  mach_port_t server_port() const { return message_server_->server_port(); }

 private:
  // Given a look_up2 request message, this looks up the appropriate sandbox
  // policy for the service name then formulates and sends the reply message.
  void HandleLookUp(mach_msg_header_t* request,
                    mach_msg_header_t* reply,
                    const BootstrapSandboxPolicy* policy);

  // Given a swap_integer request message, this verifies that it is safe, and
  // if so, forwards it on to launchd for servicing. If the request is unsafe,
  // it replies with an error.
  void HandleSwapInteger(mach_msg_header_t* request,
                         mach_msg_header_t* reply);

  // Forwards the original |request| on to real bootstrap server for handling.
  void ForwardMessage(mach_msg_header_t* request);

  // The sandbox for which this message server is running.
  const BootstrapSandbox* sandbox_;

  // The Mach IPC server.
  scoped_ptr<MachMessageServer> message_server_;

  // The Mach port handed out in reply to denied look up requests. All denied
  // requests share the same port, though nothing reads messages from it.
  base::mac::ScopedMachReceiveRight sandbox_port_;
  // The send right for the above |sandbox_port_|, used with
  // MACH_MSG_TYPE_COPY_SEND when handing out references to the dummy port.
  base::mac::ScopedMachSendRight sandbox_send_port_;

  // The compatibility shim that handles differences in message header IDs and
  // request/reply structures between different OS X versions.
  const LaunchdCompatibilityShim compat_shim_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_LAUNCHD_INTERCEPTION_SERVER_H_
