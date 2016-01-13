// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CONGESTION_CONTROL_TCP_LOSS_ALGORITHM_H_
#define NET_QUIC_CONGESTION_CONTROL_TCP_LOSS_ALGORITHM_H_

#include <algorithm>
#include <map>

#include "base/basictypes.h"
#include "net/quic/congestion_control/loss_detection_interface.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"
#include "net/quic/quic_unacked_packet_map.h"

namespace net {

// Class which implement's TCP's approach of detecting loss when 3 nacks have
// been received for a packet.  Also implements TCP's early retransmit(RFC5827).
class NET_EXPORT_PRIVATE TCPLossAlgorithm : public LossDetectionInterface {
 public:
  TCPLossAlgorithm();
  virtual ~TCPLossAlgorithm() {}

  virtual LossDetectionType GetLossDetectionType() const OVERRIDE;

  // Uses nack counts to decide when packets are lost.
  virtual SequenceNumberSet DetectLostPackets(
      const QuicUnackedPacketMap& unacked_packets,
      const QuicTime& time,
      QuicPacketSequenceNumber largest_observed,
      const RttStats& rtt_stats) OVERRIDE;

  // Returns a non-zero value when the early retransmit timer is active.
  virtual QuicTime GetLossTimeout() const OVERRIDE;

 private:
  QuicTime loss_detection_timeout_;

  DISALLOW_COPY_AND_ASSIGN(TCPLossAlgorithm);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_TCP_LOSS_ALGORITHM_H_
