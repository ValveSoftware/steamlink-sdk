// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/encoding_event_subscriber.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedPacketEvent;
using media::cast::proto::BasePacketEvent;
using media::cast::proto::LogMetadata;

namespace {

int64 InMilliseconds(base::TimeTicks event_time) {
  return (event_time - base::TimeTicks()).InMilliseconds();
}

}

namespace media {
namespace cast {

class EncodingEventSubscriberTest : public ::testing::Test {
 protected:
  EncodingEventSubscriberTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_)),
        first_rtp_timestamp_(0) {}

  void Init(EventMediaType event_media_type) {
    DCHECK(!event_subscriber_);
    event_subscriber_.reset(new EncodingEventSubscriber(event_media_type, 10));
    cast_environment_->Logging()->AddRawEventSubscriber(
        event_subscriber_.get());
  }

  virtual ~EncodingEventSubscriberTest() {
    if (event_subscriber_) {
      cast_environment_->Logging()->RemoveRawEventSubscriber(
          event_subscriber_.get());
    }
  }

  void GetEventsAndReset() {
    event_subscriber_->GetEventsAndReset(
        &metadata_, &frame_events_, &packet_events_);
    first_rtp_timestamp_ = metadata_.first_rtp_timestamp();
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<EncodingEventSubscriber> event_subscriber_;
  FrameEventList frame_events_;
  PacketEventList packet_events_;
  LogMetadata metadata_;
  RtpTimestamp first_rtp_timestamp_;
};

TEST_F(EncodingEventSubscriberTest, FrameEventTruncating) {
  Init(VIDEO_EVENT);

  base::TimeTicks now(testing_clock_->NowTicks());

  // Entry with RTP timestamp 0 should get dropped.
  for (int i = 0; i < 11; i++) {
    cast_environment_->Logging()->InsertFrameEvent(now,
                                                   FRAME_CAPTURE_BEGIN,
                                                   VIDEO_EVENT,
                                                   i * 100,
                                                   /*frame_id*/ 0);
    cast_environment_->Logging()->InsertFrameEvent(now,
                                                   FRAME_DECODED,
                                                   VIDEO_EVENT,
                                                   i * 100,
                                                   /*frame_id*/ 0);
  }

  GetEventsAndReset();

  ASSERT_EQ(10u, frame_events_.size());
  EXPECT_EQ(100u, frame_events_.front()->relative_rtp_timestamp());
  EXPECT_EQ(1000u, frame_events_.back()->relative_rtp_timestamp());
}

TEST_F(EncodingEventSubscriberTest, PacketEventTruncating) {
  Init(AUDIO_EVENT);

  base::TimeTicks now(testing_clock_->NowTicks());

  // Entry with RTP timestamp 0 should get dropped.
  for (int i = 0; i < 11; i++) {
    cast_environment_->Logging()->InsertPacketEvent(now,
                                                    PACKET_RECEIVED,
                                                    AUDIO_EVENT,
                                                    /*rtp_timestamp*/ i * 100,
                                                    /*frame_id*/ 0,
                                                    /*packet_id*/ i,
                                                    /*max_packet_id*/ 10,
                                                    /*size*/ 123);
  }

  GetEventsAndReset();

  ASSERT_EQ(10u, packet_events_.size());
  EXPECT_EQ(100u, packet_events_.front()->relative_rtp_timestamp());
  EXPECT_EQ(1000u, packet_events_.back()->relative_rtp_timestamp());
}

TEST_F(EncodingEventSubscriberTest, EventFiltering) {
  Init(VIDEO_EVENT);

  base::TimeTicks now(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  cast_environment_->Logging()->InsertFrameEvent(now,
                                                 FRAME_DECODED,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp,
                                                 /*frame_id*/ 0);

  // This is an AUDIO_EVENT and shouldn't be processed by the subscriber.
  cast_environment_->Logging()->InsertFrameEvent(now,
                                                 FRAME_DECODED,
                                                 AUDIO_EVENT,
                                                 rtp_timestamp,
                                                 /*frame_id*/ 0);

  GetEventsAndReset();

  ASSERT_EQ(1u, frame_events_.size());
  FrameEventList::iterator it = frame_events_.begin();

  linked_ptr<AggregatedFrameEvent> frame_event = *it;

  ASSERT_EQ(1, frame_event->event_type_size());
  EXPECT_EQ(media::cast::proto::FRAME_DECODED,
            frame_event->event_type(0));

  GetEventsAndReset();

  EXPECT_TRUE(packet_events_.empty());
}

TEST_F(EncodingEventSubscriberTest, FrameEvent) {
  Init(VIDEO_EVENT);
  base::TimeTicks now(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  cast_environment_->Logging()->InsertFrameEvent(now, FRAME_DECODED,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp,
                                                 /*frame_id*/ 0);

  GetEventsAndReset();

  ASSERT_EQ(1u, frame_events_.size());

  RtpTimestamp relative_rtp_timestamp = rtp_timestamp - first_rtp_timestamp_;
  FrameEventList::iterator it = frame_events_.begin();

  linked_ptr<AggregatedFrameEvent> event = *it;

  EXPECT_EQ(relative_rtp_timestamp, event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->event_type_size());
  EXPECT_EQ(media::cast::proto::FRAME_DECODED, event->event_type(0));
  ASSERT_EQ(1, event->event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now), event->event_timestamp_ms(0));

  EXPECT_EQ(0, event->encoded_frame_size());
  EXPECT_EQ(0, event->delay_millis());

  GetEventsAndReset();
  EXPECT_TRUE(frame_events_.empty());
}

TEST_F(EncodingEventSubscriberTest, FrameEventDelay) {
  Init(AUDIO_EVENT);
  base::TimeTicks now(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  int delay_ms = 100;
  cast_environment_->Logging()->InsertFrameEventWithDelay(
      now, FRAME_PLAYOUT, AUDIO_EVENT, rtp_timestamp,
      /*frame_id*/ 0, base::TimeDelta::FromMilliseconds(delay_ms));

  GetEventsAndReset();

  ASSERT_EQ(1u, frame_events_.size());

  RtpTimestamp relative_rtp_timestamp = rtp_timestamp - first_rtp_timestamp_;
  FrameEventList::iterator it = frame_events_.begin();

  linked_ptr<AggregatedFrameEvent> event = *it;

  EXPECT_EQ(relative_rtp_timestamp, event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->event_type_size());
  EXPECT_EQ(media::cast::proto::FRAME_PLAYOUT, event->event_type(0));
  ASSERT_EQ(1, event->event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now), event->event_timestamp_ms(0));

  EXPECT_EQ(0, event->encoded_frame_size());
  EXPECT_EQ(100, event->delay_millis());
  EXPECT_FALSE(event->has_key_frame());
}

TEST_F(EncodingEventSubscriberTest, FrameEventSize) {
  Init(VIDEO_EVENT);
  base::TimeTicks now(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  int size = 123;
  bool key_frame = true;
  int target_bitrate = 1024;
  cast_environment_->Logging()->InsertEncodedFrameEvent(
      now, FRAME_ENCODED, VIDEO_EVENT, rtp_timestamp,
      /*frame_id*/ 0, size, key_frame, target_bitrate);

  GetEventsAndReset();

  ASSERT_EQ(1u, frame_events_.size());

  RtpTimestamp relative_rtp_timestamp = rtp_timestamp - first_rtp_timestamp_;
  FrameEventList::iterator it = frame_events_.begin();

  linked_ptr<AggregatedFrameEvent> event = *it;

  EXPECT_EQ(relative_rtp_timestamp, event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->event_type_size());
  EXPECT_EQ(media::cast::proto::FRAME_ENCODED, event->event_type(0));
  ASSERT_EQ(1, event->event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now), event->event_timestamp_ms(0));

  EXPECT_EQ(size, event->encoded_frame_size());
  EXPECT_EQ(0, event->delay_millis());
  EXPECT_TRUE(event->has_key_frame());
  EXPECT_EQ(key_frame, event->key_frame());
  EXPECT_EQ(target_bitrate, event->target_bitrate());
}

TEST_F(EncodingEventSubscriberTest, MultipleFrameEvents) {
  Init(AUDIO_EVENT);
  RtpTimestamp rtp_timestamp1 = 100;
  RtpTimestamp rtp_timestamp2 = 200;
  base::TimeTicks now1(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertFrameEventWithDelay(
      now1, FRAME_PLAYOUT, AUDIO_EVENT, rtp_timestamp1,
      /*frame_id*/ 0, /*delay*/ base::TimeDelta::FromMilliseconds(100));

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertEncodedFrameEvent(
      now2, FRAME_ENCODED, AUDIO_EVENT, rtp_timestamp2,
      /*frame_id*/ 0, /*size*/ 123, /* key_frame - unused */ false,
      /*target_bitrate - unused*/ 0);

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now3(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertFrameEvent(
      now3, FRAME_DECODED, AUDIO_EVENT, rtp_timestamp1, /*frame_id*/ 0);

  GetEventsAndReset();

  ASSERT_EQ(2u, frame_events_.size());

  RtpTimestamp relative_rtp_timestamp = rtp_timestamp1 - first_rtp_timestamp_;
  FrameEventList::iterator it = frame_events_.begin();

  linked_ptr<AggregatedFrameEvent> event = *it;

  EXPECT_EQ(relative_rtp_timestamp, event->relative_rtp_timestamp());

  ASSERT_EQ(2, event->event_type_size());
  EXPECT_EQ(media::cast::proto::FRAME_PLAYOUT, event->event_type(0));
  EXPECT_EQ(media::cast::proto::FRAME_DECODED, event->event_type(1));

  ASSERT_EQ(2, event->event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now1), event->event_timestamp_ms(0));
  EXPECT_EQ(InMilliseconds(now3), event->event_timestamp_ms(1));

  EXPECT_FALSE(event->has_key_frame());

  relative_rtp_timestamp = rtp_timestamp2 - first_rtp_timestamp_;
  ++it;

  event = *it;

  EXPECT_EQ(relative_rtp_timestamp, event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->event_type_size());
  EXPECT_EQ(media::cast::proto::FRAME_ENCODED, event->event_type(0));

  ASSERT_EQ(1, event->event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now2), event->event_timestamp_ms(0));

  EXPECT_FALSE(event->has_key_frame());
}

TEST_F(EncodingEventSubscriberTest, PacketEvent) {
  Init(AUDIO_EVENT);
  base::TimeTicks now(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  int packet_id = 2;
  int size = 100;
  cast_environment_->Logging()->InsertPacketEvent(
      now, PACKET_RECEIVED, AUDIO_EVENT,
      rtp_timestamp, /*frame_id*/ 0, packet_id,
      /*max_packet_id*/ 10, size);

  GetEventsAndReset();

  ASSERT_EQ(1u, packet_events_.size());

  RtpTimestamp relative_rtp_timestamp = rtp_timestamp - first_rtp_timestamp_;
  PacketEventList::iterator it = packet_events_.begin();

  linked_ptr<AggregatedPacketEvent> event = *it;

  EXPECT_EQ(relative_rtp_timestamp, event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->base_packet_event_size());
  const BasePacketEvent& base_event = event->base_packet_event(0);
  EXPECT_EQ(packet_id, base_event.packet_id());
  ASSERT_EQ(1, base_event.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_RECEIVED,
            base_event.event_type(0));
  ASSERT_EQ(1, base_event.event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now), base_event.event_timestamp_ms(0));
  EXPECT_EQ(size, base_event.size());

  GetEventsAndReset();
  EXPECT_TRUE(packet_events_.empty());
}

TEST_F(EncodingEventSubscriberTest, MultiplePacketEventsForPacket) {
  Init(VIDEO_EVENT);
  base::TimeTicks now1(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  int packet_id = 2;
  int size = 100;
  cast_environment_->Logging()->InsertPacketEvent(now1,
                                                  PACKET_SENT_TO_NETWORK,
                                                  VIDEO_EVENT,
                                                  rtp_timestamp,
                                                  /*frame_id*/ 0,
                                                  packet_id,
                                                  /*max_packet_id*/ 10,
                                                  size);

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertPacketEvent(now2,
                                                  PACKET_RETRANSMITTED,
                                                  VIDEO_EVENT,
                                                  rtp_timestamp,
                                                  /*frame_id*/ 0,
                                                  packet_id,
                                                  /*max_packet_id*/ 10,
                                                  size);

  GetEventsAndReset();

  ASSERT_EQ(1u, packet_events_.size());

  RtpTimestamp relative_rtp_timestamp = rtp_timestamp - first_rtp_timestamp_;
  PacketEventList::iterator it = packet_events_.begin();

  linked_ptr<AggregatedPacketEvent> event = *it;

  EXPECT_EQ(relative_rtp_timestamp, event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->base_packet_event_size());
  const BasePacketEvent& base_event = event->base_packet_event(0);
  EXPECT_EQ(packet_id, base_event.packet_id());
  ASSERT_EQ(2, base_event.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_SENT_TO_NETWORK,
            base_event.event_type(0));
  EXPECT_EQ(media::cast::proto::PACKET_RETRANSMITTED,
            base_event.event_type(1));
  ASSERT_EQ(2, base_event.event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now1), base_event.event_timestamp_ms(0));
  EXPECT_EQ(InMilliseconds(now2), base_event.event_timestamp_ms(1));
}

TEST_F(EncodingEventSubscriberTest, MultiplePacketEventsForFrame) {
  Init(VIDEO_EVENT);
  base::TimeTicks now1(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp = 100;
  int packet_id_1 = 2;
  int packet_id_2 = 3;
  int size = 100;
  cast_environment_->Logging()->InsertPacketEvent(now1,
                                                  PACKET_SENT_TO_NETWORK,
                                                  VIDEO_EVENT,
                                                  rtp_timestamp,
                                                  /*frame_id*/ 0,
                                                  packet_id_1,
                                                  /*max_packet_id*/ 10,
                                                  size);

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertPacketEvent(now2,
                                                  PACKET_RETRANSMITTED,
                                                  VIDEO_EVENT,
                                                  rtp_timestamp,
                                                  /*frame_id*/ 0,
                                                  packet_id_2,
                                                  /*max_packet_id*/ 10,
                                                  size);

  GetEventsAndReset();

  ASSERT_EQ(1u, packet_events_.size());

  RtpTimestamp relative_rtp_timestamp = rtp_timestamp - first_rtp_timestamp_;
  PacketEventList::iterator it = packet_events_.begin();

  linked_ptr<AggregatedPacketEvent> event = *it;

  EXPECT_EQ(relative_rtp_timestamp, event->relative_rtp_timestamp());

  ASSERT_EQ(2, event->base_packet_event_size());
  const BasePacketEvent& base_event = event->base_packet_event(0);
  EXPECT_EQ(packet_id_1, base_event.packet_id());
  ASSERT_EQ(1, base_event.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_SENT_TO_NETWORK,
            base_event.event_type(0));
  ASSERT_EQ(1, base_event.event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now1), base_event.event_timestamp_ms(0));

  const BasePacketEvent& base_event_2 = event->base_packet_event(1);
  EXPECT_EQ(packet_id_2, base_event_2.packet_id());
  ASSERT_EQ(1, base_event_2.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_RETRANSMITTED,
            base_event_2.event_type(0));
  ASSERT_EQ(1, base_event_2.event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now2), base_event_2.event_timestamp_ms(0));
}

TEST_F(EncodingEventSubscriberTest, MultiplePacketEvents) {
  Init(VIDEO_EVENT);
  base::TimeTicks now1(testing_clock_->NowTicks());
  RtpTimestamp rtp_timestamp_1 = 100;
  RtpTimestamp rtp_timestamp_2 = 200;
  int packet_id_1 = 2;
  int packet_id_2 = 3;
  int size = 100;
  cast_environment_->Logging()->InsertPacketEvent(now1,
                                                  PACKET_SENT_TO_NETWORK,
                                                  VIDEO_EVENT,
                                                  rtp_timestamp_1,
                                                  /*frame_id*/ 0,
                                                  packet_id_1,
                                                  /*max_packet_id*/ 10,
                                                  size);

  testing_clock_->Advance(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_->NowTicks());
  cast_environment_->Logging()->InsertPacketEvent(now2,
                                                  PACKET_RETRANSMITTED,
                                                  VIDEO_EVENT,
                                                  rtp_timestamp_2,
                                                  /*frame_id*/ 0,
                                                  packet_id_2,
                                                  /*max_packet_id*/ 10,
                                                  size);

  GetEventsAndReset();

  ASSERT_EQ(2u, packet_events_.size());

  RtpTimestamp relative_rtp_timestamp = rtp_timestamp_1 - first_rtp_timestamp_;
  PacketEventList::iterator it = packet_events_.begin();

  linked_ptr<AggregatedPacketEvent> event = *it;

  EXPECT_EQ(relative_rtp_timestamp, event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->base_packet_event_size());
  const BasePacketEvent& base_event = event->base_packet_event(0);
  EXPECT_EQ(packet_id_1, base_event.packet_id());
  ASSERT_EQ(1, base_event.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_SENT_TO_NETWORK,
            base_event.event_type(0));
  ASSERT_EQ(1, base_event.event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now1), base_event.event_timestamp_ms(0));

  relative_rtp_timestamp = rtp_timestamp_2 - first_rtp_timestamp_;
  ++it;
  ASSERT_TRUE(it != packet_events_.end());

  event = *it;
  EXPECT_EQ(relative_rtp_timestamp, event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->base_packet_event_size());
  const BasePacketEvent& base_event_2 = event->base_packet_event(0);
  EXPECT_EQ(packet_id_2, base_event_2.packet_id());
  ASSERT_EQ(1, base_event_2.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_RETRANSMITTED,
            base_event_2.event_type(0));
  ASSERT_EQ(1, base_event_2.event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now2), base_event_2.event_timestamp_ms(0));
}

TEST_F(EncodingEventSubscriberTest, FirstRtpTimestamp) {
  Init(VIDEO_EVENT);
  RtpTimestamp rtp_timestamp = 12345;
  base::TimeTicks now(testing_clock_->NowTicks());

  cast_environment_->Logging()->InsertFrameEvent(now,
                                                 FRAME_CAPTURE_BEGIN,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp,
                                                 /*frame_id*/ 0);

  cast_environment_->Logging()->InsertFrameEvent(now,
                                                 FRAME_CAPTURE_END,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp + 30,
                                                 /*frame_id*/ 1);

  GetEventsAndReset();

  EXPECT_EQ(rtp_timestamp, first_rtp_timestamp_);
  FrameEventList::iterator it = frame_events_.begin();
  ASSERT_NE(frame_events_.end(), it);
  EXPECT_EQ(0u, (*it)->relative_rtp_timestamp());

  ++it;
  ASSERT_NE(frame_events_.end(), it);
  EXPECT_EQ(30u, (*it)->relative_rtp_timestamp());

  rtp_timestamp = 67890;

  cast_environment_->Logging()->InsertFrameEvent(now,
                                                 FRAME_CAPTURE_BEGIN,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp,
                                                 /*frame_id*/ 0);
  GetEventsAndReset();

  EXPECT_EQ(rtp_timestamp, first_rtp_timestamp_);
}

TEST_F(EncodingEventSubscriberTest, RelativeRtpTimestampWrapAround) {
  Init(VIDEO_EVENT);
  RtpTimestamp rtp_timestamp = 0xffffffff - 20;
  base::TimeTicks now(testing_clock_->NowTicks());

  cast_environment_->Logging()->InsertFrameEvent(now,
                                                 FRAME_CAPTURE_BEGIN,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp,
                                                 /*frame_id*/ 0);

  // RtpTimestamp has now wrapped around.
  cast_environment_->Logging()->InsertFrameEvent(now,
                                                 FRAME_CAPTURE_END,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp + 30,
                                                 /*frame_id*/ 1);

  GetEventsAndReset();

  FrameEventList::iterator it = frame_events_.begin();
  ASSERT_NE(frame_events_.end(), it);
  EXPECT_EQ(0u, (*it)->relative_rtp_timestamp());

  ++it;
  ASSERT_NE(frame_events_.end(), it);
  EXPECT_EQ(30u, (*it)->relative_rtp_timestamp());
}

TEST_F(EncodingEventSubscriberTest, MaxEventsPerProto) {
  Init(VIDEO_EVENT);
  RtpTimestamp rtp_timestamp = 100;
  for (int i = 0; i < kMaxEventsPerProto + 1; i++) {
    cast_environment_->Logging()->InsertFrameEvent(testing_clock_->NowTicks(),
                                                   FRAME_ACK_RECEIVED,
                                                   VIDEO_EVENT,
                                                   rtp_timestamp,
                                                   /*frame_id*/ 0);
    testing_clock_->Advance(base::TimeDelta::FromMilliseconds(30));
  }

  GetEventsAndReset();

  ASSERT_EQ(2u, frame_events_.size());
  FrameEventList::iterator frame_it = frame_events_.begin();
  ASSERT_TRUE(frame_it != frame_events_.end());

  linked_ptr<AggregatedFrameEvent> frame_event = *frame_it;

  EXPECT_EQ(kMaxEventsPerProto, frame_event->event_type_size());

  for (int i = 0; i < kMaxPacketsPerFrame + 1; i++) {
    cast_environment_->Logging()->InsertPacketEvent(
        testing_clock_->NowTicks(),
        PACKET_SENT_TO_NETWORK,
        VIDEO_EVENT,
        rtp_timestamp,
        /*frame_id*/ 0,
        i,
        kMaxPacketsPerFrame,
        123);
    testing_clock_->Advance(base::TimeDelta::FromMilliseconds(30));
  }

  GetEventsAndReset();

  EXPECT_EQ(2u, packet_events_.size());

  PacketEventList::iterator packet_it = packet_events_.begin();
  ASSERT_TRUE(packet_it != packet_events_.end());

  linked_ptr<AggregatedPacketEvent> packet_event = *packet_it;

  EXPECT_EQ(kMaxPacketsPerFrame,
      packet_event->base_packet_event_size());

  ++packet_it;
  packet_event = *packet_it;
  EXPECT_EQ(1, packet_event->base_packet_event_size());

  for (int j = 0; j < kMaxEventsPerProto + 1; j++) {
    cast_environment_->Logging()->InsertPacketEvent(
        testing_clock_->NowTicks(),
        PACKET_SENT_TO_NETWORK,
        VIDEO_EVENT,
        rtp_timestamp,
        /*frame_id*/ 0,
        0,
        0,
        123);
    testing_clock_->Advance(base::TimeDelta::FromMilliseconds(30));
  }

  GetEventsAndReset();

  EXPECT_EQ(2u, packet_events_.size());
  packet_it = packet_events_.begin();
  ASSERT_TRUE(packet_it != packet_events_.end());

  packet_event = *packet_it;

  EXPECT_EQ(kMaxEventsPerProto,
      packet_event->base_packet_event(0).event_type_size());

  ++packet_it;
  packet_event = *packet_it;
  EXPECT_EQ(1, packet_event->base_packet_event(0).event_type_size());
}

}  // namespace cast
}  // namespace media
