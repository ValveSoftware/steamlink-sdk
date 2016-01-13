// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A test only class to enable simulations of send algorithms.

#ifndef NET_QUIC_CONGESTION_CONTROL_SEND_ALGORITHM_SIMULATOR_H_
#define NET_QUIC_CONGESTION_CONTROL_SEND_ALGORITHM_SIMULATOR_H_

#include <algorithm>

#include "base/basictypes.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/quic_test_utils.h"

namespace net {

class SendAlgorithmSimulator {
 public:
  struct SentPacket {
    SentPacket(QuicPacketSequenceNumber sequence_number,
               QuicTime send_time,
               QuicTime ack_time)
        : sequence_number(sequence_number),
          send_time(send_time),
          ack_time(ack_time) {}
    QuicPacketSequenceNumber sequence_number;
    QuicTime send_time;
    QuicTime ack_time;
  };

  // |rtt_stats| should be the same RttStats used by the |send_algorithm|.
  SendAlgorithmSimulator(SendAlgorithmInterface* send_algorithm,
                         MockClock* clock_,
                         RttStats* rtt_stats,
                         QuicBandwidth bandwidth,
                         QuicTime::Delta rtt);
  ~SendAlgorithmSimulator();

  void set_forward_loss_rate(float loss_rate) {
    DCHECK_LT(loss_rate, 1.0f);
    forward_loss_rate_ = loss_rate;
  }

  void set_reverse_loss_rate(float loss_rate) {
    DCHECK_LT(loss_rate, 1.0f);
    reverse_loss_rate_ = loss_rate;
  }

  void set_loss_correlation(float loss_correlation) {
    DCHECK_LT(loss_correlation, 1.0f);
    loss_correlation_ = loss_correlation;
  }

  void set_buffer_size(size_t buffer_size_bytes) {
    buffer_size_ = buffer_size_bytes;
  }

  // Sends the specified number of bytes as quickly as possible and returns the
  // average bandwidth in bytes per second.  The time elapsed is based on
  // waiting for all acks to arrive.
  QuicBandwidth SendBytes(size_t num_bytes);

  const RttStats* rtt_stats() const { return rtt_stats_; }

  QuicByteCount max_cwnd() const { return max_cwnd_; }
  QuicByteCount min_cwnd() const { return min_cwnd_; }
  QuicByteCount max_cwnd_drop() const { return max_cwnd_drop_; }
  QuicByteCount last_cwnd() const { return last_cwnd_; }

 private:
  // NextAckTime takes into account packet loss in both forward and reverse
  // direction, as well as delayed ack behavior.
  QuicTime::Delta NextAckDelta();

  // Whether all packets in sent_packets_ are lost.
  bool AllPacketsLost();

  // Sets the next acked.
  void FindNextAcked();

  // Process all the acks that should have arrived by the current time, and
  // lose any packets that are missing.  Returns the number of bytes acked.
  int HandlePendingAck();

  void SendDataNow();
  void RecordStats();

  // Advance the time by |delta| without sending anything.
  void AdvanceTime(QuicTime::Delta delta);

  // Elapsed time from the start of the connection.
  QuicTime ElapsedTime();

  SendAlgorithmInterface* send_algorithm_;
  MockClock* clock_;
  RttStats* rtt_stats_;
  // Next packet sequence number to send.
  QuicPacketSequenceNumber next_sent_;
  // Last packet sequence number acked.
  QuicPacketSequenceNumber last_acked_;
  // Packet sequence number to ack up to.
  QuicPacketSequenceNumber next_acked_;
  // Whether the next ack should be lost.
  bool lose_next_ack_;
  QuicByteCount bytes_in_flight_;
  // The times acks are expected, assuming acks are not lost and every packet
  // is acked.
  std::list<SentPacket> sent_packets_;

  test::SimpleRandom simple_random_;
  float forward_loss_rate_;  // Loss rate on the forward path.
  float reverse_loss_rate_;  // Loss rate on the reverse path.
  float loss_correlation_;   // Likelihood the subsequent packet is lost.
  QuicBandwidth bandwidth_;
  QuicTime::Delta rtt_;
  size_t buffer_size_;       // In bytes.

  // Stats collected for understanding the congestion control.
  QuicByteCount max_cwnd_;
  QuicByteCount min_cwnd_;
  QuicByteCount max_cwnd_drop_;
  QuicByteCount last_cwnd_;

  DISALLOW_COPY_AND_ASSIGN(SendAlgorithmSimulator);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_SEND_ALGORITHM_SIMULATOR_H_
