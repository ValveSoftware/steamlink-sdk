// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/media.h"
#include "media/cast/audio_sender/audio_sender.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_sender_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class TestPacketSender : public transport::PacketSender {
 public:
  TestPacketSender() : number_of_rtp_packets_(0), number_of_rtcp_packets_(0) {}

  virtual bool SendPacket(transport::PacketRef packet,
                          const base::Closure& cb) OVERRIDE {
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

 private:
  int number_of_rtp_packets_;
  int number_of_rtcp_packets_;

  DISALLOW_COPY_AND_ASSIGN(TestPacketSender);
};

class AudioSenderTest : public ::testing::Test {
 protected:
  AudioSenderTest() {
    InitializeMediaLibraryForTesting();
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);
    cast_environment_ =
        new CastEnvironment(scoped_ptr<base::TickClock>(testing_clock_).Pass(),
                            task_runner_,
                            task_runner_,
                            task_runner_);
    audio_config_.codec = transport::kOpus;
    audio_config_.use_external_encoder = false;
    audio_config_.frequency = kDefaultAudioSamplingRate;
    audio_config_.channels = 2;
    audio_config_.bitrate = kDefaultAudioEncoderBitrate;
    audio_config_.rtp_config.payload_type = 127;

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
    audio_sender_.reset(new AudioSender(
        cast_environment_, audio_config_, transport_sender_.get()));
    task_runner_->RunTasks();
  }

  virtual ~AudioSenderTest() {}

  static void UpdateCastTransportStatus(transport::CastTransportStatus status) {
    EXPECT_EQ(transport::TRANSPORT_AUDIO_INITIALIZED, status);
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  TestPacketSender transport_;
  scoped_ptr<transport::CastTransportSenderImpl> transport_sender_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<AudioSender> audio_sender_;
  scoped_refptr<CastEnvironment> cast_environment_;
  AudioSenderConfig audio_config_;
};

TEST_F(AudioSenderTest, Encode20ms) {
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(20);
  scoped_ptr<AudioBus> bus(
      TestAudioBusFactory(audio_config_.channels,
                          audio_config_.frequency,
                          TestAudioBusFactory::kMiddleANoteFreq,
                          0.5f).NextAudioBus(kDuration));

  audio_sender_->InsertAudio(bus.Pass(), testing_clock_->NowTicks());
  task_runner_->RunTasks();
  EXPECT_LE(1, transport_.number_of_rtp_packets());
  EXPECT_LE(1, transport_.number_of_rtcp_packets());
}

TEST_F(AudioSenderTest, RtcpTimer) {
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(20);
  scoped_ptr<AudioBus> bus(
      TestAudioBusFactory(audio_config_.channels,
                          audio_config_.frequency,
                          TestAudioBusFactory::kMiddleANoteFreq,
                          0.5f).NextAudioBus(kDuration));

  audio_sender_->InsertAudio(bus.Pass(), testing_clock_->NowTicks());
  task_runner_->RunTasks();

  // Make sure that we send at least one RTCP packet.
  base::TimeDelta max_rtcp_timeout =
      base::TimeDelta::FromMilliseconds(1 + kDefaultRtcpIntervalMs * 3 / 2);
  testing_clock_->Advance(max_rtcp_timeout);
  task_runner_->RunTasks();
  EXPECT_LE(1, transport_.number_of_rtp_packets());
  EXPECT_LE(1, transport_.number_of_rtcp_packets());
}

}  // namespace cast
}  // namespace media
