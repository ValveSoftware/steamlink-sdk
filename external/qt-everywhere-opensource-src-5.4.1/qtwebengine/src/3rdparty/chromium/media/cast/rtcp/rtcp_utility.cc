// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtcp/rtcp_utility.h"

#include "base/big_endian.h"
#include "base/logging.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {

RtcpParser::RtcpParser(const uint8* rtcpData, size_t rtcpDataLength)
    : rtcp_data_begin_(rtcpData),
      rtcp_data_end_(rtcpData + rtcpDataLength),
      valid_packet_(false),
      rtcp_data_(rtcpData),
      rtcp_block_end_(NULL),
      state_(kStateTopLevel),
      number_of_blocks_(0),
      field_type_(kRtcpNotValidCode) {
  memset(&field_, 0, sizeof(field_));
  Validate();
}

RtcpParser::~RtcpParser() {}

RtcpFieldTypes RtcpParser::FieldType() const { return field_type_; }

const RtcpField& RtcpParser::Field() const { return field_; }

RtcpFieldTypes RtcpParser::Begin() {
  rtcp_data_ = rtcp_data_begin_;
  return Iterate();
}

RtcpFieldTypes RtcpParser::Iterate() {
  // Reset packet type
  field_type_ = kRtcpNotValidCode;

  if (!IsValid())
    return kRtcpNotValidCode;

  switch (state_) {
    case kStateTopLevel:
      IterateTopLevel();
      break;
    case kStateReportBlock:
      IterateReportBlockItem();
      break;
    case kStateSdes:
      IterateSdesItem();
      break;
    case kStateBye:
      IterateByeItem();
      break;
    case kStateApplicationSpecificCastReceiverFrameLog:
      IterateCastReceiverLogFrame();
      break;
    case kStateApplicationSpecificCastReceiverEventLog:
      IterateCastReceiverLogEvent();
      break;
    case kStateExtendedReportBlock:
      IterateExtendedReportItem();
      break;
    case kStateExtendedReportDelaySinceLastReceiverReport:
      IterateExtendedReportDelaySinceLastReceiverReportItem();
      break;
    case kStateGenericRtpFeedbackNack:
      IterateNackItem();
      break;
    case kStatePayloadSpecificRpsi:
      IterateRpsiItem();
      break;
    case kStatePayloadSpecificFir:
      IterateFirItem();
      break;
    case kStatePayloadSpecificApplication:
      IteratePayloadSpecificAppItem();
      break;
    case kStatePayloadSpecificRemb:
      IteratePayloadSpecificRembItem();
      break;
    case kStatePayloadSpecificCast:
      IteratePayloadSpecificCastItem();
      break;
    case kStatePayloadSpecificCastNack:
      IteratePayloadSpecificCastNackItem();
      break;
  }
  return field_type_;
}

void RtcpParser::IterateTopLevel() {
  for (;;) {
    RtcpCommonHeader header;

    bool success = RtcpParseCommonHeader(rtcp_data_, rtcp_data_end_, &header);
    if (!success)
      return;

    rtcp_block_end_ = rtcp_data_ + header.length_in_octets;

    if (rtcp_block_end_ > rtcp_data_end_)
      return;  // Bad block!

    switch (header.PT) {
      case transport::kPacketTypeSenderReport:
        // number of Report blocks
        number_of_blocks_ = header.IC;
        ParseSR();
        return;
      case transport::kPacketTypeReceiverReport:
        // number of Report blocks
        number_of_blocks_ = header.IC;
        ParseRR();
        return;
      case transport::kPacketTypeSdes:
        // number of Sdes blocks
        number_of_blocks_ = header.IC;
        if (!ParseSdes()) {
          break;  // Nothing supported found, continue to next block!
        }
        return;
      case transport::kPacketTypeBye:
        number_of_blocks_ = header.IC;
        if (!ParseBye()) {
          // Nothing supported found, continue to next block!
          break;
        }
        return;
      case transport::kPacketTypeApplicationDefined:
        if (!ParseApplicationDefined(header.IC)) {
          // Nothing supported found, continue to next block!
          break;
        }
        return;
      case transport::kPacketTypeGenericRtpFeedback:  // Fall through!
      case transport::kPacketTypePayloadSpecific:
        if (!ParseFeedBackCommon(header)) {
          // Nothing supported found, continue to next block!
          break;
        }
        return;
      case transport::kPacketTypeXr:
        if (!ParseExtendedReport()) {
          break;  // Nothing supported found, continue to next block!
        }
        return;
      default:
        // Not supported! Skip!
        EndCurrentBlock();
        break;
    }
  }
}

void RtcpParser::IterateReportBlockItem() {
  bool success = ParseReportBlockItem();
  if (!success)
    Iterate();
}

void RtcpParser::IterateSdesItem() {
  bool success = ParseSdesItem();
  if (!success)
    Iterate();
}

void RtcpParser::IterateByeItem() {
  bool success = ParseByeItem();
  if (!success)
    Iterate();
}

void RtcpParser::IterateExtendedReportItem() {
  bool success = ParseExtendedReportItem();
  if (!success)
    Iterate();
}

void RtcpParser::IterateExtendedReportDelaySinceLastReceiverReportItem() {
  bool success = ParseExtendedReportDelaySinceLastReceiverReport();
  if (!success)
    Iterate();
}

void RtcpParser::IterateNackItem() {
  bool success = ParseNackItem();
  if (!success)
    Iterate();
}

void RtcpParser::IterateRpsiItem() {
  bool success = ParseRpsiItem();
  if (!success)
    Iterate();
}

void RtcpParser::IterateFirItem() {
  bool success = ParseFirItem();
  if (!success)
    Iterate();
}

void RtcpParser::IteratePayloadSpecificAppItem() {
  bool success = ParsePayloadSpecificAppItem();
  if (!success)
    Iterate();
}

void RtcpParser::IteratePayloadSpecificRembItem() {
  bool success = ParsePayloadSpecificRembItem();
  if (!success)
    Iterate();
}

void RtcpParser::IteratePayloadSpecificCastItem() {
  bool success = ParsePayloadSpecificCastItem();
  if (!success)
    Iterate();
}

void RtcpParser::IteratePayloadSpecificCastNackItem() {
  bool success = ParsePayloadSpecificCastNackItem();
  if (!success)
    Iterate();
}

void RtcpParser::IterateCastReceiverLogFrame() {
  bool success = ParseCastReceiverLogFrameItem();
  if (!success)
    Iterate();
}

void RtcpParser::IterateCastReceiverLogEvent() {
  bool success = ParseCastReceiverLogEventItem();
  if (!success)
    Iterate();
}

void RtcpParser::Validate() {
  if (rtcp_data_ == NULL)
    return;  // NOT VALID

  RtcpCommonHeader header;
  bool success =
      RtcpParseCommonHeader(rtcp_data_begin_, rtcp_data_end_, &header);

  if (!success)
    return;  // NOT VALID!

  valid_packet_ = true;
}

bool RtcpParser::IsValid() const { return valid_packet_; }

void RtcpParser::EndCurrentBlock() { rtcp_data_ = rtcp_block_end_; }

bool RtcpParser::RtcpParseCommonHeader(const uint8* data_begin,
                                       const uint8* data_end,
                                       RtcpCommonHeader* parsed_header) const {
  if (!data_begin || !data_end)
    return false;

  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |V=2|P|    IC   |      PT       |             length            |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //
  // Common header for all Rtcp packets, 4 octets.

  if ((data_end - data_begin) < 4)
    return false;

  parsed_header->V = data_begin[0] >> 6;
  parsed_header->P = ((data_begin[0] & 0x20) == 0) ? false : true;
  parsed_header->IC = data_begin[0] & 0x1f;
  parsed_header->PT = data_begin[1];

  parsed_header->length_in_octets =
      ((data_begin[2] << 8) + data_begin[3] + 1) * 4;

  if (parsed_header->length_in_octets == 0)
    return false;

  // Check if RTP version field == 2.
  if (parsed_header->V != 2)
    return false;

  return true;
}

bool RtcpParser::ParseRR() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 8)
    return false;

  field_type_ = kRtcpRrCode;

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.Skip(4);  // Skip header
  big_endian_reader.ReadU32(&field_.receiver_report.sender_ssrc);
  field_.receiver_report.number_of_report_blocks = number_of_blocks_;
  rtcp_data_ += 8;

  // State transition
  state_ = kStateReportBlock;
  return true;
}

bool RtcpParser::ParseSR() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 28) {
    EndCurrentBlock();
    return false;
  }
  field_type_ = kRtcpSrCode;

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.Skip(4);  // Skip header
  big_endian_reader.ReadU32(&field_.sender_report.sender_ssrc);
  big_endian_reader.ReadU32(&field_.sender_report.ntp_most_significant);
  big_endian_reader.ReadU32(&field_.sender_report.ntp_least_significant);
  big_endian_reader.ReadU32(&field_.sender_report.rtp_timestamp);
  big_endian_reader.ReadU32(&field_.sender_report.sender_packet_count);
  big_endian_reader.ReadU32(&field_.sender_report.sender_octet_count);
  field_.sender_report.number_of_report_blocks = number_of_blocks_;
  rtcp_data_ += 28;

  if (number_of_blocks_ != 0) {
    // State transition.
    state_ = kStateReportBlock;
  } else {
    // Don't go to state report block item if 0 report blocks.
    state_ = kStateTopLevel;
    EndCurrentBlock();
  }
  return true;
}

bool RtcpParser::ParseReportBlockItem() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 24 || number_of_blocks_ <= 0) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU32(&field_.report_block_item.ssrc);
  big_endian_reader.ReadU8(&field_.report_block_item.fraction_lost);

  uint8 temp_number_of_packets_lost;
  big_endian_reader.ReadU8(&temp_number_of_packets_lost);
  field_.report_block_item.cumulative_number_of_packets_lost =
      temp_number_of_packets_lost << 16;
  big_endian_reader.ReadU8(&temp_number_of_packets_lost);
  field_.report_block_item.cumulative_number_of_packets_lost +=
      temp_number_of_packets_lost << 8;
  big_endian_reader.ReadU8(&temp_number_of_packets_lost);
  field_.report_block_item.cumulative_number_of_packets_lost +=
      temp_number_of_packets_lost;

  big_endian_reader.ReadU32(
      &field_.report_block_item.extended_highest_sequence_number);
  big_endian_reader.ReadU32(&field_.report_block_item.jitter);
  big_endian_reader.ReadU32(&field_.report_block_item.last_sender_report);
  big_endian_reader.ReadU32(&field_.report_block_item.delay_last_sender_report);
  rtcp_data_ += 24;

  number_of_blocks_--;
  field_type_ = kRtcpReportBlockItemCode;
  return true;
}

bool RtcpParser::ParseSdes() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;

  if (length < 8) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }
  rtcp_data_ += 4;  // Skip header

  state_ = kStateSdes;
  field_type_ = kRtcpSdesCode;
  return true;
}

bool RtcpParser::ParseSdesItem() {
  if (number_of_blocks_ <= 0) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }
  number_of_blocks_--;

  // Find c_name item in a Sdes chunk.
  while (rtcp_data_ < rtcp_block_end_) {
    ptrdiff_t data_length = rtcp_block_end_ - rtcp_data_;
    if (data_length < 4) {
      state_ = kStateTopLevel;
      EndCurrentBlock();
      return false;
    }

    uint32 ssrc;
    base::BigEndianReader big_endian_reader(
        reinterpret_cast<const char*>(rtcp_data_), data_length);
    big_endian_reader.ReadU32(&ssrc);
    rtcp_data_ += 4;

    bool found_c_name = ParseSdesTypes();
    if (found_c_name) {
      field_.c_name.sender_ssrc = ssrc;
      return true;
    }
  }
  state_ = kStateTopLevel;
  EndCurrentBlock();
  return false;
}

bool RtcpParser::ParseSdesTypes() {
  // Only the c_name item is mandatory. RFC 3550 page 46.
  bool found_c_name = false;
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);

  while (big_endian_reader.remaining() > 0) {
    uint8 tag;
    big_endian_reader.ReadU8(&tag);

    if (tag == 0) {
      // End tag! 4 octet aligned.
      rtcp_data_ = rtcp_block_end_;
      return found_c_name;
    }

    if (big_endian_reader.remaining() > 0) {
      uint8 len;
      big_endian_reader.ReadU8(&len);

      if (tag == 1) {  // c_name.
        // Sanity check.
        if (big_endian_reader.remaining() < len) {
          state_ = kStateTopLevel;
          EndCurrentBlock();
          return false;
        }
        int i = 0;
        for (; i < len; ++i) {
          uint8 c;
          big_endian_reader.ReadU8(&c);
          if ((c < ' ') || (c > '{') || (c == '%') || (c == '\\')) {
            // Illegal char.
            state_ = kStateTopLevel;
            EndCurrentBlock();
            return false;
          }
          field_.c_name.name[i] = c;
        }
        // Make sure we are null terminated.
        field_.c_name.name[i] = 0;
        field_type_ = kRtcpSdesChunkCode;
        found_c_name = true;
      } else {
        big_endian_reader.Skip(len);
      }
    }
  }
  // No end tag found!
  state_ = kStateTopLevel;
  EndCurrentBlock();
  return false;
}

bool RtcpParser::ParseBye() {
  rtcp_data_ += 4;  // Skip header.
  state_ = kStateBye;
  return ParseByeItem();
}

bool RtcpParser::ParseByeItem() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 4 || number_of_blocks_ == 0) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }

  field_type_ = kRtcpByeCode;

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU32(&field_.bye.sender_ssrc);
  rtcp_data_ += 4;

  // We can have several CSRCs attached.
  if (length >= 4 * number_of_blocks_) {
    rtcp_data_ += (number_of_blocks_ - 1) * 4;
  }
  number_of_blocks_ = 0;
  return true;
}

bool RtcpParser::ParseApplicationDefined(uint8 subtype) {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 16 || subtype != kReceiverLogSubtype) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }

  uint32 sender_ssrc;
  uint32 name;

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.Skip(4);  // Skip header.
  big_endian_reader.ReadU32(&sender_ssrc);
  big_endian_reader.ReadU32(&name);

  if (name != kCast) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }
  rtcp_data_ += 12;
  switch (subtype) {
    case kReceiverLogSubtype:
      state_ = kStateApplicationSpecificCastReceiverFrameLog;
      field_type_ = kRtcpApplicationSpecificCastReceiverLogCode;
      field_.cast_receiver_log.sender_ssrc = sender_ssrc;
      break;
    default:
      NOTREACHED();
  }
  return true;
}

bool RtcpParser::ParseCastReceiverLogFrameItem() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 12) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }
  uint32 rtp_timestamp;
  uint32 data;
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU32(&rtp_timestamp);
  big_endian_reader.ReadU32(&data);

  rtcp_data_ += 8;

  field_.cast_receiver_log.rtp_timestamp = rtp_timestamp;
  // We have 24 LSB of the event timestamp base on the wire.
  field_.cast_receiver_log.event_timestamp_base = data & 0xffffff;

  number_of_blocks_ = 1 + static_cast<uint8>(data >> 24);
  state_ = kStateApplicationSpecificCastReceiverEventLog;
  field_type_ = kRtcpApplicationSpecificCastReceiverLogFrameCode;
  return true;
}

bool RtcpParser::ParseCastReceiverLogEventItem() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 4) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }
  if (number_of_blocks_ == 0) {
    // Continue parsing the next receiver frame event.
    state_ = kStateApplicationSpecificCastReceiverFrameLog;
    return false;
  }
  number_of_blocks_--;

  uint16 delay_delta_or_packet_id;
  uint16 event_type_and_timestamp_delta;
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU16(&delay_delta_or_packet_id);
  big_endian_reader.ReadU16(&event_type_and_timestamp_delta);

  rtcp_data_ += 4;

  field_.cast_receiver_log.event =
      static_cast<uint8>(event_type_and_timestamp_delta >> 12);
  // delay_delta is in union'ed with packet_id.
  field_.cast_receiver_log.delay_delta_or_packet_id.packet_id =
      delay_delta_or_packet_id;
  field_.cast_receiver_log.event_timestamp_delta =
      event_type_and_timestamp_delta & 0xfff;

  field_type_ = kRtcpApplicationSpecificCastReceiverLogEventCode;
  return true;
}

bool RtcpParser::ParseFeedBackCommon(const RtcpCommonHeader& header) {
  DCHECK((header.PT == transport::kPacketTypeGenericRtpFeedback) ||
         (header.PT == transport::kPacketTypePayloadSpecific))
      << "Invalid state";

  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;

  if (length < 12) {  // 4 * 3, RFC4585 section 6.1
    EndCurrentBlock();
    return false;
  }

  uint32 sender_ssrc;
  uint32 media_ssrc;
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.Skip(4);  // Skip header.
  big_endian_reader.ReadU32(&sender_ssrc);
  big_endian_reader.ReadU32(&media_ssrc);

  rtcp_data_ += 12;

  if (header.PT == transport::kPacketTypeGenericRtpFeedback) {
    // Transport layer feedback
    switch (header.IC) {
      case 1:
        // Nack
        field_type_ = kRtcpGenericRtpFeedbackNackCode;
        field_.nack.sender_ssrc = sender_ssrc;
        field_.nack.media_ssrc = media_ssrc;
        state_ = kStateGenericRtpFeedbackNack;
        return true;
      case 2:
        // Used to be ACK is this code point, which is removed conficts with
        // http://tools.ietf.org/html/draft-levin-avt-rtcp-burst-00
        break;
      case 3:
        // Tmmbr
        break;
      case 4:
        // Tmmbn
        break;
      case 5:
        // RFC 6051 RTCP-sender_report-REQ Rapid Synchronisation of RTP Flows
        // Trigger a new Rtcp sender_report
        field_type_ = kRtcpGenericRtpFeedbackSrReqCode;

        // Note: No state transition, sender report REQ is empty!
        return true;
      default:
        break;
    }
    EndCurrentBlock();
    return false;

  } else if (header.PT == transport::kPacketTypePayloadSpecific) {
    // Payload specific feedback
    switch (header.IC) {
      case 1:
        // PLI
        field_type_ = kRtcpPayloadSpecificPliCode;
        field_.pli.sender_ssrc = sender_ssrc;
        field_.pli.media_ssrc = media_ssrc;

        // Note: No state transition, PLI FCI is empty!
        return true;
      case 2:
        // Sli
        break;
      case 3:
        field_type_ = kRtcpPayloadSpecificRpsiCode;
        field_.rpsi.sender_ssrc = sender_ssrc;
        field_.rpsi.media_ssrc = media_ssrc;
        state_ = kStatePayloadSpecificRpsi;
        return true;
      case 4:
        // fir
        break;
      case 15:
        field_type_ = kRtcpPayloadSpecificAppCode;
        field_.application_specific.sender_ssrc = sender_ssrc;
        field_.application_specific.media_ssrc = media_ssrc;
        state_ = kStatePayloadSpecificApplication;
        return true;
      default:
        break;
    }

    EndCurrentBlock();
    return false;
  } else {
    DCHECK(false) << "Invalid state";
    EndCurrentBlock();
    return false;
  }
}

bool RtcpParser::ParseRpsiItem() {
  // RFC 4585 6.3.3.  Reference Picture Selection Indication (rpsi)
  /*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      PB       |0| Payload Type|    Native rpsi bit string     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   defined per codec          ...                | Padding (0) |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   */
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;

  if (length < 4) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }
  if (length > 2 + kRtcpRpsiDataSize) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }

  field_type_ = kRtcpPayloadSpecificRpsiCode;

  uint8 padding_bits;
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU8(&padding_bits);
  big_endian_reader.ReadU8(&field_.rpsi.payload_type);
  big_endian_reader.ReadBytes(&field_.rpsi.native_bit_string, length - 2);
  field_.rpsi.number_of_valid_bits =
      static_cast<uint16>(length - 2) * 8 - padding_bits;

  rtcp_data_ += length;
  return true;
}

bool RtcpParser::ParseNackItem() {
  // RFC 4585 6.2.1. Generic Nack

  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 4) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }

  field_type_ = kRtcpGenericRtpFeedbackNackItemCode;

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU16(&field_.nack_item.packet_id);
  big_endian_reader.ReadU16(&field_.nack_item.bitmask);
  rtcp_data_ += 4;
  return true;
}

bool RtcpParser::ParsePayloadSpecificAppItem() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;

  if (length < 4) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }
  uint32 name;
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU32(&name);
  rtcp_data_ += 4;

  if (name == kRemb) {
    field_type_ = kRtcpPayloadSpecificRembCode;
    state_ = kStatePayloadSpecificRemb;
    return true;
  } else if (name == kCast) {
    field_type_ = kRtcpPayloadSpecificCastCode;
    state_ = kStatePayloadSpecificCast;
    return true;
  }
  state_ = kStateTopLevel;
  EndCurrentBlock();
  return false;
}

bool RtcpParser::ParsePayloadSpecificRembItem() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;

  if (length < 4) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU8(&field_.remb_item.number_of_ssrcs);

  uint8 byte_1;
  uint8 byte_2;
  uint8 byte_3;
  big_endian_reader.ReadU8(&byte_1);
  big_endian_reader.ReadU8(&byte_2);
  big_endian_reader.ReadU8(&byte_3);
  rtcp_data_ += 4;

  uint8 br_exp = (byte_1 >> 2) & 0x3F;
  uint32 br_mantissa = ((byte_1 & 0x03) << 16) + (byte_2 << 8) + byte_3;
  field_.remb_item.bitrate = (br_mantissa << br_exp);

  ptrdiff_t length_ssrcs = rtcp_block_end_ - rtcp_data_;
  if (length_ssrcs < 4 * field_.remb_item.number_of_ssrcs) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }

  field_type_ = kRtcpPayloadSpecificRembItemCode;

  for (int i = 0; i < field_.remb_item.number_of_ssrcs; i++) {
    big_endian_reader.ReadU32(&field_.remb_item.ssrcs[i]);
  }
  return true;
}

bool RtcpParser::ParsePayloadSpecificCastItem() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 4) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }
  field_type_ = kRtcpPayloadSpecificCastCode;

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU8(&field_.cast_item.last_frame_id);
  big_endian_reader.ReadU8(&field_.cast_item.number_of_lost_fields);
  big_endian_reader.ReadU16(&field_.cast_item.target_delay_ms);

  rtcp_data_ += 4;

  if (field_.cast_item.number_of_lost_fields != 0) {
    // State transition
    state_ = kStatePayloadSpecificCastNack;
  } else {
    // Don't go to state cast nack item if got 0 fields.
    state_ = kStateTopLevel;
    EndCurrentBlock();
  }
  return true;
}

bool RtcpParser::ParsePayloadSpecificCastNackItem() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 4) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }
  field_type_ = kRtcpPayloadSpecificCastNackItemCode;

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU8(&field_.cast_nack_item.frame_id);
  big_endian_reader.ReadU16(&field_.cast_nack_item.packet_id);
  big_endian_reader.ReadU8(&field_.cast_nack_item.bitmask);

  rtcp_data_ += 4;
  return true;
}

bool RtcpParser::ParseFirItem() {
  // RFC 5104 4.3.1. Full Intra Request (fir)
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;

  if (length < 8) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }
  field_type_ = kRtcpPayloadSpecificFirItemCode;

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU32(&field_.fir_item.ssrc);
  big_endian_reader.ReadU8(&field_.fir_item.command_sequence_number);

  rtcp_data_ += 8;
  return true;
}

bool RtcpParser::ParseExtendedReport() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 8)
    return false;

  field_type_ = kRtcpXrCode;

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.Skip(4);  // Skip header.
  big_endian_reader.ReadU32(&field_.extended_report.sender_ssrc);

  rtcp_data_ += 8;

  state_ = kStateExtendedReportBlock;
  return true;
}

bool RtcpParser::ParseExtendedReportItem() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 4) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }

  uint8 block_type;
  uint16 block_length;
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU8(&block_type);
  big_endian_reader.Skip(1);  // Ignore reserved.
  big_endian_reader.ReadU16(&block_length);

  rtcp_data_ += 4;

  switch (block_type) {
    case 4:
      if (block_length != 2) {
        // Invalid block length.
        state_ = kStateTopLevel;
        EndCurrentBlock();
        return false;
      }
      return ParseExtendedReportReceiverReferenceTimeReport();
    case 5:
      if (block_length % 3 != 0) {
        // Invalid block length.
        state_ = kStateTopLevel;
        EndCurrentBlock();
        return false;
      }
      if (block_length >= 3) {
        number_of_blocks_ = block_length / 3;
        state_ = kStateExtendedReportDelaySinceLastReceiverReport;
        return ParseExtendedReportDelaySinceLastReceiverReport();
      }
      return true;
    default:
      if (length < block_length * 4) {
        state_ = kStateTopLevel;
        EndCurrentBlock();
        return false;
      }
      field_type_ = kRtcpXrUnknownItemCode;
      rtcp_data_ += block_length * 4;
      return true;
  }
}

bool RtcpParser::ParseExtendedReportReceiverReferenceTimeReport() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 8) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU32(&field_.rrtr.ntp_most_significant);
  big_endian_reader.ReadU32(&field_.rrtr.ntp_least_significant);

  rtcp_data_ += 8;

  field_type_ = kRtcpXrRrtrCode;
  return true;
}

bool RtcpParser::ParseExtendedReportDelaySinceLastReceiverReport() {
  ptrdiff_t length = rtcp_block_end_ - rtcp_data_;
  if (length < 12) {
    state_ = kStateTopLevel;
    EndCurrentBlock();
    return false;
  }
  if (number_of_blocks_ == 0) {
    // Continue parsing the extended report block.
    state_ = kStateExtendedReportBlock;
    return false;
  }

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_data_), length);
  big_endian_reader.ReadU32(&field_.dlrr.receivers_ssrc);
  big_endian_reader.ReadU32(&field_.dlrr.last_receiver_report);
  big_endian_reader.ReadU32(&field_.dlrr.delay_last_receiver_report);

  rtcp_data_ += 12;

  number_of_blocks_--;
  field_type_ = kRtcpXrDlrrCode;
  return true;
}

// Converts a log event type to an integer value.
// NOTE: We have only allocated 4 bits to represent the type of event over the
// wire. Therefore, this function can only return values from 0 to 15.
uint8 ConvertEventTypeToWireFormat(CastLoggingEvent event) {
  switch (event) {
    case FRAME_ACK_SENT:
      return 11;
    case FRAME_PLAYOUT:
      return 12;
    case FRAME_DECODED:
      return 13;
    case PACKET_RECEIVED:
      return 14;
    default:
      return 0;  // Not an interesting event.
  }
}

CastLoggingEvent TranslateToLogEventFromWireFormat(uint8 event) {
  // TODO(imcheng): Remove the old mappings once they are no longer used.
  switch (event) {
    case 1:  // AudioAckSent
    case 5:  // VideoAckSent
    case 11:  // Unified
      return FRAME_ACK_SENT;
    case 2:  // AudioPlayoutDelay
    case 7:  // VideoRenderDelay
    case 12:  // Unified
      return FRAME_PLAYOUT;
    case 3:  // AudioFrameDecoded
    case 6:  // VideoFrameDecoded
    case 13:  // Unified
      return FRAME_DECODED;
    case 4:  // AudioPacketReceived
    case 8:  // VideoPacketReceived
    case 14:  // Unified
      return PACKET_RECEIVED;
    case 9:  // DuplicateAudioPacketReceived
    case 10:  // DuplicateVideoPacketReceived
    default:
      // If the sender adds new log messages we will end up here until we add
      // the new messages in the receiver.
      VLOG(1) << "Unexpected log message received: " << static_cast<int>(event);
      NOTREACHED();
      return UNKNOWN;
  }
}

}  // namespace cast
}  // namespace media
