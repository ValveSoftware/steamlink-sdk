// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/fake_video_encode_accelerator.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_sender_impl.h"
#include "media/cast/transport/pacing/paced_sender.h"
#include "media/cast/video_sender/video_sender.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {
static const uint8 kPixelValue = 123;
static const int kWidth = 320;
static const int kHeight = 240;

using testing::_;
using testing::AtLeast;

void CreateVideoEncodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    scoped_ptr<VideoEncodeAccelerator> fake_vea,
    const ReceiveVideoEncodeAcceleratorCallback& callback) {
  callback.Run(task_runner, fake_vea.Pass());
}

void CreateSharedMemory(
    size_t size, const ReceiveVideoEncodeMemoryCallback& callback) {
  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAndMapAnonymous(size)) {
    NOTREACHED();
    return;
  }
  callback.Run(shm.Pass());
}

class TestPacketSender : public transport::PacketSender {
 public:
  TestPacketSender()
      : number_of_rtp_packets_(0),
        number_of_rtcp_packets_(0),
        paused_(false) {}

  // A singular packet implies a RTCP packet.
  virtual bool SendPacket(transport::PacketRef packet,
                          const base::Closure& cb) OVERRIDE {
    if (paused_) {
      stored_packet_ = packet;
      callback_ = cb;
      return false;
    }
    if (Rtcp::IsRtcpPacket(&packet->data[0], packet->data.size())) {
      ++number_of_rtcp_packets_;
    } else {
      // Check that at least one RTCP packet was sent before the first RTP
      // packet.  This confirms that the receiver will have the necessary lip
      // sync info before it has to calculate the playout time of the first
      // frame.
      if (number_of_rtp_packets_ == 0)
        EXPECT_LE(1, number_of_rtcp_packets_);
      ++number_of_rtp_packets_;
    }
    return true;
  }

  int number_of_rtp_packets() const { return number_of_rtp_packets_; }

  int number_of_rtcp_packets() const { return number_of_rtcp_packets_; }

  void SetPause(bool paused) {
    paused_ = paused;
    if (!paused && stored_packet_) {
      SendPacket(stored_packet_, callback_);
      callback_.Run();
    }
  }

 private:
  int number_of_rtp_packets_;
  int number_of_rtcp_packets_;
  bool paused_;
  base::Closure callback_;
  transport::PacketRef stored_packet_;

  DISALLOW_COPY_AND_ASSIGN(TestPacketSender);
};

class PeerVideoSender : public VideoSender {
 public:
  PeerVideoSender(
      scoped_refptr<CastEnvironment> cast_environment,
      const VideoSenderConfig& video_config,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
      const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
      transport::CastTransportSender* const transport_sender)
      : VideoSender(cast_environment,
                    video_config,
                    create_vea_cb,
                    create_video_encode_mem_cb,
                    transport_sender) {}
  using VideoSender::OnReceivedCastFeedback;
};
}  // namespace

class VideoSenderTest : public ::testing::Test {
 protected:
  VideoSenderTest() {
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);
    cast_environment_ =
        new CastEnvironment(scoped_ptr<base::TickClock>(testing_clock_).Pass(),
                            task_runner_,
                            task_runner_,
                            task_runner_);
    last_pixel_value_ = kPixelValue;
    net::IPEndPoint dummy_endpoint;
    transport_sender_.reset(new transport::CastTransportSenderImpl(
        NULL,
        testing_clock_,
        dummy_endpoint,
        base::Bind(&UpdateCastTransportStatus),
        transport::BulkRawEventsCallback(),
        base::TimeDelta(),
        task_runner_,
        &transport_));
  }

  virtual ~VideoSenderTest() {}

  virtual void TearDown() OVERRIDE {
    video_sender_.reset();
    task_runner_->RunTasks();
  }

  static void UpdateCastTransportStatus(transport::CastTransportStatus status) {
    EXPECT_EQ(transport::TRANSPORT_VIDEO_INITIALIZED, status);
  }

  void InitEncoder(bool external) {
    VideoSenderConfig video_config;
    video_config.rtp_config.ssrc = 1;
    video_config.incoming_feedback_ssrc = 2;
    video_config.rtcp_c_name = "video_test@10.1.1.1";
    video_config.rtp_config.payload_type = 127;
    video_config.use_external_encoder = external;
    video_config.width = kWidth;
    video_config.height = kHeight;
    video_config.max_bitrate = 5000000;
    video_config.min_bitrate = 1000000;
    video_config.start_bitrate = 1000000;
    video_config.max_qp = 56;
    video_config.min_qp = 0;
    video_config.max_frame_rate = 30;
    video_config.max_number_of_video_buffers_used = 1;
    video_config.codec = transport::kVp8;

    if (external) {
      scoped_ptr<VideoEncodeAccelerator> fake_vea(
          new test::FakeVideoEncodeAccelerator(task_runner_));
      video_sender_.reset(
          new PeerVideoSender(cast_environment_,
                              video_config,
                              base::Bind(&CreateVideoEncodeAccelerator,
                                         task_runner_,
                                         base::Passed(&fake_vea)),
                              base::Bind(&CreateSharedMemory),
                              transport_sender_.get()));
    } else {
      video_sender_.reset(
          new PeerVideoSender(cast_environment_,
                              video_config,
                              CreateDefaultVideoEncodeAcceleratorCallback(),
                              CreateDefaultVideoEncodeMemoryCallback(),
                              transport_sender_.get()));
    }
    ASSERT_EQ(STATUS_VIDEO_INITIALIZED, video_sender_->InitializationResult());
  }

  scoped_refptr<media::VideoFrame> GetNewVideoFrame() {
    gfx::Size size(kWidth, kHeight);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(
            VideoFrame::I420, size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(video_frame, last_pixel_value_++);
    return video_frame;
  }

  scoped_refptr<media::VideoFrame> GetLargeNewVideoFrame() {
    gfx::Size size(kWidth, kHeight);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(
            VideoFrame::I420, size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrameWithNoise(video_frame);
    return video_frame;
  }

  void RunTasks(int during_ms) {
    task_runner_->Sleep(base::TimeDelta::FromMilliseconds(during_ms));
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  TestPacketSender transport_;
  scoped_ptr<transport::CastTransportSenderImpl> transport_sender_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<PeerVideoSender> video_sender_;
  scoped_refptr<CastEnvironment> cast_environment_;
  int last_pixel_value_;

  DISALLOW_COPY_AND_ASSIGN(VideoSenderTest);
};

TEST_F(VideoSenderTest, BuiltInEncoder) {
  InitEncoder(false);
  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  const base::TimeTicks capture_time = testing_clock_->NowTicks();
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  task_runner_->RunTasks();
  EXPECT_LE(1, transport_.number_of_rtp_packets());
  EXPECT_LE(1, transport_.number_of_rtcp_packets());
}

TEST_F(VideoSenderTest, ExternalEncoder) {
  InitEncoder(true);
  task_runner_->RunTasks();

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  const base::TimeTicks capture_time = testing_clock_->NowTicks();
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  task_runner_->RunTasks();

  // We need to run the task to cleanup the GPU instance.
  video_sender_.reset(NULL);
  task_runner_->RunTasks();
}

TEST_F(VideoSenderTest, RtcpTimer) {
  InitEncoder(false);

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  const base::TimeTicks capture_time = testing_clock_->NowTicks();
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  // Make sure that we send at least one RTCP packet.
  base::TimeDelta max_rtcp_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtcpIntervalMs * 3 / 2);

  RunTasks(max_rtcp_timeout.InMilliseconds());
  EXPECT_LE(1, transport_.number_of_rtp_packets());
  EXPECT_LE(1, transport_.number_of_rtcp_packets());
  // Build Cast msg and expect RTCP packet.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;
  video_sender_->OnReceivedCastFeedback(cast_feedback);
  RunTasks(max_rtcp_timeout.InMilliseconds());
  EXPECT_LE(1, transport_.number_of_rtcp_packets());
}

TEST_F(VideoSenderTest, ResendTimer) {
  InitEncoder(false);

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  const base::TimeTicks capture_time = testing_clock_->NowTicks();
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  // ACK the key frame.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;
  video_sender_->OnReceivedCastFeedback(cast_feedback);

  video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, capture_time);

  base::TimeDelta max_resend_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtpMaxDelayMs);

  // Make sure that we do a re-send.
  RunTasks(max_resend_timeout.InMilliseconds());
  // Should have sent at least 3 packets.
  EXPECT_LE(
      3,
      transport_.number_of_rtp_packets() + transport_.number_of_rtcp_packets());
}

TEST_F(VideoSenderTest, LogAckReceivedEvent) {
  InitEncoder(false);
  SimpleEventSubscriber event_subscriber;
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber);

  int num_frames = 10;
  for (int i = 0; i < num_frames; i++) {
    scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

    const base::TimeTicks capture_time = testing_clock_->NowTicks();
    video_sender_->InsertRawVideoFrame(video_frame, capture_time);
    RunTasks(33);
  }

  task_runner_->RunTasks();

  RtcpCastMessage cast_feedback(1);
  cast_feedback.ack_frame_id_ = num_frames - 1;

  video_sender_->OnReceivedCastFeedback(cast_feedback);

  std::vector<FrameEvent> frame_events;
  event_subscriber.GetFrameEventsAndReset(&frame_events);

  ASSERT_TRUE(!frame_events.empty());
  EXPECT_EQ(FRAME_ACK_RECEIVED, frame_events.rbegin()->type);
  EXPECT_EQ(VIDEO_EVENT, frame_events.rbegin()->media_type);
  EXPECT_EQ(num_frames - 1u, frame_events.rbegin()->frame_id);

  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber);
}

TEST_F(VideoSenderTest, StopSendingInTheAbsenceOfAck) {
  InitEncoder(false);
  // Send a stream of frames and don't ACK; by default we shouldn't have more
  // than 4 frames in flight.
  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_->NowTicks());
  RunTasks(33);

  // Send 3 more frames and record the number of packets sent.
  for (int i = 0; i < 3; ++i) {
    scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
    video_sender_->InsertRawVideoFrame(video_frame, testing_clock_->NowTicks());
    RunTasks(33);
  }
  const int number_of_packets_sent = transport_.number_of_rtp_packets();

  // Send 3 more frames - they should not be encoded, as we have not received
  // any acks.
  for (int i = 0; i < 3; ++i) {
    scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
    video_sender_->InsertRawVideoFrame(video_frame, testing_clock_->NowTicks());
    RunTasks(33);
  }

  // We expect a frame to be retransmitted because of duplicated ACKs.
  // Only one packet of the frame is re-transmitted.
  EXPECT_EQ(number_of_packets_sent + 1,
            transport_.number_of_rtp_packets());

  // Start acking and make sure we're back to steady-state.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;
  video_sender_->OnReceivedCastFeedback(cast_feedback);
  EXPECT_LE(
      4,
      transport_.number_of_rtp_packets() + transport_.number_of_rtcp_packets());

  // Empty the pipeline.
  RunTasks(100);
  // Should have sent at least 7 packets.
  EXPECT_LE(
      7,
      transport_.number_of_rtp_packets() + transport_.number_of_rtcp_packets());
}

TEST_F(VideoSenderTest, DuplicateAckRetransmit) {
  InitEncoder(false);
  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_->NowTicks());
  RunTasks(33);
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;

  // Send 3 more frames but don't ACK.
  for (int i = 0; i < 3; ++i) {
    scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
    video_sender_->InsertRawVideoFrame(video_frame, testing_clock_->NowTicks());
    RunTasks(33);
  }
  const int number_of_packets_sent = transport_.number_of_rtp_packets();

  // Send duplicated ACKs and mix some invalid NACKs.
  for (int i = 0; i < 10; ++i) {
    RtcpCastMessage ack_feedback(1);
    ack_feedback.media_ssrc_ = 2;
    ack_feedback.ack_frame_id_ = 0;
    RtcpCastMessage nack_feedback(1);
    nack_feedback.media_ssrc_ = 2;
    nack_feedback.missing_frames_and_packets_[255] = PacketIdSet();
    video_sender_->OnReceivedCastFeedback(ack_feedback);
    video_sender_->OnReceivedCastFeedback(nack_feedback);
  }
  EXPECT_EQ(number_of_packets_sent, transport_.number_of_rtp_packets());

  // Re-transmit one packet because of duplicated ACKs.
  for (int i = 0; i < 3; ++i) {
    RtcpCastMessage ack_feedback(1);
    ack_feedback.media_ssrc_ = 2;
    ack_feedback.ack_frame_id_ = 0;
    video_sender_->OnReceivedCastFeedback(ack_feedback);
  }
  EXPECT_EQ(number_of_packets_sent + 1, transport_.number_of_rtp_packets());
}

TEST_F(VideoSenderTest, DuplicateAckRetransmitDoesNotCancelRetransmits) {
  InitEncoder(false);
  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_->NowTicks());
  RunTasks(33);
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;

  // Send 2 more frames but don't ACK.
  for (int i = 0; i < 2; ++i) {
    scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
    video_sender_->InsertRawVideoFrame(video_frame, testing_clock_->NowTicks());
    RunTasks(33);
  }
  // Pause the transport
  transport_.SetPause(true);

  // Insert one more video frame.
  video_frame = GetLargeNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_->NowTicks());
  RunTasks(33);

  const int number_of_packets_sent = transport_.number_of_rtp_packets();

  // Send duplicated ACKs and mix some invalid NACKs.
  for (int i = 0; i < 10; ++i) {
    RtcpCastMessage ack_feedback(1);
    ack_feedback.media_ssrc_ = 2;
    ack_feedback.ack_frame_id_ = 0;
    RtcpCastMessage nack_feedback(1);
    nack_feedback.media_ssrc_ = 2;
    nack_feedback.missing_frames_and_packets_[255] = PacketIdSet();
    video_sender_->OnReceivedCastFeedback(ack_feedback);
    video_sender_->OnReceivedCastFeedback(nack_feedback);
  }
  EXPECT_EQ(number_of_packets_sent, transport_.number_of_rtp_packets());

  // Re-transmit one packet because of duplicated ACKs.
  for (int i = 0; i < 3; ++i) {
    RtcpCastMessage ack_feedback(1);
    ack_feedback.media_ssrc_ = 2;
    ack_feedback.ack_frame_id_ = 0;
    video_sender_->OnReceivedCastFeedback(ack_feedback);
  }

  transport_.SetPause(false);
  RunTasks(100);
  EXPECT_LT(number_of_packets_sent + 1, transport_.number_of_rtp_packets());
}

TEST_F(VideoSenderTest, AcksCancelRetransmits) {
  InitEncoder(false);
  transport_.SetPause(true);
  scoped_refptr<media::VideoFrame> video_frame = GetLargeNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_->NowTicks());
  RunTasks(33);

  // Frame should be in buffer, waiting. Now let's ack it.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;
  video_sender_->OnReceivedCastFeedback(cast_feedback);

  transport_.SetPause(false);
  RunTasks(33);
  EXPECT_EQ(0, transport_.number_of_rtp_packets());
}

TEST_F(VideoSenderTest, NAcksCancelRetransmits) {
  InitEncoder(false);
  transport_.SetPause(true);
  // Send two video frames.
  scoped_refptr<media::VideoFrame> video_frame = GetLargeNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_->NowTicks());
  RunTasks(33);
  video_frame = GetLargeNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_->NowTicks());
  RunTasks(33);

  // Frames should be in buffer, waiting. Now let's ack the first one and nack
  // one packet in the second one.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.media_ssrc_ = 2;
  cast_feedback.ack_frame_id_ = 0;
  PacketIdSet missing_packets;
  missing_packets.insert(0);
  cast_feedback.missing_frames_and_packets_[1] = missing_packets;
  video_sender_->OnReceivedCastFeedback(cast_feedback);

  transport_.SetPause(false);
  RunTasks(33);
  // Only one packet should be retransmitted.
  EXPECT_EQ(1, transport_.number_of_rtp_packets());
}

}  // namespace cast
}  // namespace media
