// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles NACK list and manages ACK.

#ifndef MEDIA_CAST_FRAMER_CAST_MESSAGE_BUILDER_H_
#define MEDIA_CAST_FRAMER_CAST_MESSAGE_BUILDER_H_

#include <deque>
#include <map>

#include "media/cast/framer/frame_id_map.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

class RtpPayloadFeedback;

typedef std::map<uint32, base::TimeTicks> TimeLastNackMap;

class CastMessageBuilder {
 public:
  CastMessageBuilder(base::TickClock* clock,
                     RtpPayloadFeedback* incoming_payload_feedback,
                     FrameIdMap* frame_id_map,
                     uint32 media_ssrc,
                     bool decoder_faster_than_max_frame_rate,
                     int max_unacked_frames);
  ~CastMessageBuilder();

  void CompleteFrameReceived(uint32 frame_id);
  bool TimeToSendNextCastMessage(base::TimeTicks* time_to_send);
  void UpdateCastMessage();
  void Reset();

 private:
  bool UpdateAckMessage(uint32 frame_id);
  void BuildPacketList();
  bool UpdateCastMessageInternal(RtcpCastMessage* message);

  base::TickClock* const clock_;  // Not owned by this class.
  RtpPayloadFeedback* const cast_feedback_;

  // CastMessageBuilder has only const access to the frame id mapper.
  const FrameIdMap* const frame_id_map_;
  const uint32 media_ssrc_;
  const bool decoder_faster_than_max_frame_rate_;
  const int max_unacked_frames_;

  RtcpCastMessage cast_msg_;
  base::TimeTicks last_update_time_;

  TimeLastNackMap time_last_nacked_map_;

  bool slowing_down_ack_;
  bool acked_last_frame_;
  uint32 last_acked_frame_id_;
  std::deque<uint32> ack_queue_;

  DISALLOW_COPY_AND_ASSIGN(CastMessageBuilder);
};

}  // namespace cast
}  // namespace media

#endif  //  MEDIA_CAST_FRAMER_CAST_MESSAGE_BUILDER_H_
