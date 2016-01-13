// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/congestion_control/congestion_control.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const uint32 kMaxBitrateConfigured = 5000000;
static const uint32 kMinBitrateConfigured = 500000;
static const int64 kStartMillisecond = INT64_C(12345678900000);
static const double kTargetEmptyBufferFraction = 0.9;

class CongestionControlTest : public ::testing::Test {
 protected:
  CongestionControlTest()
      : task_runner_(new test::FakeSingleThreadTaskRunner(&testing_clock_)) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    congestion_control_.reset(new CongestionControl(
        &testing_clock_, kMaxBitrateConfigured, kMinBitrateConfigured, 10));
  }

  void AckFrame(uint32 frame_id) {
    congestion_control_->AckFrame(frame_id, testing_clock_.NowTicks());
  }

  void Run(uint32 frames,
           size_t frame_size,
           base::TimeDelta rtt,
           base::TimeDelta frame_delay,
           base::TimeDelta ack_time) {
    for (frame_id_ = 0; frame_id_ < frames; frame_id_++) {
      congestion_control_->UpdateRtt(rtt);
      congestion_control_->SendFrameToTransport(
          frame_id_, frame_size, testing_clock_.NowTicks());
      task_runner_->PostDelayedTask(FROM_HERE,
                                    base::Bind(&CongestionControlTest::AckFrame,
                                               base::Unretained(this),
                                               frame_id_),
                                    ack_time);
      task_runner_->Sleep(frame_delay);
    }
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_ptr<CongestionControl> congestion_control_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  uint32 frame_id_;

  DISALLOW_COPY_AND_ASSIGN(CongestionControlTest);
};

TEST_F(CongestionControlTest, SimpleRun) {
  uint32 frame_delay = 33;
  uint32 frame_size = 10000 * 8;
  Run(500,
      frame_size,
      base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromMilliseconds(frame_delay),
      base::TimeDelta::FromMilliseconds(45));
  // Empty the buffer.
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(100));

  uint32 safe_bitrate = frame_size * 1000 / frame_delay;
  uint32 bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(300));
  EXPECT_NEAR(
      safe_bitrate / kTargetEmptyBufferFraction, bitrate, safe_bitrate * 0.05);

  bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + base::TimeDelta::FromMilliseconds(200),
      base::TimeDelta::FromMilliseconds(300));
  EXPECT_NEAR(safe_bitrate / kTargetEmptyBufferFraction * 2 / 3,
              bitrate,
              safe_bitrate * 0.05);

  bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + base::TimeDelta::FromMilliseconds(100),
      base::TimeDelta::FromMilliseconds(300));
  EXPECT_NEAR(safe_bitrate / kTargetEmptyBufferFraction * 1 / 3,
              bitrate,
              safe_bitrate * 0.05);

  // Add a large (100ms) frame.
  congestion_control_->SendFrameToTransport(
      frame_id_++, safe_bitrate * 100 / 1000, testing_clock_.NowTicks());

  // Results should show that we have ~200ms to send
  bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(300));
  EXPECT_NEAR(safe_bitrate / kTargetEmptyBufferFraction * 2 / 3,
              bitrate,
              safe_bitrate * 0.05);

  // Add another large (100ms) frame.
  congestion_control_->SendFrameToTransport(
      frame_id_++, safe_bitrate * 100 / 1000, testing_clock_.NowTicks());

  // Resulst should show that we have ~100ms to send
  bitrate = congestion_control_->GetBitrate(
      testing_clock_.NowTicks() + base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(300));
  EXPECT_NEAR(safe_bitrate / kTargetEmptyBufferFraction * 1 / 3,
              bitrate,
              safe_bitrate * 0.05);
}


}  // namespace cast
}  // namespace media
