// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtcp/rtcp_defines.h"

#include "media/cast/logging/logging_defines.h"

namespace media {
namespace cast {

RtcpCastMessage::RtcpCastMessage(uint32 media_ssrc)
    : media_ssrc_(media_ssrc), ack_frame_id_(0u), target_delay_ms_(0) {}
RtcpCastMessage::~RtcpCastMessage() {}

void RtcpCastMessage::Copy(const RtcpCastMessage& cast_message) {
  media_ssrc_ = cast_message.media_ssrc_;
  ack_frame_id_ = cast_message.ack_frame_id_;
  target_delay_ms_ = cast_message.target_delay_ms_;
  missing_frames_and_packets_ = cast_message.missing_frames_and_packets_;
}

RtcpReceiverEventLogMessage::RtcpReceiverEventLogMessage()
    : type(UNKNOWN), packet_id(0u) {}
RtcpReceiverEventLogMessage::~RtcpReceiverEventLogMessage() {}

RtcpReceiverFrameLogMessage::RtcpReceiverFrameLogMessage(uint32 timestamp)
    : rtp_timestamp_(timestamp) {}
RtcpReceiverFrameLogMessage::~RtcpReceiverFrameLogMessage() {}

RtcpRpsiMessage::RtcpRpsiMessage()
    : remote_ssrc(0u), payload_type(0u), picture_id(0u) {}
RtcpRpsiMessage::~RtcpRpsiMessage() {}

RtcpNackMessage::RtcpNackMessage() : remote_ssrc(0u) {}
RtcpNackMessage::~RtcpNackMessage() {}

RtcpRembMessage::RtcpRembMessage() : remb_bitrate(0u) {}
RtcpRembMessage::~RtcpRembMessage() {}

RtcpReceiverReferenceTimeReport::RtcpReceiverReferenceTimeReport()
    : remote_ssrc(0u), ntp_seconds(0u), ntp_fraction(0u) {}
RtcpReceiverReferenceTimeReport::~RtcpReceiverReferenceTimeReport() {}

RtcpEvent::RtcpEvent() : type(UNKNOWN), packet_id(0u) {}
RtcpEvent::~RtcpEvent() {}

}  // namespace cast
}  // namespace media
