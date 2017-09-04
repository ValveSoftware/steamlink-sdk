// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_MACH_MESSAGE_SERVER_H_
#define SANDBOX_MAC_MACH_MESSAGE_SERVER_H_

#include <mach/mach.h>
#include <stddef.h>

#include <memory>

#include "base/mac/dispatch_source_mach.h"
#include "base/mac/scoped_mach_port.h"
#include "base/mac/scoped_mach_vm.h"
#include "sandbox/mac/message_server.h"

namespace sandbox {

// A Mach message server that operates a receive port. Messages are received
// and then passed to the MessageDemuxer for handling. The Demuxer
// can use the server class to send a reply, forward the message to a
// different port, or reply to the message with a MIG error.
class MachMessageServer : public MessageServer {
 public:
  // Creates a new Mach message server that will send messages to |demuxer|
  // for handling. If the |server_receive_right| is non-NULL, this class will
  // take ownership of the port and it will be used to receive messages.
  // Otherwise the server will create a new receive right.
  // The maximum size of messages is specified by |buffer_size|.
  MachMessageServer(MessageDemuxer* demuxer,
                    mach_port_t server_receive_right,
                    mach_msg_size_t buffer_size);
  ~MachMessageServer() override;

  // MessageServer:
  bool Initialize() override;
  void Shutdown() override;
  pid_t GetMessageSenderPID(IPCMessage request) override;
  IPCMessage CreateReply(IPCMessage request) override;
  bool SendReply(IPCMessage reply) override;
  void ForwardMessage(IPCMessage request, mach_port_t destination) override;
  // Replies to the message with the specified |error_code| as a MIG
  // error_reply RetCode.
  void RejectMessage(IPCMessage request, int error_code) override;
  mach_port_t GetServerPort() const override;

 private:
  // Event handler for the |server_source_| that reads a message from the queue
  // and processes it.
  void ReceiveMessage();

  // The demuxer delegate. Weak.
  MessageDemuxer* demuxer_;

  // The Mach port on which the server is receiving requests.
  base::mac::ScopedMachReceiveRight server_port_;

  // The size of the two message buffers below.
  const mach_msg_size_t buffer_size_;

  // Request and reply buffers used in ReceiveMessage.
  base::mac::ScopedMachVM request_buffer_;
  base::mac::ScopedMachVM reply_buffer_;

  // MACH_RECV dispatch source that handles the |server_port_|.
  std::unique_ptr<base::DispatchSourceMach> dispatch_source_;

  // Whether or not ForwardMessage() was called during ReceiveMessage().
  bool did_forward_message_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_MACH_MESSAGE_SERVER_H_
