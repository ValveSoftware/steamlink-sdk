// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/rand_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/logging_impl.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

// Insert frame duration- one second.
const int64 kIntervalTime1S = 1;
// Test frame rate goal - 30fps.
const int kFrameIntervalMs = 33;

static const int64 kStartMillisecond = INT64_C(12345678900000);

class LoggingImplTest : public ::testing::Test {
 protected:
  LoggingImplTest() {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    logging_.AddRawEventSubscriber(&event_subscriber_);
  }

  virtual ~LoggingImplTest() {
    logging_.RemoveRawEventSubscriber(&event_subscriber_);
  }

  LoggingImpl logging_;
  base::SimpleTestTickClock testing_clock_;
  SimpleEventSubscriber event_subscriber_;

  DISALLOW_COPY_AND_ASSIGN(LoggingImplTest);
};

TEST_F(LoggingImplTest, BasicFrameLogging) {
  base::TimeTicks start_time = testing_clock_.NowTicks();
  base::TimeDelta time_interval = testing_clock_.NowTicks() - start_time;
  uint32 rtp_timestamp = 0;
  uint32 frame_id = 0;
  base::TimeTicks now;
  do {
    now = testing_clock_.NowTicks();
    logging_.InsertFrameEvent(
        now, FRAME_CAPTURE_BEGIN, VIDEO_EVENT, rtp_timestamp, frame_id);
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kFrameIntervalMs));
    rtp_timestamp += kFrameIntervalMs * 90;
    ++frame_id;
    time_interval = now - start_time;
  }  while (time_interval.InSeconds() < kIntervalTime1S);

  // Get logging data.
  std::vector<FrameEvent> frame_events;
  event_subscriber_.GetFrameEventsAndReset(&frame_events);
  // Size of vector should be equal to the number of events logged,
  // which equals to number of frames in this case.
  EXPECT_EQ(frame_id, frame_events.size());
}

TEST_F(LoggingImplTest, FrameLoggingWithSize) {
  // Average packet size.
  const int kBaseFrameSizeBytes = 25000;
  const int kRandomSizeInterval = 100;
  base::TimeTicks start_time = testing_clock_.NowTicks();
  base::TimeDelta time_interval = testing_clock_.NowTicks() - start_time;
  uint32 rtp_timestamp = 0;
  uint32 frame_id = 0;
  size_t sum_size = 0;
  int target_bitrate = 1234;
  do {
    int size = kBaseFrameSizeBytes +
        base::RandInt(-kRandomSizeInterval, kRandomSizeInterval);
    sum_size += static_cast<size_t>(size);
    logging_.InsertEncodedFrameEvent(testing_clock_.NowTicks(),
                                     FRAME_ENCODED, VIDEO_EVENT, rtp_timestamp,
                                     frame_id, size, true, target_bitrate);
    testing_clock_.Advance(base::TimeDelta::FromMilliseconds(kFrameIntervalMs));
    rtp_timestamp += kFrameIntervalMs * 90;
    ++frame_id;
    time_interval = testing_clock_.NowTicks() - start_time;
  } while (time_interval.InSeconds() < kIntervalTime1S);
  // Get logging data.
  std::vector<FrameEvent> frame_events;
  event_subscriber_.GetFrameEventsAndReset(&frame_events);
  // Size of vector should be equal to the number of events logged, which
  // equals to number of frames in this case.
  EXPECT_EQ(frame_id, frame_events.size());
}

TEST_F(LoggingImplTest, FrameLoggingWithDelay) {
  // Average packet size.
  const int kPlayoutDelayMs = 50;
  const int kRandomSizeInterval = 20;
  base::TimeTicks start_time = testing_clock_.NowTicks();
  base::TimeDelta time_interval = testing_clock_.NowTicks() - start_time;
  uint32 rtp_timestamp = 0;
  uint32 frame_id = 0;
  do {
    int delay = kPlayoutDelayMs +
                base::RandInt(-kRandomSizeInterval, kRandomSizeInterval);
    logging_.InsertFrameEventWithDelay(
        testing_clock_.NowTicks(),
        FRAME_CAPTURE_BEGIN,
        VIDEO_EVENT,
        rtp_timestamp,
        frame_id,
        base::TimeDelta::FromMilliseconds(delay));
    testing_clock_.Advance(base::TimeDelta::FromMilliseconds(kFrameIntervalMs));
    rtp_timestamp += kFrameIntervalMs * 90;
    ++frame_id;
    time_interval = testing_clock_.NowTicks() - start_time;
  } while (time_interval.InSeconds() < kIntervalTime1S);
  // Get logging data.
  std::vector<FrameEvent> frame_events;
  event_subscriber_.GetFrameEventsAndReset(&frame_events);
  // Size of vector should be equal to the number of frames logged.
  EXPECT_EQ(frame_id, frame_events.size());
}

TEST_F(LoggingImplTest, MultipleEventFrameLogging) {
  base::TimeTicks start_time = testing_clock_.NowTicks();
  base::TimeDelta time_interval = testing_clock_.NowTicks() - start_time;
  uint32 rtp_timestamp = 0u;
  uint32 frame_id = 0u;
  uint32 num_events = 0u;
  do {
    logging_.InsertFrameEvent(testing_clock_.NowTicks(),
                              FRAME_CAPTURE_END,
                              VIDEO_EVENT,
                              rtp_timestamp,
                              frame_id);
    ++num_events;
    if (frame_id % 2) {
      logging_.InsertEncodedFrameEvent(testing_clock_.NowTicks(),
                                       FRAME_ENCODED, AUDIO_EVENT,
                                       rtp_timestamp,
                                       frame_id, 1500, true, 0);
    } else if (frame_id % 3) {
      logging_.InsertFrameEvent(testing_clock_.NowTicks(), FRAME_DECODED,
                                VIDEO_EVENT, rtp_timestamp, frame_id);
    } else {
      logging_.InsertFrameEventWithDelay(
          testing_clock_.NowTicks(), FRAME_PLAYOUT, VIDEO_EVENT,
          rtp_timestamp, frame_id, base::TimeDelta::FromMilliseconds(20));
    }
    ++num_events;

    testing_clock_.Advance(base::TimeDelta::FromMilliseconds(kFrameIntervalMs));
    rtp_timestamp += kFrameIntervalMs * 90;
    ++frame_id;
    time_interval = testing_clock_.NowTicks() - start_time;
  } while (time_interval.InSeconds() < kIntervalTime1S);
  // Get logging data.
  std::vector<FrameEvent> frame_events;
  event_subscriber_.GetFrameEventsAndReset(&frame_events);
  // Size of vector should be equal to the number of frames logged.
  EXPECT_EQ(num_events, frame_events.size());
  // Multiple events captured per frame.
}

TEST_F(LoggingImplTest, PacketLogging) {
  const int kNumPacketsPerFrame = 10;
  const int kBaseSize = 2500;
  const int kSizeInterval = 100;
  base::TimeTicks start_time = testing_clock_.NowTicks();
  base::TimeTicks latest_time;
  base::TimeDelta time_interval = testing_clock_.NowTicks() - start_time;
  RtpTimestamp rtp_timestamp = 0;
  int frame_id = 0;
  int num_packets = 0;
  int sum_size = 0u;
  do {
    for (int i = 0; i < kNumPacketsPerFrame; ++i) {
      int size = kBaseSize + base::RandInt(-kSizeInterval, kSizeInterval);
      sum_size += size;
      latest_time = testing_clock_.NowTicks();
      ++num_packets;
      logging_.InsertPacketEvent(latest_time,
                                 PACKET_RECEIVED,
                                 VIDEO_EVENT,
                                 rtp_timestamp,
                                 frame_id,
                                 i,
                                 kNumPacketsPerFrame,
                                 size);
    }
    testing_clock_.Advance(base::TimeDelta::FromMilliseconds(kFrameIntervalMs));
    rtp_timestamp += kFrameIntervalMs * 90;
    ++frame_id;
    time_interval = testing_clock_.NowTicks() - start_time;
  } while (time_interval.InSeconds() < kIntervalTime1S);
  // Get logging data.
  std::vector<PacketEvent> packet_events;
  event_subscriber_.GetPacketEventsAndReset(&packet_events);
  // Size of vector should be equal to the number of packets logged.
  EXPECT_EQ(num_packets, static_cast<int>(packet_events.size()));
}

TEST_F(LoggingImplTest, MultipleRawEventSubscribers) {
  SimpleEventSubscriber event_subscriber_2;

  // Now logging_ has two subscribers.
  logging_.AddRawEventSubscriber(&event_subscriber_2);

  logging_.InsertFrameEvent(testing_clock_.NowTicks(),
                            FRAME_CAPTURE_BEGIN,
                            VIDEO_EVENT,
                            /*rtp_timestamp*/ 0u,
                            /*frame_id*/ 0u);

  std::vector<FrameEvent> frame_events;
  event_subscriber_.GetFrameEventsAndReset(&frame_events);
  EXPECT_EQ(1u, frame_events.size());
  frame_events.clear();
  event_subscriber_2.GetFrameEventsAndReset(&frame_events);
  EXPECT_EQ(1u, frame_events.size());

  logging_.RemoveRawEventSubscriber(&event_subscriber_2);
}

}  // namespace cast
}  // namespace media
