// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_RTCP_SENDER_H_
#define MEDIA_CAST_RTCP_RTCP_SENDER_H_

#include <deque>
#include <list>
#include <string>

#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/rtcp/rtcp_builder.h"

namespace media {
namespace cast {

// We limit the size of receiver logs to avoid queuing up packets.
const size_t kMaxReceiverLogBytes = 200;

// The determines how long to hold receiver log events, based on how
// many "receiver log message reports" ago the events were sent.
const size_t kReceiveLogMessageHistorySize = 20;

// This determines when to send events the second time.
const size_t kFirstRedundancyOffset = 10;
COMPILE_ASSERT(kFirstRedundancyOffset > 0 &&
                   kFirstRedundancyOffset <= kReceiveLogMessageHistorySize,
               redundancy_offset_out_of_range);

// When to send events the third time.
const size_t kSecondRedundancyOffset = 20;
COMPILE_ASSERT(kSecondRedundancyOffset >
                   kFirstRedundancyOffset && kSecondRedundancyOffset <=
                   kReceiveLogMessageHistorySize,
               redundancy_offset_out_of_range);

// TODO(mikhal): Resolve duplication between this and RtcpBuilder.
class RtcpSender {
 public:
  RtcpSender(scoped_refptr<CastEnvironment> cast_environment,
             transport::PacedPacketSender* outgoing_transport,
             uint32 sending_ssrc,
             const std::string& c_name);

  virtual ~RtcpSender();

  void SendRtcpFromRtpReceiver(
      uint32 packet_type_flags,
      const transport::RtcpReportBlock* report_block,
      const RtcpReceiverReferenceTimeReport* rrtr,
      const RtcpCastMessage* cast_message,
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap* rtcp_events,
      uint16 target_delay_ms);

 private:
  void BuildRR(const transport::RtcpReportBlock* report_block,
               Packet* packet) const;

  void AddReportBlocks(const transport::RtcpReportBlock& report_block,
                       Packet* packet) const;

  void BuildSdec(Packet* packet) const;

  void BuildPli(uint32 remote_ssrc, Packet* packet) const;

  void BuildRemb(const RtcpRembMessage* remb, Packet* packet) const;

  void BuildRpsi(const RtcpRpsiMessage* rpsi, Packet* packet) const;

  void BuildNack(const RtcpNackMessage* nack, Packet* packet) const;

  void BuildBye(Packet* packet) const;

  void BuildRrtr(const RtcpReceiverReferenceTimeReport* rrtr,
                 Packet* packet) const;

  void BuildCast(const RtcpCastMessage* cast_message,
                 uint16 target_delay_ms,
                 Packet* packet) const;

  void BuildReceiverLog(
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap& rtcp_events,
      Packet* packet);

  bool BuildRtcpReceiverLogMessage(
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap& rtcp_events,
      size_t start_size,
      RtcpReceiverLogMessage* receiver_log_message,
      size_t* number_of_frames,
      size_t* total_number_of_messages_to_send,
      size_t* rtcp_log_size);

  inline void BitrateToRembExponentBitrate(uint32 bitrate,
                                           uint8* exponent,
                                           uint32* mantissa) const {
    // 6 bit exponent and a 18 bit mantissa.
    *exponent = 0;
    for (int i = 0; i < 64; ++i) {
      if (bitrate <= (262143u << i)) {
        *exponent = i;
        break;
      }
    }
    *mantissa = (bitrate >> *exponent);
  }

  const uint32 ssrc_;
  const std::string c_name_;

  // Not owned by this class.
  transport::PacedPacketSender* const transport_;
  scoped_refptr<CastEnvironment> cast_environment_;

  std::deque<RtcpReceiverLogMessage> rtcp_events_history_;

  DISALLOW_COPY_AND_ASSIGN(RtcpSender);
};

}  // namespace cast
}  // namespace media
#endif  // MEDIA_CAST_RTCP_RTCP_SENDER_H_
