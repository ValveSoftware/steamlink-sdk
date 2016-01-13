// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_RTCP_UTILITY_H_
#define MEDIA_CAST_RTCP_RTCP_UTILITY_H_

#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

static const int kRtcpRpsiDataSize = 30;

// RFC 3550 page 44, including end null.
static const size_t kRtcpCnameSize = 256;
static const int kRtcpMaxNumberOfRembFeedbackSsrcs = 255;

static const uint32 kRemb = ('R' << 24) + ('E' << 16) + ('M' << 8) + 'B';
static const uint32 kCast = ('C' << 24) + ('A' << 16) + ('S' << 8) + 'T';

static const uint8 kReceiverLogSubtype = 2;

static const size_t kRtcpMaxReceiverLogMessages = 256;
static const size_t kRtcpMaxNackFields = 253;
static const size_t kRtcpMaxCastLossFields = 100;

struct RtcpFieldReceiverReport {
  // RFC 3550.
  uint32 sender_ssrc;
  uint8 number_of_report_blocks;
};

struct RtcpFieldSenderReport {
  // RFC 3550.
  uint32 sender_ssrc;
  uint8 number_of_report_blocks;
  uint32 ntp_most_significant;
  uint32 ntp_least_significant;
  uint32 rtp_timestamp;
  uint32 sender_packet_count;
  uint32 sender_octet_count;
};

struct RtcpFieldReportBlockItem {
  // RFC 3550.
  uint32 ssrc;
  uint8 fraction_lost;
  uint32 cumulative_number_of_packets_lost;
  uint32 extended_highest_sequence_number;
  uint32 jitter;
  uint32 last_sender_report;
  uint32 delay_last_sender_report;
};

struct RtcpFieldSdesCName {
  // RFC 3550
  uint32 sender_ssrc;
  char name[kRtcpCnameSize];
};

struct RtcpFieldBye {
  // RFC 3550.
  uint32 sender_ssrc;
};

struct RtcpFieldGenericRtpFeedbackNack {
  // RFC 4585.
  uint32 sender_ssrc;
  uint32 media_ssrc;
};

struct RtcpFieldGenericRtpFeedbackNackItem {
  // RFC 4585.
  uint16 packet_id;
  uint16 bitmask;
};

struct RtcpFieldPayloadSpecificFir {
  // RFC 5104.
  uint32 sender_ssrc;
  uint32 media_ssrc;  // zero!
};

struct RtcpFieldPayloadSpecificFirItem {
  // RFC 5104.
  uint32 ssrc;
  uint8 command_sequence_number;
};

struct RtcpFieldPayloadSpecificPli {
  // RFC 4585.
  uint32 sender_ssrc;
  uint32 media_ssrc;
};

struct RtcpFieldPayloadSpecificRpsi {
  // RFC 4585.
  uint32 sender_ssrc;
  uint32 media_ssrc;
  uint8 payload_type;
  uint16 number_of_valid_bits;
  uint8 native_bit_string[kRtcpRpsiDataSize];
};

struct RtcpFieldXr {
  // RFC 3611.
  uint32 sender_ssrc;
};

struct RtcpFieldXrRrtr {
  // RFC 3611.
  uint32 ntp_most_significant;
  uint32 ntp_least_significant;
};

struct RtcpFieldXrDlrr {
  // RFC 3611.
  uint32 receivers_ssrc;
  uint32 last_receiver_report;
  uint32 delay_last_receiver_report;
};

struct RtcpFieldPayloadSpecificApplication {
  uint32 sender_ssrc;
  uint32 media_ssrc;
};

struct RtcpFieldPayloadSpecificRembItem {
  uint32 bitrate;
  uint8 number_of_ssrcs;
  uint32 ssrcs[kRtcpMaxNumberOfRembFeedbackSsrcs];
};

struct RtcpFieldPayloadSpecificCastItem {
  uint8 last_frame_id;
  uint8 number_of_lost_fields;
  uint16 target_delay_ms;
};

struct RtcpFieldPayloadSpecificCastNackItem {
  uint8 frame_id;
  uint16 packet_id;
  uint8 bitmask;
};

struct RtcpFieldApplicationSpecificCastReceiverLogItem {
  uint32 sender_ssrc;
  uint32 rtp_timestamp;
  uint32 event_timestamp_base;
  uint8 event;
  union {
    uint16 packet_id;
    int16 delay_delta;
  } delay_delta_or_packet_id;
  uint16 event_timestamp_delta;
};

union RtcpField {
  RtcpFieldReceiverReport receiver_report;
  RtcpFieldSenderReport sender_report;
  RtcpFieldReportBlockItem report_block_item;
  RtcpFieldSdesCName c_name;
  RtcpFieldBye bye;

  RtcpFieldXr extended_report;
  RtcpFieldXrRrtr rrtr;
  RtcpFieldXrDlrr dlrr;

  RtcpFieldGenericRtpFeedbackNack nack;
  RtcpFieldGenericRtpFeedbackNackItem nack_item;

  RtcpFieldPayloadSpecificPli pli;
  RtcpFieldPayloadSpecificRpsi rpsi;
  RtcpFieldPayloadSpecificFir fir;
  RtcpFieldPayloadSpecificFirItem fir_item;
  RtcpFieldPayloadSpecificApplication application_specific;
  RtcpFieldPayloadSpecificRembItem remb_item;
  RtcpFieldPayloadSpecificCastItem cast_item;
  RtcpFieldPayloadSpecificCastNackItem cast_nack_item;

  RtcpFieldApplicationSpecificCastReceiverLogItem cast_receiver_log;
};

enum RtcpFieldTypes {
  kRtcpNotValidCode,

  // RFC 3550.
  kRtcpRrCode,
  kRtcpSrCode,
  kRtcpReportBlockItemCode,
  kRtcpSdesCode,
  kRtcpSdesChunkCode,
  kRtcpByeCode,

  // RFC 3611.
  kRtcpXrCode,
  kRtcpXrRrtrCode,
  kRtcpXrDlrrCode,
  kRtcpXrUnknownItemCode,

  // RFC 4585.
  kRtcpGenericRtpFeedbackNackCode,
  kRtcpGenericRtpFeedbackNackItemCode,
  kRtcpPayloadSpecificPliCode,
  kRtcpPayloadSpecificRpsiCode,
  kRtcpPayloadSpecificAppCode,

  // Application specific.
  kRtcpPayloadSpecificRembCode,
  kRtcpPayloadSpecificRembItemCode,
  kRtcpPayloadSpecificCastCode,
  kRtcpPayloadSpecificCastNackItemCode,
  kRtcpApplicationSpecificCastReceiverLogCode,
  kRtcpApplicationSpecificCastReceiverLogFrameCode,
  kRtcpApplicationSpecificCastReceiverLogEventCode,

  // RFC 5104.
  kRtcpPayloadSpecificFirCode,
  kRtcpPayloadSpecificFirItemCode,

  // RFC 6051.
  kRtcpGenericRtpFeedbackSrReqCode,
};

struct RtcpCommonHeader {
  uint8 V;   // Version.
  bool P;    // Padding.
  uint8 IC;  // Item count / subtype.
  uint8 PT;  // Packet Type.
  uint16 length_in_octets;
};

class RtcpParser {
 public:
  RtcpParser(const uint8* rtcp_data, size_t rtcp_length);
  ~RtcpParser();

  RtcpFieldTypes FieldType() const;
  const RtcpField& Field() const;

  bool IsValid() const;

  RtcpFieldTypes Begin();
  RtcpFieldTypes Iterate();

 private:
  enum ParseState {
    kStateTopLevel,     // Top level packet
    kStateReportBlock,  // Sender/Receiver report report blocks.
    kStateSdes,
    kStateBye,
    kStateApplicationSpecificCastReceiverFrameLog,
    kStateApplicationSpecificCastReceiverEventLog,
    kStateExtendedReportBlock,
    kStateExtendedReportDelaySinceLastReceiverReport,
    kStateGenericRtpFeedbackNack,
    kStatePayloadSpecificRpsi,
    kStatePayloadSpecificFir,
    kStatePayloadSpecificApplication,
    kStatePayloadSpecificRemb,      // Application specific Remb.
    kStatePayloadSpecificCast,      // Application specific Cast.
    kStatePayloadSpecificCastNack,  // Application specific Nack for Cast.
  };

  bool RtcpParseCommonHeader(const uint8* begin,
                             const uint8* end,
                             RtcpCommonHeader* parsed_header) const;

  void IterateTopLevel();
  void IterateReportBlockItem();
  void IterateSdesItem();
  void IterateByeItem();
  void IterateCastReceiverLogFrame();
  void IterateCastReceiverLogEvent();
  void IterateExtendedReportItem();
  void IterateExtendedReportDelaySinceLastReceiverReportItem();
  void IterateNackItem();
  void IterateRpsiItem();
  void IterateFirItem();
  void IteratePayloadSpecificAppItem();
  void IteratePayloadSpecificRembItem();
  void IteratePayloadSpecificCastItem();
  void IteratePayloadSpecificCastNackItem();

  void Validate();
  void EndCurrentBlock();

  bool ParseRR();
  bool ParseSR();
  bool ParseReportBlockItem();

  bool ParseSdes();
  bool ParseSdesItem();
  bool ParseSdesTypes();
  bool ParseBye();
  bool ParseByeItem();
  bool ParseApplicationDefined(uint8 subtype);
  bool ParseCastReceiverLogFrameItem();
  bool ParseCastReceiverLogEventItem();

  bool ParseExtendedReport();
  bool ParseExtendedReportItem();
  bool ParseExtendedReportReceiverReferenceTimeReport();
  bool ParseExtendedReportDelaySinceLastReceiverReport();

  bool ParseFeedBackCommon(const RtcpCommonHeader& header);
  bool ParseNackItem();
  bool ParseRpsiItem();
  bool ParseFirItem();
  bool ParsePayloadSpecificAppItem();
  bool ParsePayloadSpecificRembItem();
  bool ParsePayloadSpecificCastItem();
  bool ParsePayloadSpecificCastNackItem();

 private:
  const uint8* const rtcp_data_begin_;
  const uint8* const rtcp_data_end_;

  bool valid_packet_;
  const uint8* rtcp_data_;
  const uint8* rtcp_block_end_;

  ParseState state_;
  uint8 number_of_blocks_;
  RtcpFieldTypes field_type_;
  RtcpField field_;

  DISALLOW_COPY_AND_ASSIGN(RtcpParser);
};

// Converts a log event type to an integer value.
// NOTE: We have only allocated 4 bits to represent the type of event over the
// wire. Therefore, this function can only return values from 0 to 15.
uint8 ConvertEventTypeToWireFormat(CastLoggingEvent event);

// The inverse of |ConvertEventTypeToWireFormat()|.
CastLoggingEvent TranslateToLogEventFromWireFormat(uint8 event);

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTCP_RTCP_UTILITY_H_
