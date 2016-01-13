// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <stdint.h>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "media/cast/rtp_receiver/receiver_stats.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = INT64_C(12345678900000);
static const uint32 kStdTimeIncrementMs = 33;

class ReceiverStatsTest : public ::testing::Test {
 protected:
  ReceiverStatsTest()
      : stats_(&testing_clock_),
        fraction_lost_(0),
        cumulative_lost_(0),
        extended_high_sequence_number_(0),
        jitter_(0) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    start_time_ = testing_clock_.NowTicks();
    delta_increments_ = base::TimeDelta::FromMilliseconds(kStdTimeIncrementMs);
  }
  virtual ~ReceiverStatsTest() {}

  uint32 ExpectedJitter(uint32 const_interval, int num_packets) {
    float jitter = 0;
    // Assume timestamps have a constant kStdTimeIncrementMs interval.
    float float_interval =
        static_cast<float>(const_interval - kStdTimeIncrementMs);
    for (int i = 0; i < num_packets; ++i) {
      jitter += (float_interval - jitter) / 16;
    }
    return static_cast<uint32>(jitter + 0.5f);
  }

  ReceiverStats stats_;
  RtpCastHeader rtp_header_;
  uint8 fraction_lost_;
  uint32 cumulative_lost_;
  uint32 extended_high_sequence_number_;
  uint32 jitter_;
  base::SimpleTestTickClock testing_clock_;
  base::TimeTicks start_time_;
  base::TimeDelta delta_increments_;

  DISALLOW_COPY_AND_ASSIGN(ReceiverStatsTest);
};

TEST_F(ReceiverStatsTest, ResetState) {
  stats_.GetStatistics(&fraction_lost_,
                       &cumulative_lost_,
                       &extended_high_sequence_number_,
                       &jitter_);
  EXPECT_EQ(0u, fraction_lost_);
  EXPECT_EQ(0u, cumulative_lost_);
  EXPECT_EQ(0u, extended_high_sequence_number_);
  EXPECT_EQ(0u, jitter_);
}

TEST_F(ReceiverStatsTest, LossCount) {
  for (int i = 0; i < 300; ++i) {
    if (i % 4)
      stats_.UpdateStatistics(rtp_header_);
    if (i % 3) {
      rtp_header_.rtp_timestamp += 33 * 90;
    }
    ++rtp_header_.sequence_number;
    testing_clock_.Advance(delta_increments_);
  }
  stats_.GetStatistics(&fraction_lost_,
                       &cumulative_lost_,
                       &extended_high_sequence_number_,
                       &jitter_);
  EXPECT_EQ(63u, fraction_lost_);
  EXPECT_EQ(74u, cumulative_lost_);
  // Build extended sequence number.
  const uint32 extended_seq_num = rtp_header_.sequence_number - 1;
  EXPECT_EQ(extended_seq_num, extended_high_sequence_number_);
}

TEST_F(ReceiverStatsTest, NoLossWrap) {
  rtp_header_.sequence_number = 65500;
  for (int i = 0; i < 300; ++i) {
    stats_.UpdateStatistics(rtp_header_);
    if (i % 3) {
      rtp_header_.rtp_timestamp += 33 * 90;
    }
    ++rtp_header_.sequence_number;
    testing_clock_.Advance(delta_increments_);
  }
  stats_.GetStatistics(&fraction_lost_,
                       &cumulative_lost_,
                       &extended_high_sequence_number_,
                       &jitter_);
  EXPECT_EQ(0u, fraction_lost_);
  EXPECT_EQ(0u, cumulative_lost_);
  // Build extended sequence number (one wrap cycle).
  const uint32 extended_seq_num = (1 << 16) + rtp_header_.sequence_number - 1;
  EXPECT_EQ(extended_seq_num, extended_high_sequence_number_);
}

TEST_F(ReceiverStatsTest, LossCountWrap) {
  const uint32 kStartSequenceNumber = 65500;
  rtp_header_.sequence_number = kStartSequenceNumber;
  for (int i = 0; i < 300; ++i) {
    if (i % 4)
      stats_.UpdateStatistics(rtp_header_);
    if (i % 3)
      // Update timestamp.
      ++rtp_header_.rtp_timestamp;
    ++rtp_header_.sequence_number;
    testing_clock_.Advance(delta_increments_);
  }
  stats_.GetStatistics(&fraction_lost_,
                       &cumulative_lost_,
                       &extended_high_sequence_number_,
                       &jitter_);
  EXPECT_EQ(63u, fraction_lost_);
  EXPECT_EQ(74u, cumulative_lost_);
  // Build extended sequence number (one wrap cycle).
  const uint32 extended_seq_num = (1 << 16) + rtp_header_.sequence_number - 1;
  EXPECT_EQ(extended_seq_num, extended_high_sequence_number_);
}

TEST_F(ReceiverStatsTest, BasicJitter) {
  for (int i = 0; i < 300; ++i) {
    stats_.UpdateStatistics(rtp_header_);
    ++rtp_header_.sequence_number;
    rtp_header_.rtp_timestamp += 33 * 90;
    testing_clock_.Advance(delta_increments_);
  }
  stats_.GetStatistics(&fraction_lost_,
                       &cumulative_lost_,
                       &extended_high_sequence_number_,
                       &jitter_);
  EXPECT_FALSE(fraction_lost_);
  EXPECT_FALSE(cumulative_lost_);
  // Build extended sequence number (one wrap cycle).
  const uint32 extended_seq_num = rtp_header_.sequence_number - 1;
  EXPECT_EQ(extended_seq_num, extended_high_sequence_number_);
  EXPECT_EQ(ExpectedJitter(kStdTimeIncrementMs, 300), jitter_);
}

TEST_F(ReceiverStatsTest, NonTrivialJitter) {
  const int kAdditionalIncrement = 5;
  for (int i = 0; i < 300; ++i) {
    stats_.UpdateStatistics(rtp_header_);
    ++rtp_header_.sequence_number;
    rtp_header_.rtp_timestamp += 33 * 90;
    base::TimeDelta additional_delta =
        base::TimeDelta::FromMilliseconds(kAdditionalIncrement);
    testing_clock_.Advance(delta_increments_ + additional_delta);
  }
  stats_.GetStatistics(&fraction_lost_,
                       &cumulative_lost_,
                       &extended_high_sequence_number_,
                       &jitter_);
  EXPECT_FALSE(fraction_lost_);
  EXPECT_FALSE(cumulative_lost_);
  // Build extended sequence number (one wrap cycle).
  const uint32 extended_seq_num = rtp_header_.sequence_number - 1;
  EXPECT_EQ(extended_seq_num, extended_high_sequence_number_);
  EXPECT_EQ(ExpectedJitter(kStdTimeIncrementMs + kAdditionalIncrement, 300),
            jitter_);
}

}  // namespace cast
}  // namespace media
