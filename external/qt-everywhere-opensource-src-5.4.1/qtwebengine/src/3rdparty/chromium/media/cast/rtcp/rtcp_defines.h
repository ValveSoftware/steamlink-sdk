// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_RTCP_DEFINES_H_
#define MEDIA_CAST_RTCP_RTCP_DEFINES_H_

#include <map>
#include <set>

#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {

static const size_t kRtcpCastLogHeaderSize = 12;
static const size_t kRtcpReceiverFrameLogSize = 8;
static const size_t kRtcpReceiverEventLogSize = 4;

// Handle the per frame ACK and NACK messages.
class RtcpCastMessage {
 public:
  explicit RtcpCastMessage(uint32 media_ssrc);
  ~RtcpCastMessage();

  void Copy(const RtcpCastMessage& cast_message);

  uint32 media_ssrc_;
  uint32 ack_frame_id_;
  uint16 target_delay_ms_;
  MissingFramesAndPacketsMap missing_frames_and_packets_;

  DISALLOW_COPY_AND_ASSIGN(RtcpCastMessage);
};

// Log messages from receiver to sender.
struct RtcpReceiverEventLogMessage {
  RtcpReceiverEventLogMessage();
  ~RtcpReceiverEventLogMessage();

  CastLoggingEvent type;
  base::TimeTicks event_timestamp;
  base::TimeDelta delay_delta;
  uint16 packet_id;
};

typedef std::list<RtcpReceiverEventLogMessage> RtcpReceiverEventLogMessages;

struct RtcpReceiverFrameLogMessage {
  explicit RtcpReceiverFrameLogMessage(uint32 rtp_timestamp);
  ~RtcpReceiverFrameLogMessage();

  uint32 rtp_timestamp_;
  RtcpReceiverEventLogMessages event_log_messages_;

  // TODO(mikhal): Investigate what's the best way to allow adding
  // DISALLOW_COPY_AND_ASSIGN, as currently it contradicts the implementation
  // and possible changes have a big impact on design.
};

typedef std::list<RtcpReceiverFrameLogMessage> RtcpReceiverLogMessage;

struct RtcpRpsiMessage {
  RtcpRpsiMessage();
  ~RtcpRpsiMessage();

  uint32 remote_ssrc;
  uint8 payload_type;
  uint64 picture_id;
};

struct RtcpNackMessage {
  RtcpNackMessage();
  ~RtcpNackMessage();

  uint32 remote_ssrc;
  std::list<uint16> nack_list;

  DISALLOW_COPY_AND_ASSIGN(RtcpNackMessage);
};

struct RtcpRembMessage {
  RtcpRembMessage();
  ~RtcpRembMessage();

  uint32 remb_bitrate;
  std::list<uint32> remb_ssrcs;

  DISALLOW_COPY_AND_ASSIGN(RtcpRembMessage);
};

struct RtcpReceiverReferenceTimeReport {
  RtcpReceiverReferenceTimeReport();
  ~RtcpReceiverReferenceTimeReport();

  uint32 remote_ssrc;
  uint32 ntp_seconds;
  uint32 ntp_fraction;
};

inline bool operator==(RtcpReceiverReferenceTimeReport lhs,
                       RtcpReceiverReferenceTimeReport rhs) {
  return lhs.remote_ssrc == rhs.remote_ssrc &&
         lhs.ntp_seconds == rhs.ntp_seconds &&
         lhs.ntp_fraction == rhs.ntp_fraction;
}

// Struct used by raw event subscribers as an intermediate format before
// sending off to the other side via RTCP.
// (i.e., {Sender,Receiver}RtcpEventSubscriber)
struct RtcpEvent {
  RtcpEvent();
  ~RtcpEvent();

  CastLoggingEvent type;

  // Time of event logged.
  base::TimeTicks timestamp;

  // Render/playout delay. Only set for FRAME_PLAYOUT events.
  base::TimeDelta delay_delta;

  // Only set for packet events.
  uint16 packet_id;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTCP_RTCP_DEFINES_H_
