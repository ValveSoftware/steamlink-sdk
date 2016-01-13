// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtcp/rtcp_sender.h"

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/big_endian.h"
#include "base/logging.h"
#include "media/cast/cast_environment.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {
namespace {

// Max delta is 4095 milliseconds because we need to be able to encode it in
// 12 bits.
const int64 kMaxWireFormatTimeDeltaMs = INT64_C(0xfff);

uint16 MergeEventTypeAndTimestampForWireFormat(
    const CastLoggingEvent& event,
    const base::TimeDelta& time_delta) {
  int64 time_delta_ms = time_delta.InMilliseconds();

  DCHECK_GE(time_delta_ms, 0);
  DCHECK_LE(time_delta_ms, kMaxWireFormatTimeDeltaMs);

  uint16 time_delta_12_bits =
      static_cast<uint16>(time_delta_ms & kMaxWireFormatTimeDeltaMs);

  uint16 event_type_4_bits = ConvertEventTypeToWireFormat(event);
  DCHECK(event_type_4_bits);
  DCHECK(~(event_type_4_bits & 0xfff0));
  return (event_type_4_bits << 12) | time_delta_12_bits;
}

bool EventTimestampLessThan(const RtcpReceiverEventLogMessage& lhs,
                            const RtcpReceiverEventLogMessage& rhs) {
  return lhs.event_timestamp < rhs.event_timestamp;
}

void AddReceiverLog(
    const RtcpReceiverLogMessage& redundancy_receiver_log_message,
    RtcpReceiverLogMessage* receiver_log_message,
    size_t* remaining_space,
    size_t* number_of_frames,
    size_t* total_number_of_messages_to_send) {
  RtcpReceiverLogMessage::const_iterator it =
      redundancy_receiver_log_message.begin();
  while (it != redundancy_receiver_log_message.end() &&
         *remaining_space >=
             kRtcpReceiverFrameLogSize + kRtcpReceiverEventLogSize) {
    receiver_log_message->push_front(*it);
    size_t num_event_logs = (*remaining_space - kRtcpReceiverFrameLogSize) /
                            kRtcpReceiverEventLogSize;
    RtcpReceiverEventLogMessages& event_log_messages =
        receiver_log_message->front().event_log_messages_;
    if (num_event_logs < event_log_messages.size())
      event_log_messages.resize(num_event_logs);

    *remaining_space -= kRtcpReceiverFrameLogSize +
                        event_log_messages.size() * kRtcpReceiverEventLogSize;
    ++*number_of_frames;
    *total_number_of_messages_to_send += event_log_messages.size();
    ++it;
  }
}

// A class to build a string representing the NACK list in Cast message.
//
// The string will look like "23:3-6 25:1,5-6", meaning packets 3 to 6 in frame
// 23 are being NACK'ed (i.e. they are missing from the receiver's point of
// view) and packets 1, 5 and 6 are missing in frame 25. A frame that is
// completely missing will show as "26:65535".
class NackStringBuilder {
 public:
  NackStringBuilder()
      : frame_count_(0),
        packet_count_(0),
        last_frame_id_(-1),
        last_packet_id_(-1),
        contiguous_sequence_(false) {}
  ~NackStringBuilder() {}

  bool Empty() const { return frame_count_ == 0; }

  void PushFrame(int frame_id) {
    DCHECK_GE(frame_id, 0);
    if (frame_count_ > 0) {
      if (frame_id == last_frame_id_) {
        return;
      }
      if (contiguous_sequence_) {
        stream_ << "-" << last_packet_id_;
      }
      stream_ << ", ";
    }
    stream_ << frame_id;
    last_frame_id_ = frame_id;
    packet_count_ = 0;
    contiguous_sequence_ = false;
    ++frame_count_;
  }

  void PushPacket(int packet_id) {
    DCHECK_GE(last_frame_id_, 0);
    DCHECK_GE(packet_id, 0);
    if (packet_count_ == 0) {
      stream_ << ":" << packet_id;
    } else if (packet_id == last_packet_id_ + 1) {
      contiguous_sequence_ = true;
    } else {
      if (contiguous_sequence_) {
        stream_ << "-" << last_packet_id_;
        contiguous_sequence_ = false;
      }
      stream_ << "," << packet_id;
    }
    ++packet_count_;
    last_packet_id_ = packet_id;
  }

  std::string GetString() {
    if (contiguous_sequence_) {
      stream_ << "-" << last_packet_id_;
      contiguous_sequence_ = false;
    }
    return stream_.str();
  }

 private:
  std::ostringstream stream_;
  int frame_count_;
  int packet_count_;
  int last_frame_id_;
  int last_packet_id_;
  bool contiguous_sequence_;
};
}  // namespace

// TODO(mikhal): This is only used by the receiver. Consider renaming.
RtcpSender::RtcpSender(scoped_refptr<CastEnvironment> cast_environment,
                       transport::PacedPacketSender* outgoing_transport,
                       uint32 sending_ssrc,
                       const std::string& c_name)
    : ssrc_(sending_ssrc),
      c_name_(c_name),
      transport_(outgoing_transport),
      cast_environment_(cast_environment) {
  DCHECK_LT(c_name_.length(), kRtcpCnameSize) << "Invalid config";
}

RtcpSender::~RtcpSender() {}

void RtcpSender::SendRtcpFromRtpReceiver(
    uint32 packet_type_flags,
    const transport::RtcpReportBlock* report_block,
    const RtcpReceiverReferenceTimeReport* rrtr,
    const RtcpCastMessage* cast_message,
    const ReceiverRtcpEventSubscriber::RtcpEventMultiMap* rtcp_events,
    uint16 target_delay_ms) {
  if (packet_type_flags & transport::kRtcpSr ||
      packet_type_flags & transport::kRtcpDlrr ||
      packet_type_flags & transport::kRtcpSenderLog) {
    NOTREACHED() << "Invalid argument";
  }
  if (packet_type_flags & transport::kRtcpPli ||
      packet_type_flags & transport::kRtcpRpsi ||
      packet_type_flags & transport::kRtcpRemb ||
      packet_type_flags & transport::kRtcpNack) {
    // Implement these for webrtc interop.
    NOTIMPLEMENTED();
  }
  transport::PacketRef packet(new base::RefCountedData<Packet>);
  packet->data.reserve(kMaxIpPacketSize);

  if (packet_type_flags & transport::kRtcpRr) {
    BuildRR(report_block, &packet->data);
    if (!c_name_.empty()) {
      BuildSdec(&packet->data);
    }
  }
  if (packet_type_flags & transport::kRtcpBye) {
    BuildBye(&packet->data);
  }
  if (packet_type_flags & transport::kRtcpRrtr) {
    DCHECK(rrtr) << "Invalid argument";
    BuildRrtr(rrtr, &packet->data);
  }
  if (packet_type_flags & transport::kRtcpCast) {
    DCHECK(cast_message) << "Invalid argument";
    BuildCast(cast_message, target_delay_ms, &packet->data);
  }
  if (packet_type_flags & transport::kRtcpReceiverLog) {
    DCHECK(rtcp_events) << "Invalid argument";
    BuildReceiverLog(*rtcp_events, &packet->data);
  }

  if (packet->data.empty())
    return;  // Sanity don't send empty packets.

  transport_->SendRtcpPacket(ssrc_, packet);
}

void RtcpSender::BuildRR(const transport::RtcpReportBlock* report_block,
                         Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 32, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 32 > kMaxIpPacketSize)
    return;

  uint16 number_of_rows = (report_block) ? 7 : 1;
  packet->resize(start_size + 8);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 8);
  big_endian_writer.WriteU8(0x80 + (report_block ? 1 : 0));
  big_endian_writer.WriteU8(transport::kPacketTypeReceiverReport);
  big_endian_writer.WriteU16(number_of_rows);
  big_endian_writer.WriteU32(ssrc_);

  if (report_block) {
    AddReportBlocks(*report_block, packet);  // Adds 24 bytes.
  }
}

void RtcpSender::AddReportBlocks(const transport::RtcpReportBlock& report_block,
                                 Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 24, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 24 > kMaxIpPacketSize)
    return;

  packet->resize(start_size + 24);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 24);
  big_endian_writer.WriteU32(report_block.media_ssrc);
  big_endian_writer.WriteU8(report_block.fraction_lost);
  big_endian_writer.WriteU8(report_block.cumulative_lost >> 16);
  big_endian_writer.WriteU8(report_block.cumulative_lost >> 8);
  big_endian_writer.WriteU8(report_block.cumulative_lost);

  // Extended highest seq_no, contain the highest sequence number received.
  big_endian_writer.WriteU32(report_block.extended_high_sequence_number);
  big_endian_writer.WriteU32(report_block.jitter);

  // Last SR timestamp; our NTP time when we received the last report.
  // This is the value that we read from the send report packet not when we
  // received it.
  big_endian_writer.WriteU32(report_block.last_sr);

  // Delay since last received report, time since we received the report.
  big_endian_writer.WriteU32(report_block.delay_since_last_sr);
}

void RtcpSender::BuildSdec(Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 12 + c_name_.length(), kMaxIpPacketSize)
      << "Not enough buffer space";
  if (start_size + 12 > kMaxIpPacketSize)
    return;

  // SDES Source Description.
  packet->resize(start_size + 10);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 10);
  // We always need to add one SDES CNAME.
  big_endian_writer.WriteU8(0x80 + 1);
  big_endian_writer.WriteU8(transport::kPacketTypeSdes);

  // Handle SDES length later on.
  uint32 sdes_length_position = static_cast<uint32>(start_size) + 3;
  big_endian_writer.WriteU16(0);
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(1);       // CNAME = 1
  big_endian_writer.WriteU8(static_cast<uint8>(c_name_.length()));

  size_t sdes_length = 10 + c_name_.length();
  packet->insert(
      packet->end(), c_name_.c_str(), c_name_.c_str() + c_name_.length());

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
}

void RtcpSender::BuildPli(uint32 remote_ssrc, Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 12, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 12 > kMaxIpPacketSize)
    return;

  packet->resize(start_size + 12);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 12);
  uint8 FMT = 1;  // Picture loss indicator.
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(transport::kPacketTypePayloadSpecific);
  big_endian_writer.WriteU16(2);            // Used fixed length of 2.
  big_endian_writer.WriteU32(ssrc_);        // Add our own SSRC.
  big_endian_writer.WriteU32(remote_ssrc);  // Add the remote SSRC.
}

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      PB       |0| Payload Type|    Native Rpsi bit string     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   defined per codec          ...                | Padding (0) |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
void RtcpSender::BuildRpsi(const RtcpRpsiMessage* rpsi, Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 24, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 24 > kMaxIpPacketSize)
    return;

  packet->resize(start_size + 24);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 24);
  uint8 FMT = 3;  // Reference Picture Selection Indication.
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(transport::kPacketTypePayloadSpecific);

  // Calculate length.
  uint32 bits_required = 7;
  uint8 bytes_required = 1;
  while ((rpsi->picture_id >> bits_required) > 0) {
    bits_required += 7;
    bytes_required++;
  }
  uint8 size = 3;
  if (bytes_required > 6) {
    size = 5;
  } else if (bytes_required > 2) {
    size = 4;
  }
  big_endian_writer.WriteU8(0);
  big_endian_writer.WriteU8(size);
  big_endian_writer.WriteU32(ssrc_);
  big_endian_writer.WriteU32(rpsi->remote_ssrc);

  uint8 padding_bytes = 4 - ((2 + bytes_required) % 4);
  if (padding_bytes == 4) {
    padding_bytes = 0;
  }
  // Add padding length in bits, padding can be 0, 8, 16 or 24.
  big_endian_writer.WriteU8(padding_bytes * 8);
  big_endian_writer.WriteU8(rpsi->payload_type);

  // Add picture ID.
  for (int i = bytes_required - 1; i > 0; i--) {
    big_endian_writer.WriteU8(0x80 |
                              static_cast<uint8>(rpsi->picture_id >> (i * 7)));
  }
  // Add last byte of picture ID.
  big_endian_writer.WriteU8(static_cast<uint8>(rpsi->picture_id & 0x7f));

  // Add padding.
  for (int j = 0; j < padding_bytes; ++j) {
    big_endian_writer.WriteU8(0);
  }
}

void RtcpSender::BuildRemb(const RtcpRembMessage* remb, Packet* packet) const {
  size_t start_size = packet->size();
  size_t remb_size = 20 + 4 * remb->remb_ssrcs.size();
  DCHECK_LT(start_size + remb_size, kMaxIpPacketSize)
      << "Not enough buffer space";
  if (start_size + remb_size > kMaxIpPacketSize)
    return;

  packet->resize(start_size + remb_size);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), remb_size);

  // Add application layer feedback.
  uint8 FMT = 15;
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(transport::kPacketTypePayloadSpecific);
  big_endian_writer.WriteU8(0);
  big_endian_writer.WriteU8(static_cast<uint8>(remb->remb_ssrcs.size() + 4));
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(0);      // Remote SSRC must be 0.
  big_endian_writer.WriteU32(kRemb);
  big_endian_writer.WriteU8(static_cast<uint8>(remb->remb_ssrcs.size()));

  // 6 bit exponent and a 18 bit mantissa.
  uint8 bitrate_exponent;
  uint32 bitrate_mantissa;
  BitrateToRembExponentBitrate(
      remb->remb_bitrate, &bitrate_exponent, &bitrate_mantissa);

  big_endian_writer.WriteU8(static_cast<uint8>(
      (bitrate_exponent << 2) + ((bitrate_mantissa >> 16) & 0x03)));
  big_endian_writer.WriteU8(static_cast<uint8>(bitrate_mantissa >> 8));
  big_endian_writer.WriteU8(static_cast<uint8>(bitrate_mantissa));

  std::list<uint32>::const_iterator it = remb->remb_ssrcs.begin();
  for (; it != remb->remb_ssrcs.end(); ++it) {
    big_endian_writer.WriteU32(*it);
  }
}

void RtcpSender::BuildNack(const RtcpNackMessage* nack, Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 16, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 16 > kMaxIpPacketSize)
    return;

  packet->resize(start_size + 16);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 16);

  uint8 FMT = 1;
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(transport::kPacketTypeGenericRtpFeedback);
  big_endian_writer.WriteU8(0);
  size_t nack_size_pos = start_size + 3;
  big_endian_writer.WriteU8(3);
  big_endian_writer.WriteU32(ssrc_);              // Add our own SSRC.
  big_endian_writer.WriteU32(nack->remote_ssrc);  // Add the remote SSRC.

  // Build NACK bitmasks and write them to the Rtcp message.
  // The nack list should be sorted and not contain duplicates.
  size_t number_of_nack_fields = 0;
  size_t max_number_of_nack_fields = std::min<size_t>(
      kRtcpMaxNackFields, (kMaxIpPacketSize - packet->size()) / 4);

  std::list<uint16>::const_iterator it = nack->nack_list.begin();
  while (it != nack->nack_list.end() &&
         number_of_nack_fields < max_number_of_nack_fields) {
    uint16 nack_sequence_number = *it;
    uint16 bitmask = 0;
    ++it;
    while (it != nack->nack_list.end()) {
      int shift = static_cast<uint16>(*it - nack_sequence_number) - 1;
      if (shift >= 0 && shift <= 15) {
        bitmask |= (1 << shift);
        ++it;
      } else {
        break;
      }
    }
    // Write the sequence number and the bitmask to the packet.
    start_size = packet->size();
    DCHECK_LT(start_size + 4, kMaxIpPacketSize) << "Not enough buffer space";
    if (start_size + 4 > kMaxIpPacketSize)
      return;

    packet->resize(start_size + 4);
    base::BigEndianWriter big_endian_nack_writer(
        reinterpret_cast<char*>(&((*packet)[start_size])), 4);
    big_endian_nack_writer.WriteU16(nack_sequence_number);
    big_endian_nack_writer.WriteU16(bitmask);
    number_of_nack_fields++;
  }
  DCHECK_GE(kRtcpMaxNackFields, number_of_nack_fields);
  (*packet)[nack_size_pos] = static_cast<uint8>(2 + number_of_nack_fields);
}

void RtcpSender::BuildBye(Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 8, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 8 > kMaxIpPacketSize)
    return;

  packet->resize(start_size + 8);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 8);
  big_endian_writer.WriteU8(0x80 + 1);
  big_endian_writer.WriteU8(transport::kPacketTypeBye);
  big_endian_writer.WriteU16(1);      // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
}

void RtcpSender::BuildRrtr(const RtcpReceiverReferenceTimeReport* rrtr,
                           Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 20, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 20 > kMaxIpPacketSize)
    return;

  packet->resize(start_size + 20);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 20);

  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(transport::kPacketTypeXr);
  big_endian_writer.WriteU16(4);      // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(4);       // Add block type.
  big_endian_writer.WriteU8(0);       // Add reserved.
  big_endian_writer.WriteU16(2);      // Block length.

  // Add the media (received RTP) SSRC.
  big_endian_writer.WriteU32(rrtr->ntp_seconds);
  big_endian_writer.WriteU32(rrtr->ntp_fraction);
}

void RtcpSender::BuildCast(const RtcpCastMessage* cast,
                           uint16 target_delay_ms,
                           Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 20, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 20 > kMaxIpPacketSize)
    return;

  packet->resize(start_size + 20);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 20);
  uint8 FMT = 15;  // Application layer feedback.
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(transport::kPacketTypePayloadSpecific);
  big_endian_writer.WriteU8(0);
  size_t cast_size_pos = start_size + 3;  // Save length position.
  big_endian_writer.WriteU8(4);
  big_endian_writer.WriteU32(ssrc_);              // Add our own SSRC.
  big_endian_writer.WriteU32(cast->media_ssrc_);  // Remote SSRC.
  big_endian_writer.WriteU32(kCast);
  big_endian_writer.WriteU8(static_cast<uint8>(cast->ack_frame_id_));
  size_t cast_loss_field_pos = start_size + 17;  // Save loss field position.
  big_endian_writer.WriteU8(0);  // Overwritten with number_of_loss_fields.
  big_endian_writer.WriteU16(target_delay_ms);

  size_t number_of_loss_fields = 0;
  size_t max_number_of_loss_fields = std::min<size_t>(
      kRtcpMaxCastLossFields, (kMaxIpPacketSize - packet->size()) / 4);

  MissingFramesAndPacketsMap::const_iterator frame_it =
      cast->missing_frames_and_packets_.begin();

  NackStringBuilder nack_string_builder;
  for (; frame_it != cast->missing_frames_and_packets_.end() &&
             number_of_loss_fields < max_number_of_loss_fields;
       ++frame_it) {
    nack_string_builder.PushFrame(frame_it->first);
    // Iterate through all frames with missing packets.
    if (frame_it->second.empty()) {
      // Special case all packets in a frame is missing.
      start_size = packet->size();
      packet->resize(start_size + 4);
      base::BigEndianWriter big_endian_nack_writer(
          reinterpret_cast<char*>(&((*packet)[start_size])), 4);
      big_endian_nack_writer.WriteU8(static_cast<uint8>(frame_it->first));
      big_endian_nack_writer.WriteU16(kRtcpCastAllPacketsLost);
      big_endian_nack_writer.WriteU8(0);
      nack_string_builder.PushPacket(kRtcpCastAllPacketsLost);
      ++number_of_loss_fields;
    } else {
      PacketIdSet::const_iterator packet_it = frame_it->second.begin();
      while (packet_it != frame_it->second.end()) {
        uint16 packet_id = *packet_it;

        start_size = packet->size();
        packet->resize(start_size + 4);
        base::BigEndianWriter big_endian_nack_writer(
            reinterpret_cast<char*>(&((*packet)[start_size])), 4);

        // Write frame and packet id to buffer before calculating bitmask.
        big_endian_nack_writer.WriteU8(static_cast<uint8>(frame_it->first));
        big_endian_nack_writer.WriteU16(packet_id);
        nack_string_builder.PushPacket(packet_id);

        uint8 bitmask = 0;
        ++packet_it;
        while (packet_it != frame_it->second.end()) {
          int shift = static_cast<uint8>(*packet_it - packet_id) - 1;
          if (shift >= 0 && shift <= 7) {
            nack_string_builder.PushPacket(*packet_it);
            bitmask |= (1 << shift);
            ++packet_it;
          } else {
            break;
          }
        }
        big_endian_nack_writer.WriteU8(bitmask);
        ++number_of_loss_fields;
      }
    }
  }
  VLOG_IF(1, !nack_string_builder.Empty())
      << "SSRC: " << cast->media_ssrc_
      << ", ACK: " << cast->ack_frame_id_
      << ", NACK: " << nack_string_builder.GetString();
  DCHECK_LE(number_of_loss_fields, kRtcpMaxCastLossFields);
  (*packet)[cast_size_pos] = static_cast<uint8>(4 + number_of_loss_fields);
  (*packet)[cast_loss_field_pos] = static_cast<uint8>(number_of_loss_fields);
}

void RtcpSender::BuildReceiverLog(
    const ReceiverRtcpEventSubscriber::RtcpEventMultiMap& rtcp_events,
    Packet* packet) {
  const size_t packet_start_size = packet->size();
  size_t number_of_frames = 0;
  size_t total_number_of_messages_to_send = 0;
  size_t rtcp_log_size = 0;
  RtcpReceiverLogMessage receiver_log_message;

  if (!BuildRtcpReceiverLogMessage(rtcp_events,
                                   packet_start_size,
                                   &receiver_log_message,
                                   &number_of_frames,
                                   &total_number_of_messages_to_send,
                                   &rtcp_log_size)) {
    return;
  }
  packet->resize(packet_start_size + rtcp_log_size);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[packet_start_size])), rtcp_log_size);
  big_endian_writer.WriteU8(0x80 + kReceiverLogSubtype);
  big_endian_writer.WriteU8(transport::kPacketTypeApplicationDefined);
  big_endian_writer.WriteU16(static_cast<uint16>(
      2 + 2 * number_of_frames + total_number_of_messages_to_send));
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(kCast);

  while (!receiver_log_message.empty() &&
         total_number_of_messages_to_send > 0) {
    RtcpReceiverFrameLogMessage& frame_log_messages(
        receiver_log_message.front());

    // Add our frame header.
    big_endian_writer.WriteU32(frame_log_messages.rtp_timestamp_);
    size_t messages_in_frame = frame_log_messages.event_log_messages_.size();
    if (messages_in_frame > total_number_of_messages_to_send) {
      // We are running out of space.
      messages_in_frame = total_number_of_messages_to_send;
    }
    // Keep track of how many messages we have left to send.
    total_number_of_messages_to_send -= messages_in_frame;

    // On the wire format is number of messages - 1.
    big_endian_writer.WriteU8(static_cast<uint8>(messages_in_frame - 1));

    base::TimeTicks event_timestamp_base =
        frame_log_messages.event_log_messages_.front().event_timestamp;
    uint32 base_timestamp_ms =
        (event_timestamp_base - base::TimeTicks()).InMilliseconds();
    big_endian_writer.WriteU8(static_cast<uint8>(base_timestamp_ms >> 16));
    big_endian_writer.WriteU8(static_cast<uint8>(base_timestamp_ms >> 8));
    big_endian_writer.WriteU8(static_cast<uint8>(base_timestamp_ms));

    while (!frame_log_messages.event_log_messages_.empty() &&
           messages_in_frame > 0) {
      const RtcpReceiverEventLogMessage& event_message =
          frame_log_messages.event_log_messages_.front();
      uint16 event_type_and_timestamp_delta =
          MergeEventTypeAndTimestampForWireFormat(
              event_message.type,
              event_message.event_timestamp - event_timestamp_base);
      switch (event_message.type) {
        case FRAME_ACK_SENT:
        case FRAME_PLAYOUT:
        case FRAME_DECODED:
          big_endian_writer.WriteU16(
              static_cast<uint16>(event_message.delay_delta.InMilliseconds()));
          big_endian_writer.WriteU16(event_type_and_timestamp_delta);
          break;
        case PACKET_RECEIVED:
          big_endian_writer.WriteU16(event_message.packet_id);
          big_endian_writer.WriteU16(event_type_and_timestamp_delta);
          break;
        default:
          NOTREACHED();
      }
      messages_in_frame--;
      frame_log_messages.event_log_messages_.pop_front();
    }
    if (frame_log_messages.event_log_messages_.empty()) {
      // We sent all messages on this frame; pop the frame header.
      receiver_log_message.pop_front();
    }
  }
  DCHECK_EQ(total_number_of_messages_to_send, 0u);
}

bool RtcpSender::BuildRtcpReceiverLogMessage(
    const ReceiverRtcpEventSubscriber::RtcpEventMultiMap& rtcp_events,
    size_t start_size,
    RtcpReceiverLogMessage* receiver_log_message,
    size_t* number_of_frames,
    size_t* total_number_of_messages_to_send,
    size_t* rtcp_log_size) {
  size_t remaining_space =
      std::min(kMaxReceiverLogBytes, kMaxIpPacketSize - start_size);
  if (remaining_space < kRtcpCastLogHeaderSize + kRtcpReceiverFrameLogSize +
                            kRtcpReceiverEventLogSize) {
    return false;
  }

  // We use this to do event timestamp sorting and truncating for events of
  // a single frame.
  std::vector<RtcpReceiverEventLogMessage> sorted_log_messages;

  // Account for the RTCP header for an application-defined packet.
  remaining_space -= kRtcpCastLogHeaderSize;

  ReceiverRtcpEventSubscriber::RtcpEventMultiMap::const_reverse_iterator rit =
      rtcp_events.rbegin();

  while (rit != rtcp_events.rend() &&
         remaining_space >=
             kRtcpReceiverFrameLogSize + kRtcpReceiverEventLogSize) {
    const RtpTimestamp rtp_timestamp = rit->first;
    RtcpReceiverFrameLogMessage frame_log(rtp_timestamp);
    remaining_space -= kRtcpReceiverFrameLogSize;
    ++*number_of_frames;

    // Get all events of a single frame.
    sorted_log_messages.clear();
    do {
      RtcpReceiverEventLogMessage event_log_message;
      event_log_message.type = rit->second.type;
      event_log_message.event_timestamp = rit->second.timestamp;
      event_log_message.delay_delta = rit->second.delay_delta;
      event_log_message.packet_id = rit->second.packet_id;
      sorted_log_messages.push_back(event_log_message);
      ++rit;
    } while (rit != rtcp_events.rend() && rit->first == rtp_timestamp);

    std::sort(sorted_log_messages.begin(),
              sorted_log_messages.end(),
              &EventTimestampLessThan);

    // From |sorted_log_messages|, only take events that are no greater than
    // |kMaxWireFormatTimeDeltaMs| seconds away from the latest event. Events
    // older than that cannot be encoded over the wire.
    std::vector<RtcpReceiverEventLogMessage>::reverse_iterator sorted_rit =
        sorted_log_messages.rbegin();
    base::TimeTicks first_event_timestamp = sorted_rit->event_timestamp;
    size_t events_in_frame = 0;
    while (sorted_rit != sorted_log_messages.rend() &&
           events_in_frame < kRtcpMaxReceiverLogMessages &&
           remaining_space >= kRtcpReceiverEventLogSize) {
      base::TimeDelta delta(first_event_timestamp -
                            sorted_rit->event_timestamp);
      if (delta.InMilliseconds() > kMaxWireFormatTimeDeltaMs)
        break;
      frame_log.event_log_messages_.push_front(*sorted_rit);
      ++events_in_frame;
      ++*total_number_of_messages_to_send;
      remaining_space -= kRtcpReceiverEventLogSize;
      ++sorted_rit;
    }

    receiver_log_message->push_front(frame_log);
  }

  rtcp_events_history_.push_front(*receiver_log_message);

  // We don't try to match RTP timestamps of redundancy frame logs with those
  // from the newest set (which would save the space of an extra RTP timestamp
  // over the wire). Unless the redundancy frame logs are very recent, it's
  // unlikely there will be a match anyway.
  if (rtcp_events_history_.size() > kFirstRedundancyOffset) {
    // Add first redundnacy messages, if enough space remaining
    AddReceiverLog(rtcp_events_history_[kFirstRedundancyOffset],
                   receiver_log_message,
                   &remaining_space,
                   number_of_frames,
                   total_number_of_messages_to_send);
  }

  if (rtcp_events_history_.size() > kSecondRedundancyOffset) {
    // Add second redundancy messages, if enough space remaining
    AddReceiverLog(rtcp_events_history_[kSecondRedundancyOffset],
                   receiver_log_message,
                   &remaining_space,
                   number_of_frames,
                   total_number_of_messages_to_send);
  }

  if (rtcp_events_history_.size() > kReceiveLogMessageHistorySize) {
    rtcp_events_history_.pop_back();
  }

  DCHECK_LE(rtcp_events_history_.size(), kReceiveLogMessageHistorySize);

  *rtcp_log_size =
      kRtcpCastLogHeaderSize + *number_of_frames * kRtcpReceiverFrameLogSize +
      *total_number_of_messages_to_send * kRtcpReceiverEventLogSize;
  DCHECK_GE(kMaxIpPacketSize, start_size + *rtcp_log_size)
      << "Not enough buffer space.";

  VLOG(3) << "number of frames: " << *number_of_frames;
  VLOG(3) << "total messages to send: " << *total_number_of_messages_to_send;
  VLOG(3) << "rtcp log size: " << *rtcp_log_size;
  return *number_of_frames > 0;
}

}  // namespace cast
}  // namespace media
