// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/fix_rate_receiver.h"

#include "base/basictypes.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"

namespace {
  static const int kInitialBitrate = 100000;  // In bytes per second.
}

namespace net {

FixRateReceiver::FixRateReceiver()
    : configured_rate_(QuicBandwidth::FromBytesPerSecond(kInitialBitrate)) {
}

bool FixRateReceiver::GenerateCongestionFeedback(
    QuicCongestionFeedbackFrame* feedback) {
  feedback->type = kFixRate;
  feedback->fix_rate.bitrate = configured_rate_;
  return true;
}

void FixRateReceiver::RecordIncomingPacket(
    QuicByteCount /*bytes*/,
    QuicPacketSequenceNumber /*sequence_number*/,
    QuicTime /*timestamp*/) {
  // Nothing to do for this simple implementation.
}

}  // namespace net
