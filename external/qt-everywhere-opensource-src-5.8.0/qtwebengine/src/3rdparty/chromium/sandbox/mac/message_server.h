// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_MESSAGE_SERVER_H_
#define SANDBOX_MAC_MESSAGE_SERVER_H_

#include <mach/mach.h>
#include <unistd.h>

#include "sandbox/mac/xpc.h"

namespace sandbox {

// A message received by a MessageServer. Each concrete implementation of
// that interface will handle the fields of this union appropriately.
// Consumers should treat this as an opaque handle.
union IPCMessage {
  mach_msg_header_t* mach;
  xpc_object_t xpc;
};

// A delegate interface for MessageServer that handles processing of
// incoming intercepted IPC messages.
class MessageDemuxer {
 public:
  // Handle a |request| message. The message is owned by the server. Use the
  // server's methods to create and send a reply message.
  virtual void DemuxMessage(IPCMessage request) = 0;

 protected:
  virtual ~MessageDemuxer() {}
};

// An interaface for an IPC server that implements Mach messaging semantics.
// The concrete implementation may be powered by raw Mach messages, XPC, or
// some other technology. This interface is the abstraction on top of those
// that enables message interception.
class MessageServer {
 public:
  virtual ~MessageServer() {}

  // Initializes the class and starts running the message server. If this
  // returns false, no other methods may be called on this class.
  virtual bool Initialize() = 0;

  // Blocks the calling thread while the server shuts down. This prevents
  // the server from receiving new messages. After this method is called,
  // no other methods may be called on this class.
  virtual void Shutdown() = 0;

  // Given a received request message, returns the PID of the sending process.
  virtual pid_t GetMessageSenderPID(IPCMessage request) = 0;

  // Creates a reply message from a request message. The result is owned by
  // the server.
  virtual IPCMessage CreateReply(IPCMessage request) = 0;

  // Sends a reply message. Returns true if the message was sent successfully.
  virtual bool SendReply(IPCMessage reply) = 0;

  // Forwards the original |request| to the |destination| for handling.
  virtual void ForwardMessage(IPCMessage request, mach_port_t destination) = 0;

  // Replies to the received |request| message by creating a reply and setting
  // the specified |error_code| in a field that is interpreted by the
  // underlying IPC system.
  virtual void RejectMessage(IPCMessage request, int error_code) = 0;

  // Returns the Mach port on which the MessageServer is listening.
  virtual mach_port_t GetServerPort() const = 0;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_MESSAGE_SERVER_H_
