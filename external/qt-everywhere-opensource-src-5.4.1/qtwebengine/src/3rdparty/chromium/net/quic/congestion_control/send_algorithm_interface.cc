// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/send_algorithm_interface.h"

#include "net/quic/congestion_control/fix_rate_sender.h"
#include "net/quic/congestion_control/tcp_cubic_sender.h"
#include "net/quic/quic_protocol.h"

namespace net {

const bool kUseReno = false;

class RttStats;

// Factory for send side congestion control algorithm.
SendAlgorithmInterface* SendAlgorithmInterface::Create(
    const QuicClock* clock,
    const RttStats* rtt_stats,
    CongestionFeedbackType type,
    QuicConnectionStats* stats) {
  switch (type) {
    case kTCP:
      return new TcpCubicSender(clock, rtt_stats, kUseReno,
                                kMaxTcpCongestionWindow, stats);
    case kInterArrival:
      LOG(DFATAL) << "InterArrivalSendAlgorithm no longer supported.";
      return NULL;
    case kFixRate:
      return new FixRateSender(rtt_stats);
    case kTCPBBR:
      LOG(DFATAL) << "BbrTcpSender is not supported.";
      return NULL;
  }
  return NULL;
}

}  // namespace net
