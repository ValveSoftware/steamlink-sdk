// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_XPC_MESSAGE_SERVER_H_
#define SANDBOX_MAC_XPC_MESSAGE_SERVER_H_

#include <AvailabilityMacros.h>

#include <memory>

#include "base/mac/dispatch_source_mach.h"
#include "base/mac/scoped_mach_port.h"
#include "base/macros.h"
#include "sandbox/mac/message_server.h"
#include "sandbox/mac/xpc.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// An implementation of MessageServer that uses XPC pipes to read and write XPC
// messages from a Mach port.
class SANDBOX_EXPORT XPCMessageServer : public MessageServer {
 public:
  // Creates a new XPC message server that will send messages to |demuxer|
  // for handling. If the |server_receive_right| is non-NULL, this class will
  // take ownership of the port and it will be used to receive messages.
  // Otherwise the server will create a new receive right on which to listen.
  XPCMessageServer(MessageDemuxer* demuxer,
                   mach_port_t server_receive_right);
  ~XPCMessageServer() override;

  // MessageServer:
  bool Initialize() override;
  void Shutdown() override;
  pid_t GetMessageSenderPID(IPCMessage request) override;
  IPCMessage CreateReply(IPCMessage request) override;
  bool SendReply(IPCMessage reply) override;
  void ForwardMessage(IPCMessage request, mach_port_t destination) override;
  // Creates an error reply message with a field "error" set to |error_code|.
  void RejectMessage(IPCMessage request, int error_code) override;
  mach_port_t GetServerPort() const override;

 private:
  // Reads a message from the XPC pipe.
  void ReceiveMessage();

  // The demuxer delegate. Weak.
  MessageDemuxer* demuxer_;

  // The Mach port on which the server is receiving requests.
  base::mac::ScopedMachReceiveRight server_port_;

  // MACH_RECV dispatch source that handles the |server_port_|.
  std::unique_ptr<base::DispatchSourceMach> dispatch_source_;

  // The reply message, if one has been created.
  xpc_object_t reply_message_;

  DISALLOW_COPY_AND_ASSIGN(XPCMessageServer);
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_XPC_MESSAGE_SERVER_H_
