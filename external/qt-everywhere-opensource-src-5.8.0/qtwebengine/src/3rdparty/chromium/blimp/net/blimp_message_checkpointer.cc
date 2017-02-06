// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_checkpointer.h"

#include "base/logging.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/protocol_control.pb.h"
#include "blimp/net/blimp_message_checkpoint_observer.h"
#include "net/base/net_errors.h"

namespace blimp {

namespace {
const int kDeferCheckpointAckSeconds = 1;
}

BlimpMessageCheckpointer::BlimpMessageCheckpointer(
    BlimpMessageProcessor* incoming_processor,
    BlimpMessageProcessor* outgoing_processor,
    BlimpMessageCheckpointObserver* checkpoint_observer)
    : incoming_processor_(incoming_processor),
      outgoing_processor_(outgoing_processor),
      checkpoint_observer_(checkpoint_observer),
      weak_factory_(this) {
  DCHECK(incoming_processor_);
  DCHECK(outgoing_processor_);
  DCHECK(checkpoint_observer_);
}

BlimpMessageCheckpointer::~BlimpMessageCheckpointer() {}

void BlimpMessageCheckpointer::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  if (message->has_protocol_control()) {
    if (message->protocol_control().has_checkpoint_ack() &&
        message->protocol_control().checkpoint_ack().has_checkpoint_id()) {
      checkpoint_observer_->OnMessageCheckpoint(
          message->protocol_control().checkpoint_ack().checkpoint_id());
      callback.Run(net::OK);
    } else {
      DLOG(WARNING) << "Invalid checkpoint ACK. Dropping connection.";
      callback.Run(net::ERR_FAILED);
    }

    return;
  }

  // TODO(wez): Provide independent checkpoints for each message->type()?
  DCHECK(message->has_message_id());

  // Store the message-Id to include in the checkpoint ACK.
  checkpoint_id_ = message->message_id();

  // Kick the timer, if not running, to ACK after a short delay.
  if (!defer_timer_.IsRunning()) {
    defer_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromSeconds(kDeferCheckpointAckSeconds),
                       this, &BlimpMessageCheckpointer::SendCheckpointAck);
  }

  // Pass the message along for actual processing.
  incoming_processor_->ProcessMessage(
      std::move(message),
      base::Bind(&BlimpMessageCheckpointer::InvokeCompletionCallback,
                 weak_factory_.GetWeakPtr(), callback));
}

void BlimpMessageCheckpointer::InvokeCompletionCallback(
    const net::CompletionCallback& callback,
    int result) {
  callback.Run(result);
}

void BlimpMessageCheckpointer::SendCheckpointAck() {
  outgoing_processor_->ProcessMessage(
      CreateCheckpointAckMessage(checkpoint_id_), net::CompletionCallback());
}

}  // namespace blimp
