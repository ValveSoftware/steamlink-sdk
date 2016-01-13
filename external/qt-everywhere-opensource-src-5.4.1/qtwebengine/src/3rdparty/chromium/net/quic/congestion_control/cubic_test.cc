// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/quic/congestion_control/cubic.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

const float kBeta = 0.7f;  // Default Cubic backoff factor.
const uint32 kNumConnections = 2;
const float kNConnectionBeta = (kNumConnections - 1 + kBeta) / kNumConnections;
const float kNConnectionAlpha = 3 * kNumConnections * kNumConnections *
      (1 - kNConnectionBeta) / (1 + kNConnectionBeta);

class CubicTest : public ::testing::Test {
 protected:
  CubicTest()
      : one_ms_(QuicTime::Delta::FromMilliseconds(1)),
        hundred_ms_(QuicTime::Delta::FromMilliseconds(100)),
        cubic_(&clock_, &stats_) {
  }
  const QuicTime::Delta one_ms_;
  const QuicTime::Delta hundred_ms_;
  MockClock clock_;
  QuicConnectionStats stats_;
  Cubic cubic_;
};

TEST_F(CubicTest, AboveOrigin) {
  // Convex growth.
  const QuicTime::Delta rtt_min = hundred_ms_;
  uint32 current_cwnd = 10;
  uint32 expected_cwnd = current_cwnd + 1;
  // Initialize the state.
  clock_.AdvanceTime(one_ms_);
  EXPECT_EQ(expected_cwnd,
            cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min));
  current_cwnd = expected_cwnd;
  // Normal TCP phase.
  for (int i = 0; i < 48; ++i) {
    for (uint32 n = 1; n < current_cwnd / kNConnectionAlpha; ++n) {
      // Call once per ACK.
      EXPECT_NEAR(current_cwnd,
                  cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min), 1);
    }
    clock_.AdvanceTime(hundred_ms_);
    current_cwnd = cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min);
    EXPECT_NEAR(expected_cwnd, current_cwnd, 1);
    expected_cwnd++;
  }
  // Cubic phase.
  for (int i = 0; i < 52; ++i) {
    for (uint32 n = 1; n < current_cwnd; ++n) {
      // Call once per ACK.
      EXPECT_EQ(current_cwnd,
                cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min));
    }
    clock_.AdvanceTime(hundred_ms_);
    current_cwnd = cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min);
  }
  // Total time elapsed so far; add min_rtt (0.1s) here as well.
  float elapsed_time_s = 10.0f + 0.1f;
  // |expected_cwnd| is initial value of cwnd + K * t^3, where K = 0.4.
  expected_cwnd = 11 + (elapsed_time_s * elapsed_time_s * elapsed_time_s * 410)
      / 1024;
  EXPECT_EQ(expected_cwnd, current_cwnd);
}

TEST_F(CubicTest, CwndIncreaseStatsDuringConvexRegion) {
  const QuicTime::Delta rtt_min = hundred_ms_;
  uint32 current_cwnd = 10;
  uint32 expected_cwnd = current_cwnd + 1;
  // Initialize controller state.
  clock_.AdvanceTime(one_ms_);
  expected_cwnd = cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min);
  current_cwnd = expected_cwnd;
  // Testing Reno mode increase.
  for (int i = 0; i < 48; ++i) {
    for (uint32 n = 1; n < current_cwnd / kNConnectionAlpha; ++n) {
      // Call once per ACK, causing cwnd growth in Reno mode.
      cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min);
    }
    // Advance current time so that cwnd update is allowed to happen by Cubic.
    clock_.AdvanceTime(hundred_ms_);
    current_cwnd = cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min);
    EXPECT_NEAR(expected_cwnd - 10, stats_.cwnd_increase_congestion_avoidance,
                1);
    EXPECT_NEAR(1u, stats_.cwnd_increase_cubic_mode, 1);
    expected_cwnd++;
  }
  uint32 old_cwnd = current_cwnd;
  stats_.cwnd_increase_cubic_mode = 0;
  stats_.cwnd_increase_congestion_avoidance = 0;

  // Testing Cubic mode increase.
  for (int i = 0; i < 52; ++i) {
    for (uint32 n = 1; n < current_cwnd; ++n) {
      // Call once per ACK.
      cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min);
    }
    clock_.AdvanceTime(hundred_ms_);
    current_cwnd = cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min);
  }
  // Total time elapsed so far; add min_rtt (0.1s) here as well.
  float elapsed_time_s = 10.0f + 0.1f;
  // |expected_cwnd| is initial value of cwnd + K * t^3, where K = 0.4.
  expected_cwnd = 11 + (elapsed_time_s * elapsed_time_s * elapsed_time_s * 410)
      / 1024;
  EXPECT_EQ(expected_cwnd - old_cwnd, stats_.cwnd_increase_cubic_mode);
  EXPECT_EQ(expected_cwnd - old_cwnd,
            stats_.cwnd_increase_congestion_avoidance);
}


TEST_F(CubicTest, LossEvents) {
  const QuicTime::Delta rtt_min = hundred_ms_;
  uint32 current_cwnd = 422;
  uint32 expected_cwnd = current_cwnd + 1;
  // Initialize the state.
  clock_.AdvanceTime(one_ms_);
  EXPECT_EQ(expected_cwnd,
            cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min));
  expected_cwnd = static_cast<int>(current_cwnd * kNConnectionBeta);
  EXPECT_EQ(expected_cwnd,
            cubic_.CongestionWindowAfterPacketLoss(current_cwnd));
  expected_cwnd = static_cast<int>(current_cwnd * kNConnectionBeta);
  EXPECT_EQ(expected_cwnd,
            cubic_.CongestionWindowAfterPacketLoss(current_cwnd));
}

TEST_F(CubicTest, BelowOrigin) {
  // Concave growth.
  const QuicTime::Delta rtt_min = hundred_ms_;
  uint32 current_cwnd = 422;
  uint32 expected_cwnd = current_cwnd + 1;
  // Initialize the state.
  clock_.AdvanceTime(one_ms_);
  EXPECT_EQ(expected_cwnd,
            cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min));
  expected_cwnd = static_cast<int>(current_cwnd * kNConnectionBeta);
  EXPECT_EQ(expected_cwnd,
            cubic_.CongestionWindowAfterPacketLoss(current_cwnd));
  current_cwnd = expected_cwnd;
  // First update after loss to initialize the epoch.
  current_cwnd = cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min);
  uint32 old_cwnd =  current_cwnd;
  // Cubic phase.
  stats_.cwnd_increase_cubic_mode = 0;
  stats_.cwnd_increase_congestion_avoidance = 0;
  for (int i = 0; i < 40 ; ++i) {
    clock_.AdvanceTime(hundred_ms_);
    current_cwnd = cubic_.CongestionWindowAfterAck(current_cwnd, rtt_min);
  }
  expected_cwnd = 422;
  EXPECT_EQ(expected_cwnd, current_cwnd);
  EXPECT_EQ(expected_cwnd - old_cwnd, stats_.cwnd_increase_cubic_mode);
  EXPECT_EQ(expected_cwnd - old_cwnd,
            stats_.cwnd_increase_congestion_avoidance);
}

}  // namespace test
}  // namespace net
