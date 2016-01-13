// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/rtcp/rtcp_builder.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/logging.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {
namespace transport {

RtcpBuilder::RtcpBuilder(PacedSender* const outgoing_transport)
    : transport_(outgoing_transport),
      ssrc_(0) {
}

RtcpBuilder::~RtcpBuilder() {}

void RtcpBuilder::SendRtcpFromRtpSender(
    uint32 packet_type_flags,
    const RtcpSenderInfo& sender_info,
    const RtcpDlrrReportBlock& dlrr,
    uint32 sending_ssrc,
    const std::string& c_name) {
  if (packet_type_flags & kRtcpRr ||
      packet_type_flags & kRtcpPli ||
      packet_type_flags & kRtcpRrtr ||
      packet_type_flags & kRtcpCast ||
      packet_type_flags & kRtcpReceiverLog ||
      packet_type_flags & kRtcpRpsi ||
      packet_type_flags & kRtcpRemb ||
      packet_type_flags & kRtcpNack) {
    NOTREACHED() << "Invalid argument";
  }
  ssrc_ = sending_ssrc;
  c_name_ = c_name;
  PacketRef packet(new base::RefCountedData<Packet>);
  packet->data.reserve(kMaxIpPacketSize);
  if (packet_type_flags & kRtcpSr) {
    if (!BuildSR(sender_info, &packet->data)) return;
    if (!BuildSdec(&packet->data)) return;
  }
  if (packet_type_flags & kRtcpBye) {
    if (!BuildBye(&packet->data)) return;
  }
  if (packet_type_flags & kRtcpDlrr) {
    if (!BuildDlrrRb(dlrr, &packet->data)) return;
  }
  if (packet->data.empty())
    return;  // Sanity - don't send empty packets.

  transport_->SendRtcpPacket(ssrc_, packet);
}

bool RtcpBuilder::BuildSR(const RtcpSenderInfo& sender_info,
                          Packet* packet) const {
  // Sender report.
  size_t start_size = packet->size();
  if (start_size + 52 > kMaxIpPacketSize) {
    DLOG(FATAL) << "Not enough buffer space";
    return false;
  }

  uint16 number_of_rows = 6;
  packet->resize(start_size + 28);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 28);
  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(kPacketTypeSenderReport);
  big_endian_writer.WriteU16(number_of_rows);
  big_endian_writer.WriteU32(ssrc_);
  big_endian_writer.WriteU32(sender_info.ntp_seconds);
  big_endian_writer.WriteU32(sender_info.ntp_fraction);
  big_endian_writer.WriteU32(sender_info.rtp_timestamp);
  big_endian_writer.WriteU32(sender_info.send_packet_count);
  big_endian_writer.WriteU32(static_cast<uint32>(sender_info.send_octet_count));
  return true;
}

bool RtcpBuilder::BuildSdec(Packet* packet) const {
  size_t start_size = packet->size();
  if (start_size + 12 + c_name_.length() > kMaxIpPacketSize) {
    DLOG(FATAL) << "Not enough buffer space";
    return false;
  }

  // SDES Source Description.
  packet->resize(start_size + 10);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 10);
  // We always need to add one SDES CNAME.
  big_endian_writer.WriteU8(0x80 + 1);
  big_endian_writer.WriteU8(kPacketTypeSdes);

  // Handle SDES length later on.
  uint32 sdes_length_position = static_cast<uint32>(start_size) + 3;
  big_endian_writer.WriteU16(0);
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(1);  // CNAME = 1
  big_endian_writer.WriteU8(static_cast<uint8>(c_name_.length()));

  size_t sdes_length = 10 + c_name_.length();
  packet->insert(packet->end(), c_name_.c_str(),
                 c_name_.c_str() + c_name_.length());

  size_t padding = 0;

  // We must have a zero field even if we have an even multiple of 4 bytes.
  if ((packet->size() % 4) == 0) {
    padding++;
    packet->push_back(0);
  }
  while ((packet->size() % 4) != 0) {
    padding++;
    packet->push_back(0);
  }
  sdes_length += padding;

  // In 32-bit words minus one and we don't count the header.
  uint8 buffer_length = static_cast<uint8>((sdes_length / 4) - 1);
  (*packet)[sdes_length_position] = buffer_length;
  return true;
}

bool RtcpBuilder::BuildBye(Packet* packet) const {
  size_t start_size = packet->size();
  if (start_size + 8 > kMaxIpPacketSize) {
    DLOG(FATAL) << "Not enough buffer space";
    return false;
  }

  packet->resize(start_size + 8);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 8);
  big_endian_writer.WriteU8(0x80 + 1);
  big_endian_writer.WriteU8(kPacketTypeBye);
  big_endian_writer.WriteU16(1);  // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  return true;
}

/*
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |V=2|P|reserved |   PT=XR=207   |             length            |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                              SSRC                             |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     BT=5      |   reserved    |         block length          |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  |                 SSRC_1 (SSRC of first receiver)               | sub-
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
  |                         last RR (LRR)                         |   1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                   delay since last RR (DLRR)                  |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/
bool RtcpBuilder::BuildDlrrRb(const RtcpDlrrReportBlock& dlrr,
                              Packet* packet) const {
  size_t start_size = packet->size();
  if (start_size + 24 > kMaxIpPacketSize) {
    DLOG(FATAL) << "Not enough buffer space";
    return false;
  }

  packet->resize(start_size + 24);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 24);
  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(kPacketTypeXr);
  big_endian_writer.WriteU16(5);  // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(5);  // Add block type.
  big_endian_writer.WriteU8(0);  // Add reserved.
  big_endian_writer.WriteU16(3);  // Block length.
  big_endian_writer.WriteU32(ssrc_);  // Add the media (received RTP) SSRC.
  big_endian_writer.WriteU32(dlrr.last_rr);
  big_endian_writer.WriteU32(dlrr.delay_since_last_rr);
  return true;
}

}  // namespace transport
}  // namespace cast
}  // namespace media
