// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A very simple packet builder class for building RTCP packets.
// Used for testing only.
#ifndef MEDIA_CAST_RTCP_TEST_RTCP_PACKET_BUILDER_H_
#define MEDIA_CAST_RTCP_TEST_RTCP_PACKET_BUILDER_H_

#include "base/big_endian.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

// These values are arbitrary only for the purpose of testing.

namespace {
// Sender report.
static const int kNtpHigh = 0x01020304;
static const int kNtpLow = 0x05060708;
static const int kRtpTimestamp = 0x10203040;
static const int kSendPacketCount = 987;
static const int kSendOctetCount = 87654;

// Report block.
static const int kLoss = 0x01000123;
static const int kExtendedMax = 0x15678;
static const int kTestJitter = 0x10203;
static const int kLastSr = 0x34561234;
static const int kDelayLastSr = 1000;

// DLRR block.
static const int kLastRr = 0x34561234;
static const int kDelayLastRr = 1000;

// REMB.
static const int kTestRembBitrate = 52428;

// RPSI.
static const int kPayloadtype = 126;
static const uint64 kPictureId = 0x1234567890;

// NACK.
static const int kMissingPacket = 34567;

// CAST.
static const uint32 kAckFrameId = 17;
static const uint32 kLostFrameId = 18;
static const uint32 kFrameIdWithLostPackets = 19;
static const int kLostPacketId1 = 3;
static const int kLostPacketId2 = 5;
static const int kLostPacketId3 = 12;
}  // namespace

class TestRtcpPacketBuilder {
 public:
  TestRtcpPacketBuilder();

  void AddSr(uint32 sender_ssrc, int number_of_report_blocks);
  void AddSrWithNtp(uint32 sender_ssrc,
                    uint32 ntp_high,
                    uint32 ntp_low,
                    uint32 rtp_timestamp);
  void AddRr(uint32 sender_ssrc, int number_of_report_blocks);
  void AddRb(uint32 rtp_ssrc);
  void AddSdesCname(uint32 sender_ssrc, const std::string& c_name);

  void AddXrHeader(uint32 sender_ssrc);
  void AddXrDlrrBlock(uint32 sender_ssrc);
  void AddXrExtendedDlrrBlock(uint32 sender_ssrc);
  void AddXrRrtrBlock();
  void AddXrUnknownBlock();

  void AddNack(uint32 sender_ssrc, uint32 media_ssrc);
  void AddSendReportRequest(uint32 sender_ssrc, uint32 media_ssrc);

  void AddPli(uint32 sender_ssrc, uint32 media_ssrc);
  void AddRpsi(uint32 sender_ssrc, uint32 media_ssrc);
  void AddRemb(uint32 sender_ssrc, uint32 media_ssrc);
  void AddCast(uint32 sender_ssrc, uint32 media_ssrc, uint16 target_delay_ms);
  void AddReceiverLog(uint32 sender_ssrc);
  void AddReceiverFrameLog(uint32 rtp_timestamp,
                           int num_events,
                           uint32 event_timesamp_base);
  void AddReceiverEventLog(uint16 event_data,
                           CastLoggingEvent event,
                           uint16 event_timesamp_delta);

  scoped_ptr<Packet> GetPacket();
  const uint8* Data();
  int Length() { return kMaxIpPacketSize - big_endian_writer_.remaining(); }

 private:
  void AddRtcpHeader(int payload, int format_or_count);
  void PatchLengthField();

  // Where the length field of the current packet is.
  // Note: 0 is not a legal value, it is used for "uninitialized".
  uint8 buffer_[kMaxIpPacketSize];
  char* ptr_of_length_;
  base::BigEndianWriter big_endian_writer_;

  DISALLOW_COPY_AND_ASSIGN(TestRtcpPacketBuilder);
};

}  // namespace cast
}  // namespace media

#endif  //  MEDIA_CAST_RTCP_TEST_RTCP_PACKET_BUILDER_H_
