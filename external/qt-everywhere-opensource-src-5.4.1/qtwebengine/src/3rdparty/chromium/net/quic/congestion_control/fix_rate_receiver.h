// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Fix rate receive side congestion control, used for initial testing.

#ifndef NET_QUIC_CONGESTION_CONTROL_FIX_RATE_RECEIVER_H_
#define NET_QUIC_CONGESTION_CONTROL_FIX_RATE_RECEIVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/quic_bandwidth.h"

namespace net {

namespace test {
class FixRateReceiverPeer;
}  // namespace test

class NET_EXPORT_PRIVATE FixRateReceiver : public ReceiveAlgorithmInterface {
 public:
  FixRateReceiver();

  // Implements ReceiveAlgorithmInterface.
  virtual bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* feedback) OVERRIDE;

  // Implements ReceiveAlgorithmInterface.
  virtual void RecordIncomingPacket(QuicByteCount bytes,
                                    QuicPacketSequenceNumber sequence_number,
                                    QuicTime timestamp) OVERRIDE;
 private:
  friend class test::FixRateReceiverPeer;

  QuicBandwidth configured_rate_;

  DISALLOW_COPY_AND_ASSIGN(FixRateReceiver);
};

}  // namespace net
#endif  // NET_QUIC_CONGESTION_CONTROL_FIX_RATE_RECEIVER_H_
