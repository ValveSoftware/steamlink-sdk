// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/cast_transport_config.h"

namespace media {
namespace cast {
namespace transport {

namespace {
const int kDefaultRtpMaxDelayMs = 100;
}  // namespace

RtpConfig::RtpConfig()
    : ssrc(0),
      max_delay_ms(kDefaultRtpMaxDelayMs),
      payload_type(0) {}

RtpConfig::~RtpConfig() {}

CastTransportRtpConfig::CastTransportRtpConfig()
    : max_outstanding_frames(-1) {}

CastTransportRtpConfig::~CastTransportRtpConfig() {}

CastTransportAudioConfig::CastTransportAudioConfig()
    : codec(kOpus), frequency(0), channels(0) {}

CastTransportAudioConfig::~CastTransportAudioConfig() {}

CastTransportVideoConfig::CastTransportVideoConfig() : codec(kVp8) {}

CastTransportVideoConfig::~CastTransportVideoConfig() {}

EncodedFrame::EncodedFrame()
    : dependency(UNKNOWN_DEPENDENCY),
      frame_id(0),
      referenced_frame_id(0),
      rtp_timestamp(0) {}

EncodedFrame::~EncodedFrame() {}

void EncodedFrame::CopyMetadataTo(EncodedFrame* dest) const {
  DCHECK(dest);
  dest->dependency = this->dependency;
  dest->frame_id = this->frame_id;
  dest->referenced_frame_id = this->referenced_frame_id;
  dest->rtp_timestamp = this->rtp_timestamp;
  dest->reference_time = this->reference_time;
}

RtcpSenderInfo::RtcpSenderInfo()
    : ntp_seconds(0),
      ntp_fraction(0),
      rtp_timestamp(0),
      send_packet_count(0),
      send_octet_count(0) {}
RtcpSenderInfo::~RtcpSenderInfo() {}

RtcpReportBlock::RtcpReportBlock()
    : remote_ssrc(0),
      media_ssrc(0),
      fraction_lost(0),
      cumulative_lost(0),
      extended_high_sequence_number(0),
      jitter(0),
      last_sr(0),
      delay_since_last_sr(0) {}
RtcpReportBlock::~RtcpReportBlock() {}

RtcpDlrrReportBlock::RtcpDlrrReportBlock()
    : last_rr(0), delay_since_last_rr(0) {}
RtcpDlrrReportBlock::~RtcpDlrrReportBlock() {}

SendRtcpFromRtpSenderData::SendRtcpFromRtpSenderData()
    : packet_type_flags(0), sending_ssrc(0) {}
SendRtcpFromRtpSenderData::~SendRtcpFromRtpSenderData() {}

}  // namespace transport
}  // namespace cast
}  // namespace media
