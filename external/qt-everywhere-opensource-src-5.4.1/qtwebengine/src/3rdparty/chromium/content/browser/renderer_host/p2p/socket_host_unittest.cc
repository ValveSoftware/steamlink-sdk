// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/p2p/socket_host_test_utils.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

static unsigned char kFakeTag[4] = { 0xba, 0xdd, 0xba, 0xdd };
static unsigned char kTestKey[] = "12345678901234567890";
static unsigned char kTestAstValue[3] = { 0xaa, 0xbb, 0xcc };

// Rtp message with invalid length.
static unsigned char kRtpMsgWithInvalidLength[] = {
  0x94, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xAA, 0xBB, 0xCC, 0XDD,  // SSRC
  0xDD, 0xCC, 0xBB, 0xAA   // Only 1 CSRC, but CC count is 4.
};

// Rtp message with single byte header extension, invalid extension length.
static unsigned char kRtpMsgWithInvalidExtnLength[] = {
  0x90, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xBE, 0xDE, 0x0A, 0x00   // Extn length - 0x0A00
};

// Valid rtp Message with 2 byte header extension.
static unsigned char kRtpMsgWith2ByteExtnHeader[] = {
  0x90, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xAA, 0xBB, 0xCC, 0XDD,  // SSRC
  0x10, 0x00, 0x00, 0x01,  // 2 Byte header extension
  0x01, 0x00, 0x00, 0x00
};

// Stun Indication message with Zero length
static unsigned char kTurnSendIndicationMsgWithNoAttributes[] = {
  0x00, 0x16, 0x00, 0x00,  // Zero length
  0x21, 0x12, 0xA4, 0x42,  // magic cookie
  '0', '1', '2', '3',      // transaction id
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
};

// Stun Send Indication message with invalid length in stun header.
static unsigned char kTurnSendIndicationMsgWithInvalidLength[] = {
  0x00, 0x16, 0xFF, 0x00,  // length of 0xFF00
  0x21, 0x12, 0xA4, 0x42,  // magic cookie
  '0', '1', '2', '3',      // transaction id
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
};

// Stun Send Indication message with no DATA attribute in message.
static unsigned char kTurnSendIndicatinMsgWithNoDataAttribute[] = {
  0x00, 0x16, 0x00, 0x08,  // length of
  0x21, 0x12, 0xA4, 0x42,  // magic cookie
  '0', '1', '2', '3',      // transaction id
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
  0x00, 0x20, 0x00, 0x04,  // Mapped address.
  0x00, 0x00, 0x00, 0x00
};

// A valid STUN indication message with a valid RTP header in data attribute
// payload field and no extension bit set.
static unsigned char kTurnSendIndicationMsgWithoutRtpExtension[] = {
  0x00, 0x16, 0x00, 0x18,  // length of
  0x21, 0x12, 0xA4, 0x42,  // magic cookie
  '0', '1', '2', '3',      // transaction id
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
  0x00, 0x20, 0x00, 0x04,  // Mapped address.
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x13, 0x00, 0x0C,  // Data attribute.
  0x80, 0x00, 0x00, 0x00,  // RTP packet.
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
};

// A valid STUN indication message with a valid RTP header and a extension
// header.
static unsigned char kTurnSendIndicationMsgWithAbsSendTimeExtension[] = {
  0x00, 0x16, 0x00, 0x24,  // length of
  0x21, 0x12, 0xA4, 0x42,  // magic cookie
  '0', '1', '2', '3',      // transaction id
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
  0x00, 0x20, 0x00, 0x04,  // Mapped address.
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x13, 0x00, 0x18,  // Data attribute.
  0x90, 0x00, 0x00, 0x00,  // RTP packet.
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xBE, 0xDE, 0x00, 0x02,
  0x22, 0xaa, 0xbb, 0xcc,
  0x32, 0xaa, 0xbb, 0xcc,
};

// A valid TURN channel header, but not RTP packet.
static unsigned char kTurnChannelMsgNoRtpPacket[] = {
  0x40, 0x00, 0x00, 0x04,
  0xaa, 0xbb, 0xcc, 0xdd,
};

// Turn ChannelMessage with zero length of payload.
static unsigned char kTurnChannelMsgWithZeroLength[] = {
  0x40, 0x00, 0x00, 0x00
};

// Turn ChannelMessage, wrapping a RTP packet without extension.
static unsigned char kTurnChannelMsgWithRtpPacket[] = {
  0x40, 0x00, 0x00, 0x0C,
  0x80, 0x00, 0x00, 0x00,  // RTP packet.
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
};

// Turn ChannelMessage, wrapping a RTP packet with AbsSendTime Extension.
static unsigned char kTurnChannelMsgWithAbsSendTimeExtension[] = {
  0x40, 0x00, 0x00, 0x14,
  0x90, 0x00, 0x00, 0x00,  // RTP packet.
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xBE, 0xDE, 0x00, 0x01,
  0x32, 0xaa, 0xbb, 0xcc,
};

// RTP packet with single byte extension header of length 4 bytes.
// Extension id = 3 and length = 3
static unsigned char kRtpMsgWithAbsSendTimeExtension[] = {
  0x90, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xBE, 0xDE, 0x00, 0x02,
  0x22, 0x00, 0x02, 0x1c,
  0x32, 0xaa, 0xbb, 0xcc,
};

// Index of AbsSendTimeExtn data in message |kRtpMsgWithAbsSendTimeExtension|.
static const int kAstIndexInRtpMsg = 21;

namespace content {

// This test verifies parsing of all invalid raw packets.
TEST(P2PSocketHostTest, TestInvalidRawRtpMessages) {
  int start_pos = INT_MAX, rtp_length = INT_MAX;
  EXPECT_FALSE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kRtpMsgWithInvalidLength),
      sizeof(kRtpMsgWithInvalidLength),
      &start_pos, &rtp_length));
  EXPECT_EQ(INT_MAX, start_pos);
  EXPECT_EQ(INT_MAX, rtp_length);

  EXPECT_FALSE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kRtpMsgWithInvalidExtnLength),
      sizeof(kRtpMsgWithInvalidExtnLength),
      &start_pos, &rtp_length));
  EXPECT_EQ(INT_MAX, start_pos);
  EXPECT_EQ(INT_MAX, rtp_length);
}

// Verify invalid TURN send indication messages. Messages are proper STUN
// messages with incorrect values in attributes.
TEST(P2PSocketHostTest, TestInvalidTurnSendIndicationMessages) {
  // Initializing out params to very large value.
  int start_pos = INT_MAX, rtp_length = INT_MAX;
  EXPECT_FALSE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kTurnSendIndicationMsgWithNoAttributes),
      sizeof(kTurnSendIndicationMsgWithNoAttributes),
      &start_pos, &rtp_length));
  EXPECT_EQ(INT_MAX, start_pos);
  EXPECT_EQ(INT_MAX, rtp_length);

  EXPECT_FALSE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kTurnSendIndicationMsgWithInvalidLength),
      sizeof(kTurnSendIndicationMsgWithInvalidLength),
      &start_pos, &rtp_length));
  EXPECT_EQ(INT_MAX, start_pos);
  EXPECT_EQ(INT_MAX, rtp_length);

  EXPECT_FALSE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kTurnSendIndicatinMsgWithNoDataAttribute),
      sizeof(kTurnSendIndicatinMsgWithNoDataAttribute),
      &start_pos, &rtp_length));
  EXPECT_EQ(INT_MAX, start_pos);
  EXPECT_EQ(INT_MAX, rtp_length);
}

// This test verifies incorrectly formed TURN channel messages.
TEST(P2PSocketHostTest, TestInvalidTurnChannelMessages) {
  int start_pos = INT_MAX, rtp_length = INT_MAX;
  EXPECT_FALSE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kTurnChannelMsgNoRtpPacket),
      sizeof(kTurnChannelMsgNoRtpPacket),
      &start_pos, &rtp_length));
  EXPECT_EQ(INT_MAX, start_pos);
  EXPECT_EQ(INT_MAX, rtp_length);

  EXPECT_FALSE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kTurnChannelMsgWithZeroLength),
      sizeof(kTurnChannelMsgWithZeroLength),
      &start_pos, &rtp_length));
  EXPECT_EQ(INT_MAX, start_pos);
  EXPECT_EQ(INT_MAX, rtp_length);
}

// This test verifies parsing of a valid RTP packet which has 2byte header
// extension instead of a 1 byte header extension.
TEST(P2PSocketHostTest, TestValid2ByteExtnHdrRtpMessage) {
  int start_pos = INT_MAX, rtp_length = INT_MAX;
  EXPECT_TRUE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kRtpMsgWith2ByteExtnHeader),
      sizeof(kRtpMsgWith2ByteExtnHeader),
      &start_pos, &rtp_length));
  EXPECT_EQ(20, rtp_length);
  EXPECT_EQ(0, start_pos);
}

// This test verifies parsing of a valid RTP packet which has 1 byte header
// AbsSendTime extension in it.
TEST(P2PSocketHostTest, TestValidRtpPacketWithAbsSendTimeExtension) {
  int start_pos = INT_MAX, rtp_length = INT_MAX;
  EXPECT_TRUE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kRtpMsgWithAbsSendTimeExtension),
      sizeof(kRtpMsgWithAbsSendTimeExtension),
      &start_pos, &rtp_length));
  EXPECT_EQ(24, rtp_length);
  EXPECT_EQ(0, start_pos);
}

// This test verifies parsing of a valid TURN Send Indication messages.
TEST(P2PSocketHostTest, TestValidTurnSendIndicationMessages) {
  int start_pos = INT_MAX, rtp_length = INT_MAX;
  EXPECT_TRUE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kTurnSendIndicationMsgWithoutRtpExtension),
      sizeof(kTurnSendIndicationMsgWithoutRtpExtension),
      &start_pos, &rtp_length));
  EXPECT_EQ(12, rtp_length);
  EXPECT_EQ(32, start_pos);

  start_pos = INT_MAX, rtp_length = INT_MAX;
  EXPECT_TRUE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kTurnSendIndicationMsgWithAbsSendTimeExtension),
      sizeof(kTurnSendIndicationMsgWithAbsSendTimeExtension),
      &start_pos, &rtp_length));
  EXPECT_EQ(24, rtp_length);
  EXPECT_EQ(32, start_pos);
}

// This test verifies parsing of valid TURN Channel Messages.
TEST(P2PSocketHostTest, TestValidTurnChannelMessages) {
  int start_pos = -1, rtp_length = -1;
  EXPECT_TRUE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kTurnChannelMsgWithRtpPacket),
      sizeof(kTurnChannelMsgWithRtpPacket), &start_pos, &rtp_length));
  EXPECT_EQ(12, rtp_length);
  EXPECT_EQ(4, start_pos);

  start_pos = -1, rtp_length = -1;
  EXPECT_TRUE(packet_processing_helpers::GetRtpPacketStartPositionAndLength(
      reinterpret_cast<char*>(kTurnChannelMsgWithAbsSendTimeExtension),
      sizeof(kTurnChannelMsgWithAbsSendTimeExtension),
      &start_pos, &rtp_length));
  EXPECT_EQ(20, rtp_length);
  EXPECT_EQ(4, start_pos);
}

// Verify handling of a 2 byte extension header RTP messsage. Currently we don't
// handle this kind of message.
TEST(P2PSocketHostTest, TestUpdateAbsSendTimeExtensionIn2ByteHeaderExtn) {
  EXPECT_FALSE(packet_processing_helpers::UpdateRtpAbsSendTimeExtn(
    reinterpret_cast<char*>(kRtpMsgWith2ByteExtnHeader),
    sizeof(kRtpMsgWith2ByteExtnHeader), 3, 0));
}

// Verify finding an extension ID in the TURN send indication message.
TEST(P2PSocketHostTest, TestUpdateAbsSendTimeExtensionInTurnSendIndication) {
  EXPECT_TRUE(packet_processing_helpers::UpdateRtpAbsSendTimeExtn(
      reinterpret_cast<char*>(kTurnSendIndicationMsgWithoutRtpExtension),
      sizeof(kTurnSendIndicationMsgWithoutRtpExtension), 3, 0));

  EXPECT_TRUE(packet_processing_helpers::UpdateRtpAbsSendTimeExtn(
      reinterpret_cast<char*>(kTurnSendIndicationMsgWithAbsSendTimeExtension),
      sizeof(kTurnSendIndicationMsgWithAbsSendTimeExtension), 3, 0));
}

// Test without any packet options variables set. This method should return
// without HMAC value in the packet.
TEST(P2PSocketHostTest, TestApplyPacketOptionsWithDefaultValues) {
  unsigned char fake_tag[4] = { 0xba, 0xdd, 0xba, 0xdd };
  talk_base::PacketOptions options;
  std::vector<char> rtp_packet;
  rtp_packet.resize(sizeof(kRtpMsgWithAbsSendTimeExtension) + 4);  // tag length
  memcpy(&rtp_packet[0], kRtpMsgWithAbsSendTimeExtension,
         sizeof(kRtpMsgWithAbsSendTimeExtension));
  memcpy(&rtp_packet[sizeof(kRtpMsgWithAbsSendTimeExtension)], fake_tag, 4);
  EXPECT_TRUE(
      packet_processing_helpers::ApplyPacketOptions(
      &rtp_packet[0], rtp_packet.size(), options, 0));
  // Making sure we have't updated the HMAC.
  EXPECT_EQ(0, memcmp(&rtp_packet[sizeof(kRtpMsgWithAbsSendTimeExtension)],
                      fake_tag, 4));

  // Verify AbsouluteSendTime extension field is not modified.
  EXPECT_EQ(0, memcmp(&rtp_packet[kAstIndexInRtpMsg],
                      kTestAstValue, sizeof(kTestAstValue)));
}

// Veirfy HMAC is updated when packet option parameters are set.
TEST(P2PSocketHostTest, TestApplyPacketOptionsWithAuthParams) {
  talk_base::PacketOptions options;
  options.packet_time_params.srtp_auth_key.resize(20);
  options.packet_time_params.srtp_auth_key.assign(
      kTestKey, kTestKey + sizeof(kTestKey));
  options.packet_time_params.srtp_auth_tag_len = 4;

  std::vector<char> rtp_packet;
  rtp_packet.resize(sizeof(kRtpMsgWithAbsSendTimeExtension) + 4);  // tag length
  memcpy(&rtp_packet[0], kRtpMsgWithAbsSendTimeExtension,
         sizeof(kRtpMsgWithAbsSendTimeExtension));
  memcpy(&rtp_packet[sizeof(kRtpMsgWithAbsSendTimeExtension)], kFakeTag, 4);
  EXPECT_TRUE(packet_processing_helpers::ApplyPacketOptions(
                  &rtp_packet[0], rtp_packet.size(), options, 0));
  // HMAC should be different from fake_tag.
  EXPECT_NE(0, memcmp(&rtp_packet[sizeof(kRtpMsgWithAbsSendTimeExtension)],
                      kFakeTag, sizeof(kFakeTag)));

   // Verify AbsouluteSendTime extension field is not modified.
  EXPECT_EQ(0, memcmp(&rtp_packet[kAstIndexInRtpMsg],
                      kTestAstValue, sizeof(kTestAstValue)));
}

// Verify finding an extension ID in a raw rtp message.
TEST(P2PSocketHostTest, TestUpdateAbsSendTimeExtensionInRtpPacket) {
  EXPECT_TRUE(packet_processing_helpers::UpdateRtpAbsSendTimeExtn(
      reinterpret_cast<char*>(kRtpMsgWithAbsSendTimeExtension),
      sizeof(kRtpMsgWithAbsSendTimeExtension), 3, 0));
}

// Verify we update both AbsSendTime extension header and HMAC.
TEST(P2PSocketHostTest, TestApplyPacketOptionsWithAuthParamsAndAbsSendTime) {
  talk_base::PacketOptions options;
  options.packet_time_params.srtp_auth_key.resize(20);
  options.packet_time_params.srtp_auth_key.assign(
      kTestKey, kTestKey + sizeof(kTestKey));
  options.packet_time_params.srtp_auth_tag_len = 4;
  options.packet_time_params.rtp_sendtime_extension_id = 3;
  // 3 is also present in the test message.

  std::vector<char> rtp_packet;
  rtp_packet.resize(sizeof(kRtpMsgWithAbsSendTimeExtension) + 4);  // tag length
  memcpy(&rtp_packet[0], kRtpMsgWithAbsSendTimeExtension,
         sizeof(kRtpMsgWithAbsSendTimeExtension));
  memcpy(&rtp_packet[sizeof(kRtpMsgWithAbsSendTimeExtension)], kFakeTag, 4);
  EXPECT_TRUE(packet_processing_helpers::ApplyPacketOptions(
                  &rtp_packet[0], rtp_packet.size(), options, 0xccbbaa));
  // HMAC should be different from fake_tag.
  EXPECT_NE(0, memcmp(&rtp_packet[sizeof(kRtpMsgWithAbsSendTimeExtension)],
                      kFakeTag, sizeof(kFakeTag)));

  // ApplyPackets should have the new timestamp passed as input.
  unsigned char timestamp_array[3] = { 0xcc, 0xbb, 0xaa };
  EXPECT_EQ(0, memcmp(&rtp_packet[kAstIndexInRtpMsg],
                      timestamp_array, sizeof(timestamp_array)));
}

}  // namespace content
