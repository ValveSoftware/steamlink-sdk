// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_MACH_MESSAGE_SERVER_H_
#define SANDBOX_MAC_MACH_MESSAGE_SERVER_H_

#include <dispatch/dispatch.h>
#include <mach/mach.h>

#include "base/mac/scoped_mach_port.h"
#include "base/mac/scoped_mach_vm.h"

namespace sandbox {

// A delegate interface for MachMessageServer that handles processing of
// incoming intercepted IPC messages.
class MessageDemuxer {
 public:
  // Handle a |request| message and optionally create a |reply|. Both message
  // objects are owned by the server. Use the server's methods to send a
  // reply message.
  virtual void DemuxMessage(mach_msg_header_t* request,
                            mach_msg_header_t* reply) = 0;

 protected:
  virtual ~MessageDemuxer() {}
};

// A Mach message server that operates a receive port. Messages are received
// and then passed to the MessageDemuxer for handling. The Demuxer
// can use the server class to send a reply, forward the message to a
// different port, or reply to the message with a MIG error.
class MachMessageServer {
 public:
  // Creates a new Mach message server that will send messages to |demuxer|
  // for handling. If the |server_receive_right| is non-NULL, this class will
  // take ownership of the port and it will be used to receive messages.
  // Otherwise the server will create a new receive right.
  // The maximum size of messages is specified by |buffer_size|.
  MachMessageServer(MessageDemuxer* demuxer,
                    mach_port_t server_receive_right,
                    mach_msg_size_t buffer_size);
  ~MachMessageServer();

  // Initializes the class and starts running the message server. If this
  // returns false, no other methods may be called on this class.
  bool Initialize();

  // Given a received request message, returns the PID of the sending process.
  pid_t GetMessageSenderPID(mach_msg_header_t* request);

  // Sends a reply message. Returns true if the message was sent successfully.
  bool SendReply(mach_msg_header_t* reply);

  // Forwards the original |request| to the |destination| for handling.
  void ForwardMessage(mach_msg_header_t* request, mach_port_t destination);

  // Replies to the message with the specified |error_code| as a MIG
  // error_reply RetCode.
  void RejectMessage(mach_msg_header_t* reply, int error_code);

  mach_port_t server_port() const { return server_port_.get(); }

 private:
  // Event handler for the |server_source_| that reads a message from the queue
  // and processes it.
  void ReceiveMessage();

  // The demuxer delegate. Weak.
  MessageDemuxer* demuxer_;

  // The Mach port on which the server is receiving requests.
  base::mac::ScopedMachReceiveRight server_port_;

  // The dispatch queue used to service the server_source_.
  dispatch_queue_t server_queue_;

  // A MACH_RECV dispatch source for the server_port_.
  dispatch_source_t server_source_;

  // The size of the two message buffers below.
  const mach_msg_size_t buffer_size_;

  // Request and reply buffers used in ReceiveMessage.
  base::mac::ScopedMachVM request_buffer_;
  base::mac::ScopedMachVM reply_buffer_;

  // Whether or not ForwardMessage() was called during ReceiveMessage().
  bool did_forward_message_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_MACH_MESSAGE_SERVER_H_
