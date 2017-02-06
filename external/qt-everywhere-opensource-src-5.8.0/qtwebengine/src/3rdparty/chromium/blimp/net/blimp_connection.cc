// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_connection.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "blimp/common/logging.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_message_pump.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/packet_reader.h"
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

  if (message->ByteSize() > static_cast<int>(kMaxPacketPayloadSizeBytes)) {
    DLOG(ERROR) << "Message rejected (too large): " << *message;
    callback.Run(net::ERR_MSG_TOO_BIG);
    return;
  }

  if (!message->SerializeToArray(buffer_->data(), message->GetCachedSize())) {
    DLOG(ERROR) << "Failed to serialize message.";
    callback.Run(net::ERR_INVALID_ARGUMENT);
    return;
  }

  // Check that no other message writes are in-flight at this time.
  DCHECK(pending_process_msg_callback_.is_null());
  pending_process_msg_callback_ = callback;

  writer_->WritePacket(
      scoped_refptr<net::DrainableIOBuffer>(
          new net::DrainableIOBuffer(buffer_.get(), message->ByteSize())),
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

// MessageProcessor filter used to route EndConnection messages through to
// OnConnectionError notifications on the owning BlimpConnection.
class BlimpConnection::EndConnectionFilter : public BlimpMessageProcessor {
 public:
  explicit EndConnectionFilter(BlimpConnection* connection);

  void set_message_handler(BlimpMessageProcessor* message_handler) {
    message_handler_ = message_handler;
  }

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  // Owning BlimpConnection, on which to call OnConnectionError.
  BlimpConnection* connection_;

  // Caller-provided message handler to forward non-EndConnection messages to.
  BlimpMessageProcessor* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(EndConnectionFilter);
};

BlimpConnection::EndConnectionFilter::EndConnectionFilter(
    BlimpConnection* connection)
    : connection_(connection), message_handler_(nullptr) {}

void BlimpConnection::EndConnectionFilter::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  if (message->has_protocol_control() &&
      message->protocol_control().has_end_connection()) {
    // Report the EndConnection reason to connection error observers.
    connection_->OnConnectionError(
        message->protocol_control().end_connection().reason());

    // Caller must ensure |callback| safe to call after OnConnectionError.
    callback.Run(message->protocol_control().end_connection().reason());
    return;
  }

  message_handler_->ProcessMessage(std::move(message), callback);
}

BlimpConnection::BlimpConnection(std::unique_ptr<PacketReader> reader,
                                 std::unique_ptr<PacketWriter> writer)
    : reader_(std::move(reader)),
      message_pump_(new BlimpMessagePump(reader_.get())),
      writer_(std::move(writer)),
      outgoing_msg_processor_(new BlimpMessageSender(writer_.get())),
      end_connection_filter_(new EndConnectionFilter(this)) {
  DCHECK(writer_);
  DCHECK(reader_);

  message_pump_->set_error_observer(this);
  outgoing_msg_processor_->set_error_observer(this);
}

BlimpConnection::BlimpConnection() {}

BlimpConnection::~BlimpConnection() {
  VLOG(1) << "BlimpConnection destroyed.";
}

void BlimpConnection::AddConnectionErrorObserver(
    ConnectionErrorObserver* observer) {
  error_observers_.AddObserver(observer);
}

void BlimpConnection::RemoveConnectionErrorObserver(
    ConnectionErrorObserver* observer) {
  error_observers_.RemoveObserver(observer);
}

void BlimpConnection::SetIncomingMessageProcessor(
    BlimpMessageProcessor* processor) {
  end_connection_filter_->set_message_handler(processor);
  message_pump_->SetMessageProcessor(processor ? end_connection_filter_.get()
                                               : nullptr);
}

BlimpMessageProcessor* BlimpConnection::GetOutgoingMessageProcessor() {
  return outgoing_msg_processor_.get();
}

void BlimpConnection::OnConnectionError(int error) {
  VLOG(1) << "OnConnectionError, error=" << error;

  // Propagate the error to all observers.
  FOR_EACH_OBSERVER(ConnectionErrorObserver, error_observers_,
                    OnConnectionError(error));
}

}  // namespace blimp
