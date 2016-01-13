// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/logging_raw.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class LoggingRawTest : public ::testing::Test {
 protected:
  LoggingRawTest() {
    raw_.AddSubscriber(&event_subscriber_);
  }

  virtual ~LoggingRawTest() { raw_.RemoveSubscriber(&event_subscriber_); }

  LoggingRaw raw_;
  SimpleEventSubscriber event_subscriber_;
  std::vector<FrameEvent> frame_events_;
  std::vector<PacketEvent> packet_events_;
};

TEST_F(LoggingRawTest, FrameEvent) {
  CastLoggingEvent event_type = FRAME_DECODED;
  EventMediaType media_type = VIDEO_EVENT;
  uint32 frame_id = 456u;
  RtpTimestamp rtp_timestamp = 123u;
  base::TimeTicks timestamp = base::TimeTicks();
  raw_.InsertFrameEvent(timestamp, event_type, media_type,
      rtp_timestamp, frame_id);

  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  EXPECT_TRUE(packet_events_.empty());

  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(0u, frame_events_[0].size);
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(media_type, frame_events_[0].media_type);
  EXPECT_EQ(base::TimeDelta(), frame_events_[0].delay_delta);
}

TEST_F(LoggingRawTest, EncodedFrameEvent) {
  CastLoggingEvent event_type = FRAME_ENCODED;
  EventMediaType media_type = VIDEO_EVENT;
  uint32 frame_id = 456u;
  RtpTimestamp rtp_timestamp = 123u;
  base::TimeTicks timestamp = base::TimeTicks();
  int size = 1024;
  bool key_frame = true;
  int target_bitrate = 4096;
  raw_.InsertEncodedFrameEvent(timestamp, event_type, media_type,
      rtp_timestamp, frame_id, size, key_frame, target_bitrate);

  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  EXPECT_TRUE(packet_events_.empty());

  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(size, static_cast<int>(frame_events_[0].size));
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(media_type, frame_events_[0].media_type);
  EXPECT_EQ(base::TimeDelta(), frame_events_[0].delay_delta);
  EXPECT_EQ(key_frame, frame_events_[0].key_frame);
  EXPECT_EQ(target_bitrate, frame_events_[0].target_bitrate);
}

TEST_F(LoggingRawTest, FrameEventWithDelay) {
  CastLoggingEvent event_type = FRAME_PLAYOUT;
  EventMediaType media_type = VIDEO_EVENT;
  uint32 frame_id = 456u;
  RtpTimestamp rtp_timestamp = 123u;
  base::TimeTicks timestamp = base::TimeTicks();
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(20);
  raw_.InsertFrameEventWithDelay(timestamp, event_type, media_type,
      rtp_timestamp, frame_id, delay);

  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  EXPECT_TRUE(packet_events_.empty());

  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(0u, frame_events_[0].size);
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(media_type, frame_events_[0].media_type);
  EXPECT_EQ(delay, frame_events_[0].delay_delta);
}

TEST_F(LoggingRawTest, PacketEvent) {
  CastLoggingEvent event_type = PACKET_RECEIVED;
  EventMediaType media_type = VIDEO_EVENT;
  uint32 frame_id = 456u;
  uint16 packet_id = 1u;
  uint16 max_packet_id = 10u;
  RtpTimestamp rtp_timestamp = 123u;
  base::TimeTicks timestamp = base::TimeTicks();
  size_t size = 1024u;
  raw_.InsertPacketEvent(timestamp, event_type, media_type,
      rtp_timestamp, frame_id, packet_id, max_packet_id, size);

  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  EXPECT_TRUE(frame_events_.empty());

  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  ASSERT_EQ(1u, packet_events_.size());

  EXPECT_EQ(rtp_timestamp, packet_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, packet_events_[0].frame_id);
  EXPECT_EQ(max_packet_id, packet_events_[0].max_packet_id);
  EXPECT_EQ(packet_id, packet_events_[0].packet_id);
  EXPECT_EQ(size, packet_events_[0].size);
  EXPECT_EQ(timestamp, packet_events_[0].timestamp);
  EXPECT_EQ(event_type, packet_events_[0].type);
  EXPECT_EQ(media_type, packet_events_[0].media_type);
}

TEST_F(LoggingRawTest, MultipleSubscribers) {
  SimpleEventSubscriber event_subscriber_2;

  // Now raw_ has two subscribers.
  raw_.AddSubscriber(&event_subscriber_2);

  CastLoggingEvent event_type = FRAME_DECODED;
  EventMediaType media_type = VIDEO_EVENT;
  uint32 frame_id = 456u;
  RtpTimestamp rtp_timestamp = 123u;
  base::TimeTicks timestamp = base::TimeTicks();
  raw_.InsertFrameEvent(timestamp, event_type, media_type,
                        rtp_timestamp, frame_id);

  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  EXPECT_TRUE(packet_events_.empty());

  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(0u, frame_events_[0].size);
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(media_type, frame_events_[0].media_type);
  EXPECT_EQ(base::TimeDelta(), frame_events_[0].delay_delta);

  event_subscriber_2.GetPacketEventsAndReset(&packet_events_);
  EXPECT_TRUE(packet_events_.empty());

  event_subscriber_2.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(0u, frame_events_[0].size);
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(media_type, frame_events_[0].media_type);
  EXPECT_EQ(base::TimeDelta(), frame_events_[0].delay_delta);

  // Remove event_subscriber_2, so it shouldn't receive events after this.
  raw_.RemoveSubscriber(&event_subscriber_2);

  media_type = AUDIO_EVENT;
  frame_id = 789;
  rtp_timestamp = 456;
  timestamp = base::TimeTicks();
  raw_.InsertFrameEvent(timestamp, event_type, media_type,
                        rtp_timestamp, frame_id);

  // |event_subscriber_| should still receive events.
  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(0u, frame_events_[0].size);
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(media_type, frame_events_[0].media_type);
  EXPECT_EQ(base::TimeDelta(), frame_events_[0].delay_delta);

  event_subscriber_2.GetFrameEventsAndReset(&frame_events_);
  EXPECT_TRUE(frame_events_.empty());
}

}  // namespace cast
}  // namespace media
