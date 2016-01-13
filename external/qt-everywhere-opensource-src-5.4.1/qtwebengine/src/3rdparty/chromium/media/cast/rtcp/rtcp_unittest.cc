// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/rtcp/mock_rtcp_receiver_feedback.h"
#include "media/cast/rtcp/mock_rtcp_sender_feedback.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_sender_impl.h"
#include "media/cast/transport/pacing/paced_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

static const uint32 kSenderSsrc = 0x10203;
static const uint32 kReceiverSsrc = 0x40506;
static const std::string kCName("test@10.1.1.1");
static const uint32 kRtcpIntervalMs = 500;
static const int64 kAddedDelay = 123;
static const int64 kAddedShortDelay = 100;

class RtcpTestPacketSender : public transport::PacketSender {
 public:
  explicit RtcpTestPacketSender(base::SimpleTestTickClock* testing_clock)
      : drop_packets_(false),
        short_delay_(false),
        rtcp_receiver_(NULL),
        testing_clock_(testing_clock) {}
  virtual ~RtcpTestPacketSender() {}
  // Packet lists imply a RTP packet.
  void set_rtcp_receiver(Rtcp* rtcp) { rtcp_receiver_ = rtcp; }

  void set_short_delay() { short_delay_ = true; }

  void set_drop_packets(bool drop_packets) { drop_packets_ = drop_packets; }

  // A singular packet implies a RTCP packet.
  virtual bool SendPacket(transport::PacketRef packet,
                          const base::Closure& cb) OVERRIDE {
    if (short_delay_) {
      testing_clock_->Advance(
          base::TimeDelta::FromMilliseconds(kAddedShortDelay));
    } else {
      testing_clock_->Advance(base::TimeDelta::FromMilliseconds(kAddedDelay));
    }
    if (drop_packets_)
      return true;

    rtcp_receiver_->IncomingRtcpPacket(&packet->data[0], packet->data.size());
    return true;
  }

 private:
  bool drop_packets_;
  bool short_delay_;
  Rtcp* rtcp_receiver_;
  base::SimpleTestTickClock* testing_clock_;

  DISALLOW_COPY_AND_ASSIGN(RtcpTestPacketSender);
};

class LocalRtcpTransport : public transport::PacedPacketSender {
 public:
  LocalRtcpTransport(scoped_refptr<CastEnvironment> cast_environment,
                     base::SimpleTestTickClock* testing_clock)
      : drop_packets_(false),
        short_delay_(false),
        testing_clock_(testing_clock) {}

  void set_rtcp_receiver(Rtcp* rtcp) { rtcp_ = rtcp; }

  void set_short_delay() { short_delay_ = true; }

  void set_drop_packets(bool drop_packets) { drop_packets_ = drop_packets; }

  virtual bool SendRtcpPacket(uint32 ssrc,
                              transport::PacketRef packet) OVERRIDE {
    if (short_delay_) {
      testing_clock_->Advance(
          base::TimeDelta::FromMilliseconds(kAddedShortDelay));
    } else {
      testing_clock_->Advance(base::TimeDelta::FromMilliseconds(kAddedDelay));
    }
    if (drop_packets_)
      return true;

    rtcp_->IncomingRtcpPacket(&packet->data[0], packet->data.size());
    return true;
  }

  virtual bool SendPackets(
      const transport::SendPacketVector& packets) OVERRIDE {
    return false;
  }

  virtual bool ResendPackets(
      const transport::SendPacketVector& packets,
      base::TimeDelta dedupe_window) OVERRIDE {
    return false;
  }

  virtual void CancelSendingPacket(
      const transport::PacketKey& packet_key) OVERRIDE {
  }

 private:
  bool drop_packets_;
  bool short_delay_;
  Rtcp* rtcp_;
  base::SimpleTestTickClock* testing_clock_;
  scoped_refptr<CastEnvironment> cast_environment_;

  DISALLOW_COPY_AND_ASSIGN(LocalRtcpTransport);
};

class RtcpPeer : public Rtcp {
 public:
  RtcpPeer(scoped_refptr<CastEnvironment> cast_environment,
           RtcpSenderFeedback* sender_feedback,
           transport::CastTransportSender* const transport_sender,
           transport::PacedPacketSender* paced_packet_sender,
           RtpReceiverStatistics* rtp_receiver_statistics,
           RtcpMode rtcp_mode,
           const base::TimeDelta& rtcp_interval,
           uint32 local_ssrc,
           uint32 remote_ssrc,
           const std::string& c_name)
      : Rtcp(cast_environment,
             sender_feedback,
             transport_sender,
             paced_packet_sender,
             rtp_receiver_statistics,
             rtcp_mode,
             rtcp_interval,
             local_ssrc,
             remote_ssrc,
             c_name,
             AUDIO_EVENT) {}

  using Rtcp::OnReceivedNtp;
  using Rtcp::OnReceivedLipSyncInfo;
};

class RtcpTest : public ::testing::Test {
 protected:
  RtcpTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_)),
        sender_to_receiver_(testing_clock_),
        receiver_to_sender_(cast_environment_, testing_clock_) {
    testing_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    net::IPEndPoint dummy_endpoint;
    transport_sender_.reset(new transport::CastTransportSenderImpl(
        NULL,
        testing_clock_,
        dummy_endpoint,
        base::Bind(&UpdateCastTransportStatus),
        transport::BulkRawEventsCallback(),
        base::TimeDelta(),
        task_runner_,
        &sender_to_receiver_));
    transport::CastTransportAudioConfig config;
    config.rtp.config.ssrc = kSenderSsrc;
    config.rtp.max_outstanding_frames = 1;
    transport_sender_->InitializeAudio(config);
    EXPECT_CALL(mock_sender_feedback_, OnReceivedCastFeedback(_)).Times(0);
  }

  virtual ~RtcpTest() {}

  static void UpdateCastTransportStatus(transport::CastTransportStatus status) {
    bool result = (status == transport::TRANSPORT_AUDIO_INITIALIZED ||
                   status == transport::TRANSPORT_VIDEO_INITIALIZED);
    EXPECT_TRUE(result);
  }

  void RunTasks(int during_ms) {
    for (int i = 0; i < during_ms; ++i) {
      // Call process the timers every 1 ms.
      testing_clock_->Advance(base::TimeDelta::FromMilliseconds(1));
      task_runner_->RunTasks();
    }
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  RtcpTestPacketSender sender_to_receiver_;
  scoped_ptr<transport::CastTransportSenderImpl> transport_sender_;
  LocalRtcpTransport receiver_to_sender_;
  MockRtcpSenderFeedback mock_sender_feedback_;

  DISALLOW_COPY_AND_ASSIGN(RtcpTest);
};

TEST_F(RtcpTest, TimeToSend) {
  const base::TimeTicks start_time = testing_clock_->NowTicks();
  Rtcp rtcp(cast_environment_,
            &mock_sender_feedback_,
            transport_sender_.get(),
            &receiver_to_sender_,
            NULL,
            kRtcpCompound,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            kSenderSsrc,
            kReceiverSsrc,
            kCName,
            AUDIO_EVENT);
  receiver_to_sender_.set_rtcp_receiver(&rtcp);
  EXPECT_LE(start_time, rtcp.TimeToSendNextRtcpReport());
  EXPECT_GE(
      start_time + base::TimeDelta::FromMilliseconds(kRtcpIntervalMs * 3 / 2),
      rtcp.TimeToSendNextRtcpReport());
  base::TimeDelta delta = rtcp.TimeToSendNextRtcpReport() - start_time;
  testing_clock_->Advance(delta);
  EXPECT_EQ(testing_clock_->NowTicks(), rtcp.TimeToSendNextRtcpReport());
}

TEST_F(RtcpTest, BasicSenderReport) {
  Rtcp rtcp(cast_environment_,
            &mock_sender_feedback_,
            transport_sender_.get(),
            NULL,
            NULL,
            kRtcpCompound,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            kSenderSsrc,
            kReceiverSsrc,
            kCName,
            AUDIO_EVENT);
  sender_to_receiver_.set_rtcp_receiver(&rtcp);
  rtcp.SendRtcpFromRtpSender(base::TimeTicks(), 0);
}

TEST_F(RtcpTest, BasicReceiverReport) {
  Rtcp rtcp(cast_environment_,
            &mock_sender_feedback_,
            NULL,
            &receiver_to_sender_,
            NULL,
            kRtcpCompound,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            kSenderSsrc,
            kReceiverSsrc,
            kCName,
            AUDIO_EVENT);
  receiver_to_sender_.set_rtcp_receiver(&rtcp);
  rtcp.SendRtcpFromRtpReceiver(NULL, NULL);
}

TEST_F(RtcpTest, BasicCast) {
  EXPECT_CALL(mock_sender_feedback_, OnReceivedCastFeedback(_)).Times(1);

  // Media receiver.
  Rtcp rtcp(cast_environment_,
            &mock_sender_feedback_,
            NULL,
            &receiver_to_sender_,
            NULL,
            kRtcpReducedSize,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            kSenderSsrc,
            kSenderSsrc,
            kCName,
            AUDIO_EVENT);
  receiver_to_sender_.set_rtcp_receiver(&rtcp);
  RtcpCastMessage cast_message(kSenderSsrc);
  cast_message.ack_frame_id_ = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets_[kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets_[kFrameIdWithLostPackets] =
      missing_packets;
  rtcp.SendRtcpFromRtpReceiver(&cast_message, NULL);
}

TEST_F(RtcpTest, RttReducedSizeRtcp) {
  // Media receiver.
  Rtcp rtcp_receiver(cast_environment_,
                     &mock_sender_feedback_,
                     NULL,
                     &receiver_to_sender_,
                     NULL,
                     kRtcpReducedSize,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     kReceiverSsrc,
                     kSenderSsrc,
                     kCName,
                     AUDIO_EVENT);

  // Media sender.
  Rtcp rtcp_sender(cast_environment_,
                   &mock_sender_feedback_,
                   transport_sender_.get(),
                   NULL,
                   NULL,
                   kRtcpReducedSize,
                   base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                   kSenderSsrc,
                   kReceiverSsrc,
                   kCName,
                   AUDIO_EVENT);

  sender_to_receiver_.set_rtcp_receiver(&rtcp_receiver);
  receiver_to_sender_.set_rtcp_receiver(&rtcp_sender);

  base::TimeDelta rtt;
  base::TimeDelta avg_rtt;
  base::TimeDelta min_rtt;
  base::TimeDelta max_rtt;
  EXPECT_FALSE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_FALSE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));

  rtcp_sender.SendRtcpFromRtpSender(testing_clock_->NowTicks(), 1);
  RunTasks(33);
  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_FALSE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 2);
  rtcp_sender.SendRtcpFromRtpSender(testing_clock_->NowTicks(), 2);
  RunTasks(33);
  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));

  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 2);
}

TEST_F(RtcpTest, Rtt) {
  // Media receiver.
  Rtcp rtcp_receiver(cast_environment_,
                     &mock_sender_feedback_,
                     NULL,
                     &receiver_to_sender_,
                     NULL,
                     kRtcpCompound,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     kReceiverSsrc,
                     kSenderSsrc,
                     kCName,
                     AUDIO_EVENT);

  // Media sender.
  Rtcp rtcp_sender(cast_environment_,
                   &mock_sender_feedback_,
                   transport_sender_.get(),
                   NULL,
                   NULL,
                   kRtcpCompound,
                   base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                   kSenderSsrc,
                   kReceiverSsrc,
                   kCName,
                   AUDIO_EVENT);

  receiver_to_sender_.set_rtcp_receiver(&rtcp_sender);
  sender_to_receiver_.set_rtcp_receiver(&rtcp_receiver);

  base::TimeDelta rtt;
  base::TimeDelta avg_rtt;
  base::TimeDelta min_rtt;
  base::TimeDelta max_rtt;
  EXPECT_FALSE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_FALSE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));

  rtcp_sender.SendRtcpFromRtpSender(testing_clock_->NowTicks(), 1);
  RunTasks(33);
  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);

  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  RunTasks(33);

  EXPECT_FALSE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  RunTasks(33);

  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 2);

  rtcp_sender.SendRtcpFromRtpSender(testing_clock_->NowTicks(), 2);
  RunTasks(33);
  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 2);

  receiver_to_sender_.set_short_delay();
  sender_to_receiver_.set_short_delay();
  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_NEAR(kAddedDelay + kAddedShortDelay, rtt.InMilliseconds(), 2);
  EXPECT_NEAR(
      (kAddedShortDelay + 3 * kAddedDelay) / 2, avg_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(kAddedDelay + kAddedShortDelay, min_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 2);

  rtcp_sender.SendRtcpFromRtpSender(testing_clock_->NowTicks(), 3);
  RunTasks(33);
  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_NEAR(2 * kAddedShortDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR((2 * kAddedShortDelay + 2 * kAddedDelay) / 2,
              avg_rtt.InMilliseconds(),
              1);
  EXPECT_NEAR(2 * kAddedShortDelay, min_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 2);

  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_NEAR(2 * kAddedShortDelay, rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedShortDelay, min_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 2);

  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_NEAR(2 * kAddedShortDelay, rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedShortDelay, min_rtt.InMilliseconds(), 2);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 2);
}

TEST_F(RtcpTest, RttWithPacketLoss) {
  // Media receiver.
  Rtcp rtcp_receiver(cast_environment_,
                     &mock_sender_feedback_,
                     NULL,
                     &receiver_to_sender_,
                     NULL,
                     kRtcpReducedSize,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     kReceiverSsrc,
                     kSenderSsrc,
                     kCName,
                     AUDIO_EVENT);

  // Media sender.
  Rtcp rtcp_sender(cast_environment_,
                   &mock_sender_feedback_,
                   transport_sender_.get(),
                   NULL,
                   NULL,
                   kRtcpReducedSize,
                   base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                   kSenderSsrc,
                   kReceiverSsrc,
                   kCName,
                   AUDIO_EVENT);

  receiver_to_sender_.set_rtcp_receiver(&rtcp_sender);
  sender_to_receiver_.set_rtcp_receiver(&rtcp_receiver);

  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  rtcp_sender.SendRtcpFromRtpSender(testing_clock_->NowTicks(), 0);
  RunTasks(33);

  base::TimeDelta rtt;
  base::TimeDelta avg_rtt;
  base::TimeDelta min_rtt;
  base::TimeDelta max_rtt;
  EXPECT_FALSE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);

  receiver_to_sender_.set_short_delay();
  sender_to_receiver_.set_short_delay();
  receiver_to_sender_.set_drop_packets(true);

  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  rtcp_sender.SendRtcpFromRtpSender(testing_clock_->NowTicks(), 1);
  RunTasks(33);

  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_NEAR(kAddedDelay + kAddedShortDelay, rtt.InMilliseconds(), 2);
}

TEST_F(RtcpTest, NtpAndTime) {
  const int64 kSecondsbetweenYear1900and2010 = INT64_C(40176 * 24 * 60 * 60);
  const int64 kSecondsbetweenYear1900and2030 = INT64_C(47481 * 24 * 60 * 60);

  uint32 ntp_seconds_1 = 0;
  uint32 ntp_fraction_1 = 0;
  base::TimeTicks input_time = base::TimeTicks::Now();
  ConvertTimeTicksToNtp(input_time, &ntp_seconds_1, &ntp_fraction_1);

  // Verify absolute value.
  EXPECT_GT(ntp_seconds_1, kSecondsbetweenYear1900and2010);
  EXPECT_LT(ntp_seconds_1, kSecondsbetweenYear1900and2030);

  base::TimeTicks out_1 = ConvertNtpToTimeTicks(ntp_seconds_1, ntp_fraction_1);
  EXPECT_EQ(input_time, out_1);  // Verify inverse.

  base::TimeDelta time_delta = base::TimeDelta::FromMilliseconds(1000);
  input_time += time_delta;

  uint32 ntp_seconds_2 = 0;
  uint32 ntp_fraction_2 = 0;

  ConvertTimeTicksToNtp(input_time, &ntp_seconds_2, &ntp_fraction_2);
  base::TimeTicks out_2 = ConvertNtpToTimeTicks(ntp_seconds_2, ntp_fraction_2);
  EXPECT_EQ(input_time, out_2);  // Verify inverse.

  // Verify delta.
  EXPECT_EQ((out_2 - out_1), time_delta);
  EXPECT_EQ((ntp_seconds_2 - ntp_seconds_1), UINT32_C(1));
  EXPECT_NEAR(ntp_fraction_2, ntp_fraction_1, 1);

  time_delta = base::TimeDelta::FromMilliseconds(500);
  input_time += time_delta;

  uint32 ntp_seconds_3 = 0;
  uint32 ntp_fraction_3 = 0;

  ConvertTimeTicksToNtp(input_time, &ntp_seconds_3, &ntp_fraction_3);
  base::TimeTicks out_3 = ConvertNtpToTimeTicks(ntp_seconds_3, ntp_fraction_3);
  EXPECT_EQ(input_time, out_3);  // Verify inverse.

  // Verify delta.
  EXPECT_EQ((out_3 - out_2), time_delta);
  EXPECT_NEAR((ntp_fraction_3 - ntp_fraction_2), 0xffffffff / 2, 1);
}

}  // namespace cast
}  // namespace media
