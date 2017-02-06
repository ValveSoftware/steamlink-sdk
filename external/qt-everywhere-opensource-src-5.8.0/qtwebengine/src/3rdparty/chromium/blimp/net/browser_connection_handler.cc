// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/browser_connection_handler.h"

#include "base/logging.h"
#include "base/macros.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_message_checkpointer.h"
#include "blimp/net/blimp_message_demultiplexer.h"
#include "blimp/net/blimp_message_multiplexer.h"
#include "blimp/net/blimp_message_output_buffer.h"
#include "blimp/net/blimp_message_processor.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace {

// Maximum footprint of the output buffer.
// TODO(kmarshall): Use a value that's computed from the platform.
const int kMaxBufferSizeBytes = 1 << 24;

}  // namespace

BrowserConnectionHandler::BrowserConnectionHandler()
    : demultiplexer_(new BlimpMessageDemultiplexer),
      output_buffer_(new BlimpMessageOutputBuffer(kMaxBufferSizeBytes)),
      multiplexer_(new BlimpMessageMultiplexer(output_buffer_.get())),
      checkpointer_(new BlimpMessageCheckpointer(demultiplexer_.get(),
                                                 output_buffer_.get(),
                                                 output_buffer_.get())) {}

BrowserConnectionHandler::~BrowserConnectionHandler() {}

std::unique_ptr<BlimpMessageProcessor>
BrowserConnectionHandler::RegisterFeature(
    BlimpMessage::FeatureCase feature_case,
    BlimpMessageProcessor* incoming_processor) {
  demultiplexer_->AddProcessor(feature_case, incoming_processor);
  return multiplexer_->CreateSender(feature_case);
}

void BrowserConnectionHandler::HandleConnection(
    std::unique_ptr<BlimpConnection> connection) {
  DCHECK(connection);
  VLOG(1) << "HandleConnection " << connection.get();

  if (connection_) {
    DropCurrentConnection();
  }
  connection_ = std::move(connection);

  // Hook up message streams to the connection.
  connection_->SetIncomingMessageProcessor(checkpointer_.get());
  output_buffer_->SetOutputProcessor(
      connection_->GetOutgoingMessageProcessor());
  connection_->AddConnectionErrorObserver(this);
}

void BrowserConnectionHandler::OnConnectionError(int error) {
  DropCurrentConnection();
}

void BrowserConnectionHandler::DropCurrentConnection() {
  if (!connection_) {
    return;
  }

  output_buffer_->SetOutputProcessor(nullptr);
  connection_.reset();
}

}  // namespace blimp
