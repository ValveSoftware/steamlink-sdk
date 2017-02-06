// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/mach_message_server.h"

#include <bsm/libbsm.h>
#include <servers/bootstrap.h>
#include <stddef.h>

#include <string>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/strings/stringprintf.h"

namespace sandbox {

MachMessageServer::MachMessageServer(
    MessageDemuxer* demuxer,
    mach_port_t server_receive_right,
    mach_msg_size_t buffer_size)
    : demuxer_(demuxer),
      server_port_(server_receive_right),
      buffer_size_(
          mach_vm_round_page(buffer_size + sizeof(mach_msg_audit_trailer_t))),
      did_forward_message_(false) {
  DCHECK(demuxer_);
}

MachMessageServer::~MachMessageServer() {
}

bool MachMessageServer::Initialize() {
  mach_port_t task = mach_task_self();
  kern_return_t kr;

  // Allocate a port for use as a new server port if one was not passed to the
  // constructor.
  if (!server_port_.is_valid()) {
    mach_port_t port;
    if ((kr = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &port)) !=
            KERN_SUCCESS) {
      MACH_LOG(ERROR, kr) << "Failed to allocate new server port.";
      return false;
    }
    server_port_.reset(port);
  }

  // Allocate the message request and reply buffers.
  const int kMachMsgMemoryFlags = VM_MAKE_TAG(VM_MEMORY_MACH_MSG) |
                                  VM_FLAGS_ANYWHERE;
  vm_address_t buffer = 0;

  kr = vm_allocate(task, &buffer, buffer_size_, kMachMsgMemoryFlags);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "Failed to allocate request buffer.";
    return false;
  }
  request_buffer_.reset(buffer, buffer_size_);

  kr = vm_allocate(task, &buffer, buffer_size_, kMachMsgMemoryFlags);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "Failed to allocate reply buffer.";
    return false;
  }
  reply_buffer_.reset(buffer, buffer_size_);

  // Set up the dispatch queue to service the bootstrap port.
  std::string label = base::StringPrintf(
      "org.chromium.sandbox.MachMessageServer.%p", demuxer_);
  dispatch_source_.reset(new base::DispatchSourceMach(
      label.c_str(), server_port_.get(), ^{ ReceiveMessage(); }));
  dispatch_source_->Resume();

  return true;
}

void MachMessageServer::Shutdown() {
  dispatch_source_.reset();
}

pid_t MachMessageServer::GetMessageSenderPID(IPCMessage request) {
  // Get the PID of the task that sent this request. This requires getting at
  // the trailer of the message, from the header.
  mach_msg_audit_trailer_t* trailer =
      reinterpret_cast<mach_msg_audit_trailer_t*>(
          reinterpret_cast<vm_address_t>(request.mach) +
              round_msg(request.mach->msgh_size));
  // TODO(rsesek): In the 10.7 SDK, there's audit_token_to_pid().
  pid_t sender_pid;
  audit_token_to_au32(trailer->msgh_audit,
      NULL, NULL, NULL, NULL, NULL, &sender_pid, NULL, NULL);
  return sender_pid;
}

IPCMessage MachMessageServer::CreateReply(IPCMessage request_message) {
  mach_msg_header_t* request = request_message.mach;

  IPCMessage reply_message;
  mach_msg_header_t* reply = reply_message.mach =
      reinterpret_cast<mach_msg_header_t*>(reply_buffer_.address());
  bzero(reply, buffer_size_);

  reply->msgh_bits = MACH_MSGH_BITS_REMOTE(reply->msgh_bits);
  // Since mach_msg will automatically swap the request and reply ports,
  // undo that.
  reply->msgh_remote_port = request->msgh_remote_port;
  reply->msgh_local_port = MACH_PORT_NULL;
  // MIG servers simply add 100 to the request ID to generate the reply ID.
  reply->msgh_id = request->msgh_id + 100;

  return reply_message;
}

bool MachMessageServer::SendReply(IPCMessage reply) {
  kern_return_t kr = mach_msg(reply.mach, MACH_SEND_MSG,
      reply.mach->msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE,
      MACH_PORT_NULL);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "Unable to send intercepted reply message.";
  return kr == KERN_SUCCESS;
}

void MachMessageServer::ForwardMessage(IPCMessage message,
                                       mach_port_t destination) {
  mach_msg_header_t* request = message.mach;
  request->msgh_local_port = request->msgh_remote_port;
  request->msgh_remote_port = destination;
  // Preserve the msgh_bits that do not deal with the local and remote ports.
  request->msgh_bits = (request->msgh_bits & ~MACH_MSGH_BITS_PORTS_MASK) |
      MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MOVE_SEND_ONCE);
  kern_return_t kr = mach_msg_send(request);
  if (kr == KERN_SUCCESS) {
    did_forward_message_ = true;
  } else {
    MACH_LOG(ERROR, kr) << "Unable to forward message to the real launchd.";
  }
}

void MachMessageServer::RejectMessage(IPCMessage request, int error_code) {
  IPCMessage reply = CreateReply(request);
  mig_reply_error_t* error_reply =
      reinterpret_cast<mig_reply_error_t*>(reply.mach);
  error_reply->Head.msgh_size = sizeof(mig_reply_error_t);
  error_reply->Head.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND_ONCE);
  error_reply->NDR = NDR_record;
  error_reply->RetCode = error_code;
  SendReply(reply);
}

mach_port_t MachMessageServer::GetServerPort() const {
  return server_port_.get();
}

void MachMessageServer::ReceiveMessage() {
  const mach_msg_options_t kRcvOptions = MACH_RCV_MSG |
      MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
      MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);

  mach_msg_header_t* request =
      reinterpret_cast<mach_msg_header_t*>(request_buffer_.address());
  mach_msg_header_t* reply =
      reinterpret_cast<mach_msg_header_t*>(reply_buffer_.address());

  // Zero out the buffers from handling any previous message.
  bzero(request, buffer_size_);
  bzero(reply, buffer_size_);
  did_forward_message_ = false;

  // A Mach message server-once. The system library to run a message server
  // cannot be used here, because some requests are conditionally forwarded
  // to another server.
  kern_return_t kr = mach_msg(request, kRcvOptions, 0, buffer_size_,
      server_port_.get(), MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "Unable to receive message.";
    return;
  }

  // Process the message.
  IPCMessage request_message = { request };
  demuxer_->DemuxMessage(request_message);

  // Free any descriptors in the message body. If the message was forwarded,
  // any descriptors would have been moved out of the process on send. If the
  // forwarded message was sent from the process hosting this sandbox server,
  // destroying the message could also destroy rights held outside the scope of
  // this message server.
  if (!did_forward_message_) {
    mach_msg_destroy(request);
    mach_msg_destroy(reply);
  }
}

}  // namespace sandbox
