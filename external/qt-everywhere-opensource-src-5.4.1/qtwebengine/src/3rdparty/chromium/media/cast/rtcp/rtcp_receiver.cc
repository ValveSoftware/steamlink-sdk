// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtcp/rtcp_receiver.h"

#include "base/logging.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace {

// A receiver frame event is identified by frame RTP timestamp, event timestamp
// and event type.
// A receiver packet event is identified by all of the above plus packet id.
// The key format is as follows:
// First uint64:
//   bits 0-11: zeroes (unused).
//   bits 12-15: event type ID.
//   bits 16-31: packet ID if packet event, 0 otherwise.
//   bits 32-63: RTP timestamp.
// Second uint64:
//   bits 0-63: event TimeTicks internal value.
std::pair<uint64, uint64> GetReceiverEventKey(
    uint32 frame_rtp_timestamp, const base::TimeTicks& event_timestamp,
    uint8 event_type, uint16 packet_id_or_zero) {
  uint64 value1 = event_type;
  value1 <<= 16;
  value1 |= packet_id_or_zero;
  value1 <<= 32;
  value1 |= frame_rtp_timestamp;
  return std::make_pair(
      value1, static_cast<uint64>(event_timestamp.ToInternalValue()));
}

}  // namespace

namespace media {
namespace cast {

RtcpReceiver::RtcpReceiver(scoped_refptr<CastEnvironment> cast_environment,
                           RtcpSenderFeedback* sender_feedback,
                           RtcpReceiverFeedback* receiver_feedback,
                           RtcpRttFeedback* rtt_feedback,
                           uint32 local_ssrc)
    : ssrc_(local_ssrc),
      remote_ssrc_(0),
      sender_feedback_(sender_feedback),
      receiver_feedback_(receiver_feedback),
      rtt_feedback_(rtt_feedback),
      cast_environment_(cast_environment),
      receiver_event_history_size_(0) {}

RtcpReceiver::~RtcpReceiver() {}

void RtcpReceiver::SetRemoteSSRC(uint32 ssrc) { remote_ssrc_ = ssrc; }

void RtcpReceiver::SetCastReceiverEventHistorySize(size_t size) {
  receiver_event_history_size_ = size;
}

void RtcpReceiver::IncomingRtcpPacket(RtcpParser* rtcp_parser) {
  RtcpFieldTypes field_type = rtcp_parser->Begin();
  while (field_type != kRtcpNotValidCode) {
    // Each "case" is responsible for iterate the parser to the next top
    // level packet.
    switch (field_type) {
      case kRtcpSrCode:
        HandleSenderReport(rtcp_parser);
        break;
      case kRtcpRrCode:
        HandleReceiverReport(rtcp_parser);
        break;
      case kRtcpSdesCode:
        HandleSDES(rtcp_parser);
        break;
      case kRtcpByeCode:
        HandleBYE(rtcp_parser);
        break;
      case kRtcpXrCode:
        HandleXr(rtcp_parser);
        break;
      case kRtcpGenericRtpFeedbackNackCode:
        HandleNACK(rtcp_parser);
        break;
      case kRtcpGenericRtpFeedbackSrReqCode:
        HandleSendReportRequest(rtcp_parser);
        break;
      case kRtcpPayloadSpecificPliCode:
        HandlePLI(rtcp_parser);
        break;
      case kRtcpPayloadSpecificRpsiCode:
        HandleRpsi(rtcp_parser);
        break;
      case kRtcpPayloadSpecificFirCode:
        HandleFIR(rtcp_parser);
        break;
      case kRtcpPayloadSpecificAppCode:
        HandlePayloadSpecificApp(rtcp_parser);
        break;
      case kRtcpApplicationSpecificCastReceiverLogCode:
        HandleApplicationSpecificCastReceiverLog(rtcp_parser);
        break;
      case kRtcpPayloadSpecificRembCode:
      case kRtcpPayloadSpecificRembItemCode:
      case kRtcpPayloadSpecificCastCode:
      case kRtcpPayloadSpecificCastNackItemCode:
      case kRtcpApplicationSpecificCastReceiverLogFrameCode:
      case kRtcpApplicationSpecificCastReceiverLogEventCode:
      case kRtcpNotValidCode:
      case kRtcpReportBlockItemCode:
      case kRtcpSdesChunkCode:
      case kRtcpGenericRtpFeedbackNackItemCode:
      case kRtcpPayloadSpecificFirItemCode:
      case kRtcpXrRrtrCode:
      case kRtcpXrDlrrCode:
      case kRtcpXrUnknownItemCode:
        rtcp_parser->Iterate();
        NOTREACHED() << "Invalid state";
        break;
    }
    field_type = rtcp_parser->FieldType();
  }
}

void RtcpReceiver::HandleSenderReport(RtcpParser* rtcp_parser) {
  RtcpFieldTypes rtcp_field_type = rtcp_parser->FieldType();
  const RtcpField& rtcp_field = rtcp_parser->Field();

  DCHECK(rtcp_field_type == kRtcpSrCode) << "Invalid state";

  // Synchronization source identifier for the originator of this SR packet.
  uint32 remote_ssrc = rtcp_field.sender_report.sender_ssrc;

  VLOG(2) << "Cast RTCP received SR from SSRC " << remote_ssrc;

  if (remote_ssrc_ == remote_ssrc) {
    transport::RtcpSenderInfo remote_sender_info;
    remote_sender_info.ntp_seconds =
        rtcp_field.sender_report.ntp_most_significant;
    remote_sender_info.ntp_fraction =
        rtcp_field.sender_report.ntp_least_significant;
    remote_sender_info.rtp_timestamp = rtcp_field.sender_report.rtp_timestamp;
    remote_sender_info.send_packet_count =
        rtcp_field.sender_report.sender_packet_count;
    remote_sender_info.send_octet_count =
        rtcp_field.sender_report.sender_octet_count;
    if (receiver_feedback_) {
      receiver_feedback_->OnReceivedSenderReport(remote_sender_info);
    }
  }
  rtcp_field_type = rtcp_parser->Iterate();
  while (rtcp_field_type == kRtcpReportBlockItemCode) {
    HandleReportBlock(&rtcp_field, remote_ssrc);
    rtcp_field_type = rtcp_parser->Iterate();
  }
}

void RtcpReceiver::HandleReceiverReport(RtcpParser* rtcp_parser) {
  RtcpFieldTypes rtcp_field_type = rtcp_parser->FieldType();
  const RtcpField& rtcp_field = rtcp_parser->Field();

  DCHECK(rtcp_field_type == kRtcpRrCode) << "Invalid state";

  uint32 remote_ssrc = rtcp_field.receiver_report.sender_ssrc;

  VLOG(2) << "Cast RTCP received RR from SSRC " << remote_ssrc;

  rtcp_field_type = rtcp_parser->Iterate();
  while (rtcp_field_type == kRtcpReportBlockItemCode) {
    HandleReportBlock(&rtcp_field, remote_ssrc);
    rtcp_field_type = rtcp_parser->Iterate();
  }
}

void RtcpReceiver::HandleReportBlock(const RtcpField* rtcp_field,
                                     uint32 remote_ssrc) {
  // This will be called once per report block in the Rtcp packet.
  // We filter out all report blocks that are not for us.
  // Each packet has max 31 RR blocks.
  //
  // We can calculate RTT if we send a send report and get a report block back.

  // |rtcp_field.ReportBlockItem.ssrc| is the ssrc identifier of the source to
  // which the information in this reception report block pertains.

  const RtcpFieldReportBlockItem& rb = rtcp_field->report_block_item;

  // Filter out all report blocks that are not for us.
  if (rb.ssrc != ssrc_) {
    // This block is not for us ignore it.
    return;
  }
  VLOG(2) << "Cast RTCP received RB from SSRC " << remote_ssrc;

  transport::RtcpReportBlock report_block;
  report_block.remote_ssrc = remote_ssrc;
  report_block.media_ssrc = rb.ssrc;
  report_block.fraction_lost = rb.fraction_lost;
  report_block.cumulative_lost = rb.cumulative_number_of_packets_lost;
  report_block.extended_high_sequence_number =
      rb.extended_highest_sequence_number;
  report_block.jitter = rb.jitter;
  report_block.last_sr = rb.last_sender_report;
  report_block.delay_since_last_sr = rb.delay_last_sender_report;

  if (rtt_feedback_) {
    rtt_feedback_->OnReceivedDelaySinceLastReport(
        rb.ssrc, rb.last_sender_report, rb.delay_last_sender_report);
  }
}

void RtcpReceiver::HandleSDES(RtcpParser* rtcp_parser) {
  RtcpFieldTypes field_type = rtcp_parser->Iterate();
  while (field_type == kRtcpSdesChunkCode) {
    HandleSDESChunk(rtcp_parser);
    field_type = rtcp_parser->Iterate();
  }
}

void RtcpReceiver::HandleSDESChunk(RtcpParser* rtcp_parser) {
  const RtcpField& rtcp_field = rtcp_parser->Field();
  VLOG(2) << "Cast RTCP received SDES with cname " << rtcp_field.c_name.name;
}

void RtcpReceiver::HandleXr(RtcpParser* rtcp_parser) {
  RtcpFieldTypes rtcp_field_type = rtcp_parser->FieldType();
  const RtcpField& rtcp_field = rtcp_parser->Field();

  DCHECK(rtcp_field_type == kRtcpXrCode) << "Invalid state";

  uint32 remote_ssrc = rtcp_field.extended_report.sender_ssrc;
  rtcp_field_type = rtcp_parser->Iterate();

  while (rtcp_field_type == kRtcpXrDlrrCode ||
         rtcp_field_type == kRtcpXrRrtrCode ||
         rtcp_field_type == kRtcpXrUnknownItemCode) {
    if (rtcp_field_type == kRtcpXrRrtrCode) {
      HandleRrtr(rtcp_parser, remote_ssrc);
    } else if (rtcp_field_type == kRtcpXrDlrrCode) {
      HandleDlrr(rtcp_parser);
    }
    rtcp_field_type = rtcp_parser->Iterate();
  }
}

void RtcpReceiver::HandleRrtr(RtcpParser* rtcp_parser, uint32 remote_ssrc) {
  if (remote_ssrc_ != remote_ssrc) {
    // Not to us.
    return;
  }
  const RtcpField& rtcp_field = rtcp_parser->Field();
  RtcpReceiverReferenceTimeReport remote_time_report;
  remote_time_report.remote_ssrc = remote_ssrc;
  remote_time_report.ntp_seconds = rtcp_field.rrtr.ntp_most_significant;
  remote_time_report.ntp_fraction = rtcp_field.rrtr.ntp_least_significant;

  if (receiver_feedback_) {
    receiver_feedback_->OnReceiverReferenceTimeReport(remote_time_report);
  }
}

void RtcpReceiver::HandleDlrr(RtcpParser* rtcp_parser) {
  const RtcpField& rtcp_field = rtcp_parser->Field();
  if (remote_ssrc_ != rtcp_field.dlrr.receivers_ssrc) {
    // Not to us.
    return;
  }
  if (rtt_feedback_) {
    rtt_feedback_->OnReceivedDelaySinceLastReport(
        rtcp_field.dlrr.receivers_ssrc,
        rtcp_field.dlrr.last_receiver_report,
        rtcp_field.dlrr.delay_last_receiver_report);
  }
}

void RtcpReceiver::HandleNACK(RtcpParser* rtcp_parser) {
  const RtcpField& rtcp_field = rtcp_parser->Field();
  if (ssrc_ != rtcp_field.nack.media_ssrc) {
    RtcpFieldTypes field_type;
    // Message not to us. Iterate until we have passed this message.
    do {
      field_type = rtcp_parser->Iterate();
    } while (field_type == kRtcpGenericRtpFeedbackNackItemCode);
    return;
  }
  std::list<uint16> nackSequenceNumbers;

  RtcpFieldTypes field_type = rtcp_parser->Iterate();
  while (field_type == kRtcpGenericRtpFeedbackNackItemCode) {
    HandleNACKItem(&rtcp_field, &nackSequenceNumbers);
    field_type = rtcp_parser->Iterate();
  }
}

void RtcpReceiver::HandleNACKItem(const RtcpField* rtcp_field,
                                  std::list<uint16>* nack_sequence_numbers) {
  nack_sequence_numbers->push_back(rtcp_field->nack_item.packet_id);

  uint16 bitmask = rtcp_field->nack_item.bitmask;
  if (bitmask) {
    for (int i = 1; i <= 16; ++i) {
      if (bitmask & 1) {
        nack_sequence_numbers->push_back(rtcp_field->nack_item.packet_id + i);
      }
      bitmask = bitmask >> 1;
    }
  }
}

void RtcpReceiver::HandleBYE(RtcpParser* rtcp_parser) {
  const RtcpField& rtcp_field = rtcp_parser->Field();
  uint32 remote_ssrc = rtcp_field.bye.sender_ssrc;
  if (remote_ssrc_ == remote_ssrc) {
    VLOG(2) << "Cast RTCP received BYE from SSRC " << remote_ssrc;
  }
  rtcp_parser->Iterate();
}

void RtcpReceiver::HandlePLI(RtcpParser* rtcp_parser) {
  const RtcpField& rtcp_field = rtcp_parser->Field();
  if (ssrc_ == rtcp_field.pli.media_ssrc) {
    // Received a signal that we need to send a new key frame.
    VLOG(2) << "Cast RTCP received PLI on our SSRC " << ssrc_;
  }
  rtcp_parser->Iterate();
}

void RtcpReceiver::HandleSendReportRequest(RtcpParser* rtcp_parser) {
  if (receiver_feedback_) {
    receiver_feedback_->OnReceivedSendReportRequest();
  }
  rtcp_parser->Iterate();
}

void RtcpReceiver::HandleRpsi(RtcpParser* rtcp_parser) {
  const RtcpField& rtcp_field = rtcp_parser->Field();
  if (rtcp_parser->Iterate() != kRtcpPayloadSpecificRpsiCode) {
    return;
  }
  if (rtcp_field.rpsi.number_of_valid_bits % 8 != 0) {
    // Continue
    return;
  }
  uint64 rpsi_picture_id = 0;

  // Convert native_bit_string to rpsi_picture_id
  uint8 bytes = rtcp_field.rpsi.number_of_valid_bits / 8;
  for (uint8 n = 0; n < (bytes - 1); ++n) {
    rpsi_picture_id += (rtcp_field.rpsi.native_bit_string[n] & 0x7f);
    rpsi_picture_id <<= 7;  // Prepare next.
  }
  rpsi_picture_id += (rtcp_field.rpsi.native_bit_string[bytes - 1] & 0x7f);

  VLOG(2) << "Cast RTCP received RPSI with picture_id " << rpsi_picture_id;
}

void RtcpReceiver::HandlePayloadSpecificApp(RtcpParser* rtcp_parser) {
  const RtcpField& rtcp_field = rtcp_parser->Field();
  uint32 remote_ssrc = rtcp_field.application_specific.sender_ssrc;
  if (remote_ssrc_ != remote_ssrc) {
    // Message not to us. Iterate until we have passed this message.
    RtcpFieldTypes field_type;
    do {
      field_type = rtcp_parser->Iterate();
    } while (field_type == kRtcpPayloadSpecificRembCode ||
             field_type == kRtcpPayloadSpecificRembItemCode ||
             field_type == kRtcpPayloadSpecificCastCode ||
             field_type == kRtcpPayloadSpecificCastNackItemCode);
    return;
  }

  RtcpFieldTypes packet_type = rtcp_parser->Iterate();
  switch (packet_type) {
    case kRtcpPayloadSpecificRembCode:
      packet_type = rtcp_parser->Iterate();
      if (packet_type == kRtcpPayloadSpecificRembItemCode) {
        HandlePayloadSpecificRembItem(rtcp_parser);
        rtcp_parser->Iterate();
      }
      break;
    case kRtcpPayloadSpecificCastCode:
      packet_type = rtcp_parser->Iterate();
      if (packet_type == kRtcpPayloadSpecificCastCode) {
        HandlePayloadSpecificCastItem(rtcp_parser);
      }
      break;
    default:
      return;
  }
}

void RtcpReceiver::HandlePayloadSpecificRembItem(RtcpParser* rtcp_parser) {
  const RtcpField& rtcp_field = rtcp_parser->Field();

  for (int i = 0; i < rtcp_field.remb_item.number_of_ssrcs; ++i) {
    if (rtcp_field.remb_item.ssrcs[i] == ssrc_) {
      // Found matching ssrc.
      VLOG(2) << "Cast RTCP received REMB with received_bitrate "
              << rtcp_field.remb_item.bitrate;
      return;
    }
  }
}

void RtcpReceiver::HandleApplicationSpecificCastReceiverLog(
    RtcpParser* rtcp_parser) {
  const RtcpField& rtcp_field = rtcp_parser->Field();

  uint32 remote_ssrc = rtcp_field.cast_receiver_log.sender_ssrc;
  if (remote_ssrc_ != remote_ssrc) {
    // Message not to us. Iterate until we have passed this message.
    RtcpFieldTypes field_type;
    do {
      field_type = rtcp_parser->Iterate();
    } while (field_type == kRtcpApplicationSpecificCastReceiverLogFrameCode ||
             field_type == kRtcpApplicationSpecificCastReceiverLogEventCode);
    return;
  }
  RtcpReceiverLogMessage receiver_log;
  RtcpFieldTypes field_type = rtcp_parser->Iterate();
  while (field_type == kRtcpApplicationSpecificCastReceiverLogFrameCode) {
    RtcpReceiverFrameLogMessage frame_log(
        rtcp_field.cast_receiver_log.rtp_timestamp);

    field_type = rtcp_parser->Iterate();
    while (field_type == kRtcpApplicationSpecificCastReceiverLogEventCode) {
      HandleApplicationSpecificCastReceiverEventLog(
          rtcp_field.cast_receiver_log.rtp_timestamp,
          rtcp_parser,
          &frame_log.event_log_messages_);
      field_type = rtcp_parser->Iterate();
    }

    if (!frame_log.event_log_messages_.empty())
      receiver_log.push_back(frame_log);
  }

  if (receiver_feedback_ && !receiver_log.empty()) {
    receiver_feedback_->OnReceivedReceiverLog(receiver_log);
  }
}

void RtcpReceiver::HandleApplicationSpecificCastReceiverEventLog(
    uint32 frame_rtp_timestamp,
    RtcpParser* rtcp_parser,
    RtcpReceiverEventLogMessages* event_log_messages) {
  const RtcpField& rtcp_field = rtcp_parser->Field();

  const uint8 event = rtcp_field.cast_receiver_log.event;
  const CastLoggingEvent event_type = TranslateToLogEventFromWireFormat(event);
  uint16 packet_id = event_type == PACKET_RECEIVED ?
      rtcp_field.cast_receiver_log.delay_delta_or_packet_id.packet_id : 0;
  const base::TimeTicks event_timestamp =
      base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(
          rtcp_field.cast_receiver_log.event_timestamp_base +
          rtcp_field.cast_receiver_log.event_timestamp_delta);

  // The following code checks to see if we have already seen this event.
  // The algorithm works by maintaining a sliding window of events. We have
  // a queue and a set of events. We enqueue every new event and insert it
  // into the set. When the queue becomes too big we remove the oldest event
  // from both the queue and the set.
  ReceiverEventKey key =
      GetReceiverEventKey(
          frame_rtp_timestamp, event_timestamp, event, packet_id);
  if (receiver_event_key_set_.find(key) != receiver_event_key_set_.end()) {
    return;
  } else {
    receiver_event_key_set_.insert(key);
    receiver_event_key_queue_.push(key);

    if (receiver_event_key_queue_.size() > receiver_event_history_size_) {
      const ReceiverEventKey oldest_key = receiver_event_key_queue_.front();
      receiver_event_key_queue_.pop();
      receiver_event_key_set_.erase(oldest_key);
    }
  }

  RtcpReceiverEventLogMessage event_log;
  event_log.type = event_type;
  event_log.event_timestamp = event_timestamp;
  event_log.delay_delta = base::TimeDelta::FromMilliseconds(
      rtcp_field.cast_receiver_log.delay_delta_or_packet_id.delay_delta);
  event_log.packet_id =
      rtcp_field.cast_receiver_log.delay_delta_or_packet_id.packet_id;
  event_log_messages->push_back(event_log);
}

void RtcpReceiver::HandlePayloadSpecificCastItem(RtcpParser* rtcp_parser) {
  const RtcpField& rtcp_field = rtcp_parser->Field();
  RtcpCastMessage cast_message(remote_ssrc_);
  cast_message.ack_frame_id_ = ack_frame_id_wrap_helper_.MapTo32bitsFrameId(
      rtcp_field.cast_item.last_frame_id);
  cast_message.target_delay_ms_ = rtcp_field.cast_item.target_delay_ms;

  RtcpFieldTypes packet_type = rtcp_parser->Iterate();
  while (packet_type == kRtcpPayloadSpecificCastNackItemCode) {
    const RtcpField& rtcp_field = rtcp_parser->Field();
    HandlePayloadSpecificCastNackItem(
        &rtcp_field, &cast_message.missing_frames_and_packets_);
    packet_type = rtcp_parser->Iterate();
  }
  if (sender_feedback_) {
    sender_feedback_->OnReceivedCastFeedback(cast_message);
  }
}

void RtcpReceiver::HandlePayloadSpecificCastNackItem(
    const RtcpField* rtcp_field,
    MissingFramesAndPacketsMap* missing_frames_and_packets) {

  MissingFramesAndPacketsMap::iterator frame_it =
      missing_frames_and_packets->find(rtcp_field->cast_nack_item.frame_id);

  if (frame_it == missing_frames_and_packets->end()) {
    // First missing packet in a frame.
    PacketIdSet empty_set;
    std::pair<MissingFramesAndPacketsMap::iterator, bool> ret =
        missing_frames_and_packets->insert(std::pair<uint8, PacketIdSet>(
            rtcp_field->cast_nack_item.frame_id, empty_set));
    frame_it = ret.first;
    DCHECK(frame_it != missing_frames_and_packets->end()) << "Invalid state";
  }
  uint16 packet_id = rtcp_field->cast_nack_item.packet_id;
  frame_it->second.insert(packet_id);

  if (packet_id == kRtcpCastAllPacketsLost) {
    // Special case all packets in a frame is missing.
    return;
  }
  uint8 bitmask = rtcp_field->cast_nack_item.bitmask;

  if (bitmask) {
    for (int i = 1; i <= 8; ++i) {
      if (bitmask & 1) {
        frame_it->second.insert(packet_id + i);
      }
      bitmask = bitmask >> 1;
    }
  }
}

void RtcpReceiver::HandleFIR(RtcpParser* rtcp_parser) {
  const RtcpField& rtcp_field = rtcp_parser->Field();

  RtcpFieldTypes field_type = rtcp_parser->Iterate();
  while (field_type == kRtcpPayloadSpecificFirItemCode) {
    HandleFIRItem(&rtcp_field);
    field_type = rtcp_parser->Iterate();
  }
}

void RtcpReceiver::HandleFIRItem(const RtcpField* rtcp_field) {
  // Is it our sender that is requested to generate a new keyframe.
  if (ssrc_ != rtcp_field->fir_item.ssrc)
    return;

  VLOG(2) << "Cast RTCP received FIR on our SSRC " << ssrc_;
}

}  // namespace cast
}  // namespace media
