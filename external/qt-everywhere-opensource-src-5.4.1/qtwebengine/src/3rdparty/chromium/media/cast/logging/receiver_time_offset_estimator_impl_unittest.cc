// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/receiver_time_offset_estimator_impl.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class ReceiverTimeOffsetEstimatorImplTest : public ::testing::Test {
 protected:
  ReceiverTimeOffsetEstimatorImplTest()
      : sender_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(sender_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(sender_clock_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_)) {
    cast_environment_->Logging()->AddRawEventSubscriber(&estimator_);
  }

  virtual ~ReceiverTimeOffsetEstimatorImplTest() {
    cast_environment_->Logging()->RemoveRawEventSubscriber(&estimator_);
  }

  void AdvanceClocks(base::TimeDelta time) {
    sender_clock_->Advance(time);
    receiver_clock_.Advance(time);
  }

  base::SimpleTestTickClock* sender_clock_;  // Owned by CastEnvironment.
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  base::SimpleTestTickClock receiver_clock_;
  ReceiverTimeOffsetEstimatorImpl estimator_;
};

// Suppose the true offset is 100ms.
// Event A occurred at sender time 20ms.
// Event B occurred at receiver time 130ms. (sender time 30ms)
// Event C occurred at sender time 60ms.
// Then the bound after all 3 events have arrived is [130-60=70, 130-20=110].
TEST_F(ReceiverTimeOffsetEstimatorImplTest, EstimateOffset) {
  int64 true_offset_ms = 100;
  receiver_clock_.Advance(base::TimeDelta::FromMilliseconds(true_offset_ms));

  base::TimeDelta lower_bound;
  base::TimeDelta upper_bound;

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  RtpTimestamp rtp_timestamp = 0;
  uint32 frame_id = 0;

  AdvanceClocks(base::TimeDelta::FromMilliseconds(20));

  cast_environment_->Logging()->InsertEncodedFrameEvent(
      sender_clock_->NowTicks(),
      FRAME_ENCODED, VIDEO_EVENT,
      rtp_timestamp,
      frame_id,
      1234,
      true,
      5678);

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  AdvanceClocks(base::TimeDelta::FromMilliseconds(10));
  cast_environment_->Logging()->InsertFrameEvent(
      receiver_clock_.NowTicks(), FRAME_ACK_SENT, VIDEO_EVENT,
      rtp_timestamp, frame_id);

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  AdvanceClocks(base::TimeDelta::FromMilliseconds(30));
  cast_environment_->Logging()->InsertFrameEvent(
      sender_clock_->NowTicks(), FRAME_ACK_RECEIVED, VIDEO_EVENT,
      rtp_timestamp, frame_id);

  EXPECT_TRUE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  int64 lower_bound_ms = lower_bound.InMilliseconds();
  int64 upper_bound_ms = upper_bound.InMilliseconds();
  EXPECT_EQ(70, lower_bound_ms);
  EXPECT_EQ(110, upper_bound_ms);
  EXPECT_GE(true_offset_ms, lower_bound_ms);
  EXPECT_LE(true_offset_ms, upper_bound_ms);
}

// Same scenario as above, but event C arrives before event B. It doens't mean
// event C occurred before event B.
TEST_F(ReceiverTimeOffsetEstimatorImplTest, EventCArrivesBeforeEventB) {
  int64 true_offset_ms = 100;
  receiver_clock_.Advance(base::TimeDelta::FromMilliseconds(true_offset_ms));

  base::TimeDelta lower_bound;
  base::TimeDelta upper_bound;

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  RtpTimestamp rtp_timestamp = 0;
  uint32 frame_id = 0;

  AdvanceClocks(base::TimeDelta::FromMilliseconds(20));

  cast_environment_->Logging()->InsertEncodedFrameEvent(
      sender_clock_->NowTicks(),
      FRAME_ENCODED, VIDEO_EVENT,
      rtp_timestamp,
      frame_id,
      1234,
      true,
      5678);

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  AdvanceClocks(base::TimeDelta::FromMilliseconds(10));
  base::TimeTicks event_b_time = receiver_clock_.NowTicks();
  AdvanceClocks(base::TimeDelta::FromMilliseconds(30));
  base::TimeTicks event_c_time = sender_clock_->NowTicks();

  cast_environment_->Logging()->InsertFrameEvent(
      event_c_time, FRAME_ACK_RECEIVED, VIDEO_EVENT, rtp_timestamp, frame_id);

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  cast_environment_->Logging()->InsertFrameEvent(
      event_b_time, FRAME_ACK_SENT, VIDEO_EVENT, rtp_timestamp, frame_id);

  EXPECT_TRUE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  int64 lower_bound_ms = lower_bound.InMilliseconds();
  int64 upper_bound_ms = upper_bound.InMilliseconds();
  EXPECT_EQ(70, lower_bound_ms);
  EXPECT_EQ(110, upper_bound_ms);
  EXPECT_GE(true_offset_ms, lower_bound_ms);
  EXPECT_LE(true_offset_ms, upper_bound_ms);
}

TEST_F(ReceiverTimeOffsetEstimatorImplTest, MultipleIterations) {
  int64 true_offset_ms = 100;
  receiver_clock_.Advance(base::TimeDelta::FromMilliseconds(true_offset_ms));

  base::TimeDelta lower_bound;
  base::TimeDelta upper_bound;

  RtpTimestamp rtp_timestamp_a = 0;
  int frame_id_a = 0;
  RtpTimestamp rtp_timestamp_b = 90;
  int frame_id_b = 1;
  RtpTimestamp rtp_timestamp_c = 180;
  int frame_id_c = 2;

  // Frame 1 times: [20, 30+100, 60]
  // Frame 2 times: [30, 50+100, 55]
  // Frame 3 times: [77, 80+100, 110]
  // Bound should end up at [95, 103]
  // Events times in chronological order: 20, 30 x2, 50, 55, 60, 77, 80, 110
  AdvanceClocks(base::TimeDelta::FromMilliseconds(20));
  cast_environment_->Logging()->InsertEncodedFrameEvent(
      sender_clock_->NowTicks(),
      FRAME_ENCODED, VIDEO_EVENT,
      rtp_timestamp_a,
      frame_id_a,
      1234,
      true,
      5678);

  AdvanceClocks(base::TimeDelta::FromMilliseconds(10));
  cast_environment_->Logging()->InsertEncodedFrameEvent(
      sender_clock_->NowTicks(),
      FRAME_ENCODED, VIDEO_EVENT,
      rtp_timestamp_b,
      frame_id_b,
      1234,
      true,
      5678);
  cast_environment_->Logging()->InsertFrameEvent(
      receiver_clock_.NowTicks(), FRAME_ACK_SENT, VIDEO_EVENT,
      rtp_timestamp_a, frame_id_a);

  AdvanceClocks(base::TimeDelta::FromMilliseconds(20));
  cast_environment_->Logging()->InsertFrameEvent(
      receiver_clock_.NowTicks(), FRAME_ACK_SENT, VIDEO_EVENT,
      rtp_timestamp_b, frame_id_b);

  AdvanceClocks(base::TimeDelta::FromMilliseconds(5));
  cast_environment_->Logging()->InsertFrameEvent(sender_clock_->NowTicks(),
                                                 FRAME_ACK_RECEIVED,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp_b,
                                                 frame_id_b);

  AdvanceClocks(base::TimeDelta::FromMilliseconds(5));
  cast_environment_->Logging()->InsertFrameEvent(sender_clock_->NowTicks(),
                                                 FRAME_ACK_RECEIVED,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp_a,
                                                 frame_id_a);

  AdvanceClocks(base::TimeDelta::FromMilliseconds(17));
  cast_environment_->Logging()->InsertEncodedFrameEvent(
      sender_clock_->NowTicks(),
      FRAME_ENCODED, VIDEO_EVENT,
      rtp_timestamp_c,
      frame_id_c,
      1234,
      true,
      5678);

  AdvanceClocks(base::TimeDelta::FromMilliseconds(3));
  cast_environment_->Logging()->InsertFrameEvent(
      receiver_clock_.NowTicks(), FRAME_ACK_SENT, VIDEO_EVENT,
      rtp_timestamp_c, frame_id_c);

  AdvanceClocks(base::TimeDelta::FromMilliseconds(30));
  cast_environment_->Logging()->InsertFrameEvent(sender_clock_->NowTicks(),
                                                 FRAME_ACK_RECEIVED,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp_c,
                                                 frame_id_c);

  EXPECT_TRUE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));
  int64 lower_bound_ms = lower_bound.InMilliseconds();
  int64 upper_bound_ms = upper_bound.InMilliseconds();
  EXPECT_EQ(95, lower_bound_ms);
  EXPECT_EQ(103, upper_bound_ms);
  EXPECT_GE(true_offset_ms, lower_bound_ms);
  EXPECT_LE(true_offset_ms, upper_bound_ms);
}

}  // namespace cast
}  // namespace media
