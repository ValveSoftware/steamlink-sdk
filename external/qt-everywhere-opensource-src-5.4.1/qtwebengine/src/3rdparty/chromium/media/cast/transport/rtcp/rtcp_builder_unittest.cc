// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/transport/pacing/paced_sender.h"
#include "media/cast/transport/rtcp/rtcp_builder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

namespace {
static const uint32 kSendingSsrc = 0x12345678;
static const std::string kCName("test@10.1.1.1");
}  // namespace

class TestRtcpTransport : public PacedPacketSender {
 public:
  TestRtcpTransport()
      : expected_packet_length_(0),
        packet_count_(0) {
  }

  virtual bool SendRtcpPacket(const Packet& packet) OVERRIDE {
    EXPECT_EQ(expected_packet_length_, packet.size());
    EXPECT_EQ(0, memcmp(expected_packet_, &(packet[0]), packet.size()));
    packet_count_++;
    return true;
  }

  virtual bool SendPackets(const PacketList& packets) OVERRIDE {
    return false;
  }

  virtual bool ResendPackets(const PacketList& packets) OVERRIDE {
    return false;
  }

  void SetExpectedRtcpPacket(const uint8* rtcp_buffer, size_t length) {
    expected_packet_length_ = length;
    memcpy(expected_packet_, rtcp_buffer, length);
  }

  int packet_count() const { return packet_count_; }

 private:
  uint8 expected_packet_[kMaxIpPacketSize];
  size_t expected_packet_length_;
  int packet_count_;
};

class RtcpBuilderTest : public ::testing::Test {
 protected:
  RtcpBuilderTest()
      : task_runner_(new test::FakeSingleThreadTaskRunner(&testing_clock_)),
        cast_environment_(new CastEnvironment(&testing_clock_, task_runner_,
            task_runner_, task_runner_, task_runner_, task_runner_,
            GetDefaultCastSenderLoggingConfig())),
        rtcp_builder_(new RtcpBuilder(&test_transport_, kSendingSsrc, kCName)) {
  }

  base::SimpleTestTickClock testing_clock_;
  TestRtcpTransport test_transport_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<RtcpBuilder> rtcp_builder_;
};

TEST_F(RtcpBuilderTest, RtcpSenderReport) {
  RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = kNtpHigh;
  sender_info.ntp_fraction = kNtpLow;
  sender_info.rtp_timestamp = kRtpTimestamp;
  sender_info.send_packet_count = kSendPacketCount;
  sender_info.send_octet_count = kSendOctetCount;

  // Sender report + c_name.
  TestRtcpPacketBuilder p;
  p.AddSr(kSendingSsrc, 0);
  p.AddSdesCname(kSendingSsrc, kCName);
  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  rtcp_builder_->SendRtcpFromRtpSender(RtcpBuilder::kRtcpSr,
                                       &sender_info,
                                       NULL,
                                       NULL,
                                       kSendingSsrc,
                                       kCName);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpBuilderTest, RtcpSenderReportWithDlrr) {
  RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = kNtpHigh;
  sender_info.ntp_fraction = kNtpLow;
  sender_info.rtp_timestamp = kRtpTimestamp;
  sender_info.send_packet_count = kSendPacketCount;
  sender_info.send_octet_count = kSendOctetCount;

  // Sender report + c_name + dlrr.
  TestRtcpPacketBuilder p1;
  p1.AddSr(kSendingSsrc, 0);
  p1.AddSdesCname(kSendingSsrc, kCName);
  p1.AddXrHeader(kSendingSsrc);
  p1.AddXrDlrrBlock(kSendingSsrc);
  test_transport_.SetExpectedRtcpPacket(p1.Packet(), p1.Length());

  RtcpDlrrReportBlock dlrr_rb;
  dlrr_rb.last_rr = kLastRr;
  dlrr_rb.delay_since_last_rr = kDelayLastRr;

  rtcp_builder_->SendRtcpFromRtpSender(
      RtcpBuilder::kRtcpSr | RtcpBuilder::kRtcpDlrr,
      &sender_info,
      &dlrr_rb,
      NULL,
      kSendingSsrc,
      kCName);

  EXPECT_EQ(1, test_transport_.packet_count());
}

TEST_F(RtcpBuilderTest, RtcpSenderReportWithDlrr) {
  RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = kNtpHigh;
  sender_info.ntp_fraction = kNtpLow;
  sender_info.rtp_timestamp = kRtpTimestamp;
  sender_info.send_packet_count = kSendPacketCount;
  sender_info.send_octet_count = kSendOctetCount;

  // Sender report + c_name + dlrr + sender log.
  TestRtcpPacketBuilder p;
  p.AddSr(kSendingSsrc, 0);
  p.AddSdesCname(kSendingSsrc, kCName);
  p.AddXrHeader(kSendingSsrc);
  p.AddXrDlrrBlock(kSendingSsrc);

  test_transport_.SetExpectedRtcpPacket(p.Packet(), p.Length());

  RtcpDlrrReportBlock dlrr_rb;
  dlrr_rb.last_rr = kLastRr;
  dlrr_rb.delay_since_last_rr = kDelayLastRr;

  rtcp_builder_->SendRtcpFromRtpSender(
      RtcpBuilder::kRtcpSr | RtcpBuilder::kRtcpDlrr |
          RtcpBuilder::kRtcpSenderLog,
      &sender_info,
      &dlrr_rb,
      kSendingSsrc,
      kCName);

  EXPECT_EQ(1, test_transport_.packet_count());
}

}  // namespace cast
}  // namespace media
