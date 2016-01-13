// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtp_receiver/receiver_stats.h"

#include "base/logging.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

static const uint32 kMaxSequenceNumber = 65536;

ReceiverStats::ReceiverStats(base::TickClock* clock)
    : clock_(clock),
      min_sequence_number_(0),
      max_sequence_number_(0),
      total_number_packets_(0),
      sequence_number_cycles_(0),
      interval_min_sequence_number_(0),
      interval_number_packets_(0),
      interval_wrap_count_(0) {}

ReceiverStats::~ReceiverStats() {}

void ReceiverStats::GetStatistics(uint8* fraction_lost,
                                  uint32* cumulative_lost,
                                  uint32* extended_high_sequence_number,
                                  uint32* jitter) {
  // Compute losses.
  if (interval_number_packets_ == 0) {
    *fraction_lost = 0;
  } else {
    int diff = 0;
    if (interval_wrap_count_ == 0) {
      diff = max_sequence_number_ - interval_min_sequence_number_ + 1;
    } else {
      diff = kMaxSequenceNumber * (interval_wrap_count_ - 1) +
             (max_sequence_number_ - interval_min_sequence_number_ +
              kMaxSequenceNumber + 1);
    }

    if (diff < 1) {
      *fraction_lost = 0;
    } else {
      float tmp_ratio =
          (1 - static_cast<float>(interval_number_packets_) / abs(diff));
      *fraction_lost = static_cast<uint8>(256 * tmp_ratio);
    }
  }

  int expected_packets_num = max_sequence_number_ - min_sequence_number_ + 1;
  if (total_number_packets_ == 0) {
    *cumulative_lost = 0;
  } else if (sequence_number_cycles_ == 0) {
    *cumulative_lost = expected_packets_num - total_number_packets_;
  } else {
    *cumulative_lost =
        kMaxSequenceNumber * (sequence_number_cycles_ - 1) +
        (expected_packets_num - total_number_packets_ + kMaxSequenceNumber);
  }

  // Extended high sequence number consists of the highest seq number and the
  // number of cycles (wrap).
  *extended_high_sequence_number =
      (sequence_number_cycles_ << 16) + max_sequence_number_;

  *jitter = static_cast<uint32>(std::abs(jitter_.InMillisecondsRoundedUp()));

  // Reset interval values.
  interval_min_sequence_number_ = 0;
  interval_number_packets_ = 0;
  interval_wrap_count_ = 0;
}

void ReceiverStats::UpdateStatistics(const RtpCastHeader& header) {
  const uint16 new_seq_num = header.sequence_number;

  if (interval_number_packets_ == 0) {
    // First packet in the interval.
    interval_min_sequence_number_ = new_seq_num;
  }
  if (total_number_packets_ == 0) {
    // First incoming packet.
    min_sequence_number_ = new_seq_num;
    max_sequence_number_ = new_seq_num;
  }

  if (IsNewerSequenceNumber(new_seq_num, max_sequence_number_)) {
    // Check wrap.
    if (new_seq_num < max_sequence_number_) {
      ++sequence_number_cycles_;
      ++interval_wrap_count_;
    }
    max_sequence_number_ = new_seq_num;
  }

  // Compute Jitter.
  base::TimeTicks now = clock_->NowTicks();
  base::TimeDelta delta_new_timestamp =
      base::TimeDelta::FromMilliseconds(header.rtp_timestamp);
  if (total_number_packets_ > 0) {
    // Update jitter.
    base::TimeDelta delta =
        (now - last_received_packet_time_) -
        ((delta_new_timestamp - last_received_timestamp_) / 90);
    jitter_ += (delta - jitter_) / 16;
  }
  last_received_timestamp_ = delta_new_timestamp;
  last_received_packet_time_ = now;

  // Increment counters.
  ++total_number_packets_;
  ++interval_number_packets_;
}

}  // namespace cast
}  // namespace media
