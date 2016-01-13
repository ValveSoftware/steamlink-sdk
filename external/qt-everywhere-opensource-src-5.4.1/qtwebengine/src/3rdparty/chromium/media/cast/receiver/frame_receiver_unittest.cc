// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/receiver/frame_receiver.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/transport/pacing/mock_paced_packet_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace media {
namespace cast {

namespace {

const int kPacketSize = 1500;
const uint32 kFirstFrameId = 1234;
const int kPlayoutDelayMillis = 100;

class FakeFrameClient {
 public:
  FakeFrameClient() : num_called_(0) {}
  virtual ~FakeFrameClient() {}

  void AddExpectedResult(uint32 expected_frame_id,
                         const base::TimeTicks& expected_playout_time) {
    expected_results_.push_back(
        std::make_pair(expected_frame_id, expected_playout_time));
  }

  void DeliverEncodedFrame(scoped_ptr<transport::EncodedFrame> frame) {
    SCOPED_TRACE(::testing::Message() << "num_called_ is " << num_called_);
    ASSERT_FALSE(!frame)
        << "If at shutdown: There were unsatisfied requests enqueued.";
    ASSERT_FALSE(expected_results_.empty());
    EXPECT_EQ(expected_results_.front().first, frame->frame_id);
    EXPECT_EQ(expected_results_.front().second, frame->reference_time);
    expected_results_.pop_front();
    ++num_called_;
  }

  int number_times_called() const { return num_called_; }

 private:
  std::deque<std::pair<uint32, base::TimeTicks> > expected_results_;
  int num_called_;

  DISALLOW_COPY_AND_ASSIGN(FakeFrameClient);
};
}  // namespace

class FrameReceiverTest : public ::testing::Test {
 protected:
  FrameReceiverTest() {
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    start_time_ = testing_clock_->NowTicks();
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);

    cast_environment_ =
        new CastEnvironment(scoped_ptr<base::TickClock>(testing_clock_).Pass(),
                            task_runner_,
                            task_runner_,
                            task_runner_);
  }

  virtual ~FrameReceiverTest() {}

  virtual void SetUp() {
    payload_.assign(kPacketSize, 0);

    // Always start with a key frame.
    rtp_header_.is_key_frame = true;
    rtp_header_.frame_id = kFirstFrameId;
    rtp_header_.packet_id = 0;
    rtp_header_.max_packet_id = 0;
    rtp_header_.reference_frame_id = rtp_header_.frame_id;
    rtp_header_.rtp_timestamp = 0;
  }

  void CreateFrameReceiverOfAudio() {
    config_ = GetDefaultAudioReceiverConfig();
    config_.rtp_max_delay_ms = kPlayoutDelayMillis;

    receiver_.reset(new FrameReceiver(
        cast_environment_, config_, AUDIO_EVENT, &mock_transport_));
  }

  void CreateFrameReceiverOfVideo() {
    config_ = GetDefaultVideoReceiverConfig();
    config_.rtp_max_delay_ms = kPlayoutDelayMillis;
    // Note: Frame rate must divide 1000 without remainder so the test code
    // doesn't have to account for rounding errors.
    config_.max_frame_rate = 25;

    receiver_.reset(new FrameReceiver(
        cast_environment_, config_, VIDEO_EVENT, &mock_transport_));
  }

  void FeedOneFrameIntoReceiver() {
    // Note: For testing purposes, a frame consists of only a single packet.
    receiver_->ProcessParsedPacket(
        rtp_header_, payload_.data(), payload_.size());
  }

  void FeedLipSyncInfoIntoReceiver() {
    const base::TimeTicks now = testing_clock_->NowTicks();
    const int64 rtp_timestamp = (now - start_time_) *
        config_.frequency / base::TimeDelta::FromSeconds(1);
    CHECK_LE(0, rtp_timestamp);
    uint32 ntp_seconds;
    uint32 ntp_fraction;
    ConvertTimeTicksToNtp(now, &ntp_seconds, &ntp_fraction);
    TestRtcpPacketBuilder rtcp_packet;
    rtcp_packet.AddSrWithNtp(config_.incoming_ssrc,
                             ntp_seconds, ntp_fraction,
                             static_cast<uint32>(rtp_timestamp));
    ASSERT_TRUE(receiver_->ProcessPacket(rtcp_packet.GetPacket().Pass()));
  }

  FrameReceiverConfig config_;
  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  base::TimeTicks start_time_;
  transport::MockPacedPacketSender mock_transport_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  FakeFrameClient frame_client_;

  // Important for the FrameReceiver to be declared last, since its dependencies
  // must remain alive until after its destruction.
  scoped_ptr<FrameReceiver> receiver_;

  DISALLOW_COPY_AND_ASSIGN(FrameReceiverTest);
};

TEST_F(FrameReceiverTest, RejectsUnparsablePackets) {
  CreateFrameReceiverOfVideo();

  SimpleEventSubscriber event_subscriber;
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber);

  const bool success = receiver_->ProcessPacket(
      scoped_ptr<Packet>(new Packet(kPacketSize, 0xff)).Pass());
  EXPECT_FALSE(success);

  // Confirm no log events.
  std::vector<FrameEvent> frame_events;
  event_subscriber.GetFrameEventsAndReset(&frame_events);
  EXPECT_TRUE(frame_events.empty());
  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber);
}

TEST_F(FrameReceiverTest, ReceivesOneFrame) {
  CreateFrameReceiverOfAudio();

  SimpleEventSubscriber event_subscriber;
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber);

  EXPECT_CALL(mock_transport_, SendRtcpPacket(_, _))
      .WillRepeatedly(testing::Return(true));

  FeedLipSyncInfoIntoReceiver();
  task_runner_->RunTasks();

  // Enqueue a request for a frame.
  receiver_->RequestEncodedFrame(
      base::Bind(&FakeFrameClient::DeliverEncodedFrame,
                 base::Unretained(&frame_client_)));

  // The request should not be satisfied since no packets have been received.
  task_runner_->RunTasks();
  EXPECT_EQ(0, frame_client_.number_times_called());

  // Deliver one frame to the receiver and expect to get one frame back.
  const base::TimeDelta target_playout_delay =
      base::TimeDelta::FromMilliseconds(kPlayoutDelayMillis);
  frame_client_.AddExpectedResult(
      kFirstFrameId, testing_clock_->NowTicks() + target_playout_delay);
  FeedOneFrameIntoReceiver();
  task_runner_->RunTasks();
  EXPECT_EQ(1, frame_client_.number_times_called());

  // Was the frame logged?
  std::vector<FrameEvent> frame_events;
  event_subscriber.GetFrameEventsAndReset(&frame_events);
  ASSERT_TRUE(!frame_events.empty());
  EXPECT_EQ(FRAME_ACK_SENT, frame_events.begin()->type);
  EXPECT_EQ(AUDIO_EVENT, frame_events.begin()->media_type);
  EXPECT_EQ(rtp_header_.frame_id, frame_events.begin()->frame_id);
  EXPECT_EQ(rtp_header_.rtp_timestamp, frame_events.begin()->rtp_timestamp);
  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber);
}

TEST_F(FrameReceiverTest, ReceivesFramesSkippingWhenAppropriate) {
  CreateFrameReceiverOfAudio();

  SimpleEventSubscriber event_subscriber;
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber);

  EXPECT_CALL(mock_transport_, SendRtcpPacket(_, _))
      .WillRepeatedly(testing::Return(true));

  const uint32 rtp_advance_per_frame =
      config_.frequency / config_.max_frame_rate;
  const base::TimeDelta time_advance_per_frame =
      base::TimeDelta::FromSeconds(1) / config_.max_frame_rate;

  // Feed and process lip sync in receiver.
  FeedLipSyncInfoIntoReceiver();
  task_runner_->RunTasks();
  const base::TimeTicks first_frame_capture_time = testing_clock_->NowTicks();

  // Enqueue a request for a frame.
  const ReceiveEncodedFrameCallback frame_encoded_callback =
      base::Bind(&FakeFrameClient::DeliverEncodedFrame,
                 base::Unretained(&frame_client_));
  receiver_->RequestEncodedFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(0, frame_client_.number_times_called());

  // Receive one frame and expect to see the first request satisfied.
  const base::TimeDelta target_playout_delay =
      base::TimeDelta::FromMilliseconds(kPlayoutDelayMillis);
  frame_client_.AddExpectedResult(
      kFirstFrameId, first_frame_capture_time + target_playout_delay);
  rtp_header_.rtp_timestamp = 0;
  FeedOneFrameIntoReceiver();  // Frame 1
  task_runner_->RunTasks();
  EXPECT_EQ(1, frame_client_.number_times_called());

  // Enqueue a second request for a frame, but it should not be fulfilled yet.
  receiver_->RequestEncodedFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, frame_client_.number_times_called());

  // Receive one frame out-of-order: Make sure that we are not continuous and
  // that the RTP timestamp represents a time in the future.
  rtp_header_.frame_id = kFirstFrameId + 2;  // "Frame 3"
  rtp_header_.reference_frame_id = rtp_header_.frame_id;
  rtp_header_.rtp_timestamp += 2 * rtp_advance_per_frame;
  frame_client_.AddExpectedResult(
      kFirstFrameId + 2,
      first_frame_capture_time + 2 * time_advance_per_frame +
          target_playout_delay);
  FeedOneFrameIntoReceiver();  // Frame 3

  // Frame 2 should not come out at this point in time.
  task_runner_->RunTasks();
  EXPECT_EQ(1, frame_client_.number_times_called());

  // Enqueue a third request for a frame.
  receiver_->RequestEncodedFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, frame_client_.number_times_called());

  // Now, advance time forward such that the receiver is convinced it should
  // skip Frame 2.  Frame 3 is emitted (to satisfy the second request) because a
  // decision was made to skip over the no-show Frame 2.
  testing_clock_->Advance(2 * time_advance_per_frame + target_playout_delay);
  task_runner_->RunTasks();
  EXPECT_EQ(2, frame_client_.number_times_called());

  // Receive Frame 4 and expect it to fulfill the third request immediately.
  rtp_header_.frame_id = kFirstFrameId + 3;  // "Frame 4"
  rtp_header_.reference_frame_id = rtp_header_.frame_id;
  rtp_header_.rtp_timestamp += rtp_advance_per_frame;
  frame_client_.AddExpectedResult(
      kFirstFrameId + 3, first_frame_capture_time + 3 * time_advance_per_frame +
          target_playout_delay);
  FeedOneFrameIntoReceiver();    // Frame 4
  task_runner_->RunTasks();
  EXPECT_EQ(3, frame_client_.number_times_called());

  // Move forward to the playout time of an unreceived Frame 5.  Expect no
  // additional frames were emitted.
  testing_clock_->Advance(3 * time_advance_per_frame);
  task_runner_->RunTasks();
  EXPECT_EQ(3, frame_client_.number_times_called());

  // Were only non-skipped frames logged?
  std::vector<FrameEvent> frame_events;
  event_subscriber.GetFrameEventsAndReset(&frame_events);
  ASSERT_TRUE(!frame_events.empty());
  for (size_t i = 0; i < frame_events.size(); ++i) {
    EXPECT_EQ(FRAME_ACK_SENT, frame_events[i].type);
    EXPECT_EQ(AUDIO_EVENT, frame_events[i].media_type);
    EXPECT_LE(kFirstFrameId, frame_events[i].frame_id);
    EXPECT_GE(kFirstFrameId + 4, frame_events[i].frame_id);
    const int frame_offset = frame_events[i].frame_id - kFirstFrameId;
    EXPECT_NE(frame_offset, 1);  // Frame 2 never received.
    EXPECT_EQ(frame_offset * rtp_advance_per_frame,
              frame_events[i].rtp_timestamp);
  }
  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber);
}

TEST_F(FrameReceiverTest, ReceivesFramesRefusingToSkipAny) {
  CreateFrameReceiverOfVideo();

  SimpleEventSubscriber event_subscriber;
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber);

  EXPECT_CALL(mock_transport_, SendRtcpPacket(_, _))
      .WillRepeatedly(testing::Return(true));

  const uint32 rtp_advance_per_frame =
      config_.frequency / config_.max_frame_rate;
  const base::TimeDelta time_advance_per_frame =
      base::TimeDelta::FromSeconds(1) / config_.max_frame_rate;

  // Feed and process lip sync in receiver.
  FeedLipSyncInfoIntoReceiver();
  task_runner_->RunTasks();
  const base::TimeTicks first_frame_capture_time = testing_clock_->NowTicks();

  // Enqueue a request for a frame.
  const ReceiveEncodedFrameCallback frame_encoded_callback =
      base::Bind(&FakeFrameClient::DeliverEncodedFrame,
                 base::Unretained(&frame_client_));
  receiver_->RequestEncodedFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(0, frame_client_.number_times_called());

  // Receive one frame and expect to see the first request satisfied.
  const base::TimeDelta target_playout_delay =
      base::TimeDelta::FromMilliseconds(kPlayoutDelayMillis);
  frame_client_.AddExpectedResult(
      kFirstFrameId, first_frame_capture_time + target_playout_delay);
  rtp_header_.rtp_timestamp = 0;
  FeedOneFrameIntoReceiver();  // Frame 1
  task_runner_->RunTasks();
  EXPECT_EQ(1, frame_client_.number_times_called());

  // Enqueue a second request for a frame, but it should not be fulfilled yet.
  receiver_->RequestEncodedFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, frame_client_.number_times_called());

  // Receive one frame out-of-order: Make sure that we are not continuous and
  // that the RTP timestamp represents a time in the future.
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = kFirstFrameId + 2;  // "Frame 3"
  rtp_header_.reference_frame_id = kFirstFrameId + 1;  // "Frame 2"
  rtp_header_.rtp_timestamp += 2 * rtp_advance_per_frame;
  FeedOneFrameIntoReceiver();  // Frame 3

  // Frame 2 should not come out at this point in time.
  task_runner_->RunTasks();
  EXPECT_EQ(1, frame_client_.number_times_called());

  // Enqueue a third request for a frame.
  receiver_->RequestEncodedFrame(frame_encoded_callback);
  task_runner_->RunTasks();
  EXPECT_EQ(1, frame_client_.number_times_called());

  // Now, advance time forward such that Frame 2 is now too late for playback.
  // Regardless, the receiver must NOT emit Frame 3 yet because it is not
  // allowed to skip frames when dependencies are not satisfied.  In other
  // words, Frame 3 is not decodable without Frame 2.
  testing_clock_->Advance(2 * time_advance_per_frame + target_playout_delay);
  task_runner_->RunTasks();
  EXPECT_EQ(1, frame_client_.number_times_called());

  // Now receive Frame 2 and expect both the second and third requests to be
  // fulfilled immediately.
  frame_client_.AddExpectedResult(
      kFirstFrameId + 1,  // "Frame 2"
      first_frame_capture_time + 1 * time_advance_per_frame +
          target_playout_delay);
  frame_client_.AddExpectedResult(
      kFirstFrameId + 2,  // "Frame 3"
      first_frame_capture_time + 2 * time_advance_per_frame +
          target_playout_delay);
  --rtp_header_.frame_id;  // "Frame 2"
  --rtp_header_.reference_frame_id;  // "Frame 1"
  rtp_header_.rtp_timestamp -= rtp_advance_per_frame;
  FeedOneFrameIntoReceiver();  // Frame 2
  task_runner_->RunTasks();
  EXPECT_EQ(3, frame_client_.number_times_called());

  // Move forward to the playout time of an unreceived Frame 5.  Expect no
  // additional frames were emitted.
  testing_clock_->Advance(3 * time_advance_per_frame);
  task_runner_->RunTasks();
  EXPECT_EQ(3, frame_client_.number_times_called());

  // Sanity-check logging results.
  std::vector<FrameEvent> frame_events;
  event_subscriber.GetFrameEventsAndReset(&frame_events);
  ASSERT_TRUE(!frame_events.empty());
  for (size_t i = 0; i < frame_events.size(); ++i) {
    EXPECT_EQ(FRAME_ACK_SENT, frame_events[i].type);
    EXPECT_EQ(VIDEO_EVENT, frame_events[i].media_type);
    EXPECT_LE(kFirstFrameId, frame_events[i].frame_id);
    EXPECT_GE(kFirstFrameId + 3, frame_events[i].frame_id);
    const int frame_offset = frame_events[i].frame_id - kFirstFrameId;
    EXPECT_EQ(frame_offset * rtp_advance_per_frame,
              frame_events[i].rtp_timestamp);
  }
  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber);
}

}  // namespace cast
}  // namespace media
