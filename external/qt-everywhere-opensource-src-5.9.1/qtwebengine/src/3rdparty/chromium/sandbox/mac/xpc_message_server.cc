// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/xpc_message_server.h"

#include <bsm/libbsm.h>

#include <string>

#include "base/mac/mach_logging.h"
#include "base/strings/stringprintf.h"
#include "sandbox/mac/xpc.h"

namespace sandbox {

XPCMessageServer::XPCMessageServer(MessageDemuxer* demuxer,
                                   mach_port_t server_receive_right)
    : demuxer_(demuxer),
      server_port_(server_receive_right),
      reply_message_(NULL) {}

XPCMessageServer::~XPCMessageServer() {
}

bool XPCMessageServer::Initialize() {
  // Allocate a port for use as a new server port if one was not passed to the
  // constructor.
  if (!server_port_.is_valid()) {
    mach_port_t port;
    kern_return_t kr;
    if ((kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
            &port)) != KERN_SUCCESS) {
      MACH_LOG(ERROR, kr) << "Failed to allocate new server port.";
      return false;
    }
    server_port_.reset(port);
  }

  std::string label = base::StringPrintf(
      "org.chromium.sandbox.XPCMessageServer.%p", demuxer_);
  dispatch_source_.reset(new base::DispatchSourceMach(
      label.c_str(), server_port_.get(), ^{ ReceiveMessage(); }));
  dispatch_source_->Resume();

  return true;
}

void XPCMessageServer::Shutdown() {
  dispatch_source_.reset();
}

pid_t XPCMessageServer::GetMessageSenderPID(IPCMessage request) {
  audit_token_t token;
  xpc_dictionary_get_audit_token(request.xpc, &token);
  pid_t sender_pid = audit_token_to_pid(token);
  return sender_pid;
}

IPCMessage XPCMessageServer::CreateReply(IPCMessage request) {
  if (!reply_message_)
    reply_message_ = xpc_dictionary_create_reply(request.xpc);

  IPCMessage reply;
  reply.xpc = reply_message_;
  return reply;
}

bool XPCMessageServer::SendReply(IPCMessage reply) {
  if (!reply.xpc)
    return false;

  int rv = xpc_pipe_routine_reply(reply.xpc);
  if (rv) {
    LOG(ERROR) << "Failed to xpc_pipe_routine_reply(): " << rv;
    return false;
  }
  return true;
}

void XPCMessageServer::ForwardMessage(IPCMessage request,
                                      mach_port_t destination) {
  xpc_pipe_t pipe = xpc_pipe_create_from_port(destination, 0);
  int rv = xpc_pipe_routine_forward(pipe, request.xpc);
  if (rv) {
    LOG(ERROR) << "Failed to xpc_pipe_routine_forward(): " << rv;
  }
  xpc_release(pipe);
}

void XPCMessageServer::RejectMessage(IPCMessage request, int error_code) {
  IPCMessage reply = CreateReply(request);
  // The message wasn't expecting a reply, so rejecting means ignoring it.
  if (reply.xpc) {
    xpc_dictionary_set_int64(reply.xpc, "error", error_code);
    SendReply(reply);
  }
}

mach_port_t XPCMessageServer::GetServerPort() const {
  return server_port_.get();
}

void XPCMessageServer::ReceiveMessage() {
  IPCMessage request;
  int rv = xpc_pipe_receive(server_port_.get(), &request.xpc);
  if (rv) {
    LOG(ERROR) << "Failed to xpc_pipe_receive(): " << rv;
    return;
  }

  demuxer_->DemuxMessage(request);

  xpc_release(request.xpc);
  if (reply_message_) {
    xpc_release(reply_message_);
    reply_message_ = NULL;
  }
}

}  // namespace sandbox
