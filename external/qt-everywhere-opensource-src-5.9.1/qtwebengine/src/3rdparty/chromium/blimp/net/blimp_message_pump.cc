// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_pump.h"

#include "base/macros.h"
#include "blimp/common/logging.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/packet_reader.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace blimp {

BlimpMessagePump::BlimpMessagePump(PacketReader* reader)
    : reader_(reader), buffer_(new net::GrowableIOBuffer), weak_factory_(this) {
  DCHECK(reader_);
  buffer_->SetCapacity(kMaxPacketPayloadSizeBytes);
}

BlimpMessagePump::~BlimpMessagePump() {}

void BlimpMessagePump::SetMessageProcessor(BlimpMessageProcessor* processor) {
  DVLOG(1) << "SetMessageProcessor, processor=" << processor;
  if (processor && !processor_) {
    processor_ = processor;
    ReadNextPacket();
  } else {
    // Don't allow |processor_| to be cleared while there's a read inflight.
    if (processor) {
      DCHECK(!processor_ || !read_inflight_);
    }
    processor_ = processor;
  }
}

void BlimpMessagePump::ReadNextPacket() {
  DVLOG(2) << "ReadNextPacket";
  DCHECK(processor_);
  DCHECK(!read_inflight_);
  read_inflight_ = true;
  buffer_->set_offset(0);
  reader_->ReadPacket(buffer_.get(),
                      base::Bind(&BlimpMessagePump::OnReadPacketComplete,
                                 weak_factory_.GetWeakPtr()));
}

void BlimpMessagePump::OnReadPacketComplete(int result) {
  DVLOG(2) << "OnReadPacketComplete, result=" << result;
  DCHECK(read_inflight_);
  read_inflight_ = false;
  if (result >= 0) {
    std::unique_ptr<BlimpMessage> message(new BlimpMessage);
    if (message->ParseFromArray(buffer_->data(), result)) {
      VLOG(1) << "Received " << *message;
      processor_->ProcessMessage(
          std::move(message),
          base::Bind(&BlimpMessagePump::OnProcessMessageComplete,
                     weak_factory_.GetWeakPtr()));
    } else {
      result = net::ERR_FAILED;
    }
  }

  if (result < 0) {
    error_observer_->OnConnectionError(result);
  }
}

void BlimpMessagePump::OnProcessMessageComplete(int result) {
  DVLOG(2) << "OnProcessMessageComplete, result=" << result;

  if (result < 0) {
    error_observer_->OnConnectionError(result);
    return;
  }

  if (processor_)
    ReadNextPacket();
}

}  // namespace blimp
