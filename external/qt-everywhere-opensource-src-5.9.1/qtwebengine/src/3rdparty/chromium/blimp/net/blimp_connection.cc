// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_connection.h"

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

BlimpConnection::BlimpConnection()
    : end_connection_filter_(new EndConnectionFilter(this)) {}

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

void BlimpConnection::AddEndConnectionProcessor(
    BlimpMessageProcessor* processor) {
  end_connection_filter_->set_message_handler(processor);
}

BlimpMessageProcessor* BlimpConnection::GetEndConnectionProcessor() const {
  return end_connection_filter_.get();
}

void BlimpConnection::OnConnectionError(int error) {
  VLOG(1) << "OnConnectionError, error=" << error;

  // Propagate the error to all observers.
  for (auto& observer : error_observers_) {
    observer.OnConnectionError(error);
  }
}

}  // namespace blimp
