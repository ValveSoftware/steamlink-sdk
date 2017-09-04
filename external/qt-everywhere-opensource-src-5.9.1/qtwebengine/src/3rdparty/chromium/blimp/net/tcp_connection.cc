// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/tcp_connection.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/logging.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_message_pump.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/message_port.h"
#include "blimp/net/packet_writer.h"
#include "net/base/completion_callback.h"

namespace blimp {

// Forwards incoming blimp messages to PacketWriter.
class BlimpMessageSender : public BlimpMessageProcessor {
 public:
  explicit BlimpMessageSender(PacketWriter* writer);
  ~BlimpMessageSender() override;

  void set_error_observer(ConnectionErrorObserver* observer) {
    error_observer_ = observer;
  }

  // BlimpMessageProcessor implementation.
  // |callback| receives net::OK on write success, or receives an error code
  // otherwise.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  void OnWritePacketComplete(int result);

  PacketWriter* writer_;
  ConnectionErrorObserver* error_observer_ = nullptr;
  scoped_refptr<net::IOBuffer> buffer_;
  net::CompletionCallback pending_process_msg_callback_;
  base::WeakPtrFactory<BlimpMessageSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessageSender);
};

BlimpMessageSender::BlimpMessageSender(PacketWriter* writer)
    : writer_(writer),
      buffer_(new net::IOBuffer(kMaxPacketPayloadSizeBytes)),
      weak_factory_(this) {
  DCHECK(writer_);
}

BlimpMessageSender::~BlimpMessageSender() {
  DVLOG(1) << "BlimpMessageSender destroyed.";
}

void BlimpMessageSender::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK(error_observer_);
  VLOG(1) << "Sending " << *message;

  const int msg_byte_size = message->ByteSize();
  if (msg_byte_size > static_cast<int>(kMaxPacketPayloadSizeBytes)) {
    DLOG(ERROR) << "Message rejected (too large): " << *message;
    callback.Run(net::ERR_MSG_TOO_BIG);
    return;
  }
  if (!message->SerializeToArray(buffer_->data(), msg_byte_size)) {
    DLOG(ERROR) << "Failed to serialize message.";
    callback.Run(net::ERR_INVALID_ARGUMENT);
    return;
  }

  // Check that no other message writes are in-flight at this time.
  DCHECK(pending_process_msg_callback_.is_null());
  pending_process_msg_callback_ = callback;

  writer_->WritePacket(
      scoped_refptr<net::DrainableIOBuffer>(
          new net::DrainableIOBuffer(buffer_.get(), msg_byte_size)),
      base::Bind(&BlimpMessageSender::OnWritePacketComplete,
                 weak_factory_.GetWeakPtr()));
}

void BlimpMessageSender::OnWritePacketComplete(int result) {
  DVLOG(2) << "OnWritePacketComplete, result=" << result;
  DCHECK_NE(net::ERR_IO_PENDING, result);

  // Create a stack-local copy of |pending_process_msg_callback_|, in case an
  // observer deletes |this|.
  net::CompletionCallback process_callback =
      base::ResetAndReturn(&pending_process_msg_callback_);

  if (result != net::OK) {
    error_observer_->OnConnectionError(result);
  }

  process_callback.Run(result);
}

TCPConnection::TCPConnection(std::unique_ptr<MessagePort> message_port)
    : BlimpConnection(),
      message_port_(std::move(message_port)),
      message_pump_(new BlimpMessagePump(message_port_->reader())),
      outgoing_msg_processor_(new BlimpMessageSender(message_port_->writer())) {
  message_pump_->set_error_observer(this);
  outgoing_msg_processor_->set_error_observer(this);
}

TCPConnection::~TCPConnection() {
  VLOG(1) << "TCPConnection destroyed.";
}

void TCPConnection::SetIncomingMessageProcessor(
    BlimpMessageProcessor* processor) {
  AddEndConnectionProcessor(processor);
  message_pump_->SetMessageProcessor(
      (processor != nullptr) ? GetEndConnectionProcessor() : nullptr);
}

BlimpMessageProcessor* TCPConnection::GetOutgoingMessageProcessor() {
  return outgoing_msg_processor_.get();
}

}  // namespace blimp
