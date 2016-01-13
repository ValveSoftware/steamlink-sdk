// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_RTCP_RECEIVER_H_
#define MEDIA_CAST_RTCP_RTCP_RECEIVER_H_

#include <queue>

#include "base/containers/hash_tables.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {

class RtcpReceiverFeedback {
 public:
  virtual void OnReceivedSenderReport(
      const transport::RtcpSenderInfo& remote_sender_info) = 0;

  virtual void OnReceiverReferenceTimeReport(
      const RtcpReceiverReferenceTimeReport& remote_time_report) = 0;

  virtual void OnReceivedSendReportRequest() = 0;

  virtual void OnReceivedReceiverLog(
      const RtcpReceiverLogMessage& receiver_log) = 0;

  virtual ~RtcpReceiverFeedback() {}
};

class RtcpRttFeedback {
 public:
  virtual void OnReceivedDelaySinceLastReport(
      uint32 receivers_ssrc,
      uint32 last_report,
      uint32 delay_since_last_report) = 0;

  virtual ~RtcpRttFeedback() {}
};

class RtcpReceiver {
 public:
  explicit RtcpReceiver(scoped_refptr<CastEnvironment> cast_environment,
                        RtcpSenderFeedback* sender_feedback,
                        RtcpReceiverFeedback* receiver_feedback,
                        RtcpRttFeedback* rtt_feedback,
                        uint32 local_ssrc);
  virtual ~RtcpReceiver();

  void SetRemoteSSRC(uint32 ssrc);

  // Set the history size to record Cast receiver events. Event history is
  // used to remove duplicates. The history has no more than |size| events.
  void SetCastReceiverEventHistorySize(size_t size);

  void IncomingRtcpPacket(RtcpParser* rtcp_parser);

 private:
  void HandleSenderReport(RtcpParser* rtcp_parser);

  void HandleReceiverReport(RtcpParser* rtcp_parser);

  void HandleReportBlock(const RtcpField* rtcp_field, uint32 remote_ssrc);

  void HandleSDES(RtcpParser* rtcp_parser);
  void HandleSDESChunk(RtcpParser* rtcp_parser);

  void HandleBYE(RtcpParser* rtcp_parser);

  void HandleXr(RtcpParser* rtcp_parser);
  void HandleRrtr(RtcpParser* rtcp_parser, uint32 remote_ssrc);
  void HandleDlrr(RtcpParser* rtcp_parser);

  //  Generic RTP Feedback.
  void HandleNACK(RtcpParser* rtcp_parser);
  void HandleNACKItem(const RtcpField* rtcp_field,
                      std::list<uint16>* nack_sequence_numbers);

  void HandleSendReportRequest(RtcpParser* rtcp_parser);

  // Payload-specific.
  void HandlePLI(RtcpParser* rtcp_parser);

  void HandleSLI(RtcpParser* rtcp_parser);
  void HandleSLIItem(RtcpField* rtcpPacket);

  void HandleRpsi(RtcpParser* rtcp_parser);

  void HandleFIR(RtcpParser* rtcp_parser);
  void HandleFIRItem(const RtcpField* rtcp_field);

  void HandlePayloadSpecificApp(RtcpParser* rtcp_parser);
  void HandlePayloadSpecificRembItem(RtcpParser* rtcp_parser);
  void HandlePayloadSpecificCastItem(RtcpParser* rtcp_parser);
  void HandlePayloadSpecificCastNackItem(
      const RtcpField* rtcp_field,
      MissingFramesAndPacketsMap* missing_frames_and_packets);

  void HandleApplicationSpecificCastReceiverLog(RtcpParser* rtcp_parser);
  void HandleApplicationSpecificCastSenderLog(RtcpParser* rtcp_parser);
  void HandleApplicationSpecificCastReceiverEventLog(
      uint32 frame_rtp_timestamp,
      RtcpParser* rtcp_parser,
      RtcpReceiverEventLogMessages* event_log_messages);

  const uint32 ssrc_;
  uint32 remote_ssrc_;

  // Not owned by this class.
  RtcpSenderFeedback* const sender_feedback_;
  RtcpReceiverFeedback* const receiver_feedback_;
  RtcpRttFeedback* const rtt_feedback_;
  scoped_refptr<CastEnvironment> cast_environment_;

  transport::FrameIdWrapHelper ack_frame_id_wrap_helper_;

  // Maintains a history of receiver events.
  size_t receiver_event_history_size_;
  typedef std::pair<uint64, uint64> ReceiverEventKey;
  base::hash_set<ReceiverEventKey> receiver_event_key_set_;
  std::queue<ReceiverEventKey> receiver_event_key_queue_;

  DISALLOW_COPY_AND_ASSIGN(RtcpReceiver);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTCP_RTCP_RECEIVER_H_
