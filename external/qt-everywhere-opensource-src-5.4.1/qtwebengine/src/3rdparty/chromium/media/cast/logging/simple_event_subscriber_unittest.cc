// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class SimpleEventSubscriberTest : public ::testing::Test {
 protected:
  SimpleEventSubscriberTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_)) {
    cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);
  }

  virtual ~SimpleEventSubscriberTest() {
    cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber_);
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  SimpleEventSubscriber event_subscriber_;
};

TEST_F(SimpleEventSubscriberTest, GetAndResetEvents) {
  // Log some frame events.
  cast_environment_->Logging()->InsertEncodedFrameEvent(
      testing_clock_->NowTicks(), FRAME_ENCODED, AUDIO_EVENT,
      /*rtp_timestamp*/ 100u, /*frame_id*/ 0u, /*frame_size*/ 123,
      /*key_frame*/ false, 0);
  cast_environment_->Logging()->InsertFrameEventWithDelay(
      testing_clock_->NowTicks(), FRAME_PLAYOUT, AUDIO_EVENT,
      /*rtp_timestamp*/ 100u,
      /*frame_id*/ 0u, /*delay*/ base::TimeDelta::FromMilliseconds(100));
  cast_environment_->Logging()->InsertFrameEvent(
      testing_clock_->NowTicks(), FRAME_DECODED, AUDIO_EVENT,
      /*rtp_timestamp*/ 200u,
      /*frame_id*/ 0u);

  // Log some packet events.
  cast_environment_->Logging()->InsertPacketEvent(
      testing_clock_->NowTicks(), PACKET_RECEIVED, AUDIO_EVENT,
      /*rtp_timestamp*/ 200u,
      /*frame_id*/ 0u, /*packet_id*/ 1u, /*max_packet_id*/ 5u, /*size*/ 100u);
  cast_environment_->Logging()->InsertPacketEvent(
      testing_clock_->NowTicks(), FRAME_DECODED, VIDEO_EVENT,
      /*rtp_timestamp*/ 200u, /*frame_id*/ 0u, /*packet_id*/ 1u,
      /*max_packet_id*/ 5u, /*size*/ 100u);
  cast_environment_->Logging()->InsertPacketEvent(
      testing_clock_->NowTicks(), FRAME_DECODED, VIDEO_EVENT,
      /*rtp_timestamp*/ 300u, /*frame_id*/ 0u, /*packet_id*/ 1u,
      /*max_packet_id*/ 5u, /*size*/ 100u);

  std::vector<FrameEvent> frame_events;
  event_subscriber_.GetFrameEventsAndReset(&frame_events);
  EXPECT_EQ(3u, frame_events.size());

  std::vector<PacketEvent> packet_events;
  event_subscriber_.GetPacketEventsAndReset(&packet_events);
  EXPECT_EQ(3u, packet_events.size());

  // Calling this function again should result in empty vector because no events
  // were logged since last call.
  event_subscriber_.GetFrameEventsAndReset(&frame_events);
  event_subscriber_.GetPacketEventsAndReset(&packet_events);
  EXPECT_TRUE(frame_events.empty());
  EXPECT_TRUE(packet_events.empty());
}

}  // namespace cast
}  // namespace media
