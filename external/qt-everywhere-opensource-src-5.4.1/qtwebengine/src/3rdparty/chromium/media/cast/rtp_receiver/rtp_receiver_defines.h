// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_RECEIVER_RTP_RECEIVER_DEFINES_H_
#define MEDIA_CAST_RTP_RECEIVER_RTP_RECEIVER_DEFINES_H_

#include "base/basictypes.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

struct RtpCastHeader {
  RtpCastHeader();

  // Elements from RTP packet header.
  bool marker;
  uint8 payload_type;
  uint16 sequence_number;
  uint32 rtp_timestamp;
  uint32 sender_ssrc;

  // Elements from Cast header (at beginning of RTP payload).
  bool is_key_frame;
  uint32 frame_id;
  uint16 packet_id;
  uint16 max_packet_id;
  uint32 reference_frame_id;
};

class RtpPayloadFeedback {
 public:
  virtual void CastFeedback(const RtcpCastMessage& cast_feedback) = 0;

 protected:
  virtual ~RtpPayloadFeedback();
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_RECEIVER_RTP_RECEIVER_DEFINES_H_
