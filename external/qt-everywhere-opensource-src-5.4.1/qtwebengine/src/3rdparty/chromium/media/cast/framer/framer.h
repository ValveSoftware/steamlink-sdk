// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_FRAMER_FRAMER_H_
#define MEDIA_CAST_FRAMER_FRAMER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/framer/cast_message_builder.h"
#include "media/cast/framer/frame_buffer.h"
#include "media/cast/framer/frame_id_map.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

typedef std::map<uint32, linked_ptr<FrameBuffer> > FrameList;

class Framer {
 public:
  Framer(base::TickClock* clock,
         RtpPayloadFeedback* incoming_payload_feedback,
         uint32 ssrc,
         bool decoder_faster_than_max_frame_rate,
         int max_unacked_frames);
  ~Framer();

  // Return true when receiving the last packet in a frame, creating a
  // complete frame. If a duplicate packet for an already complete frame is
  // received, the function returns false but sets |duplicate| to true.
  bool InsertPacket(const uint8* payload_data,
                    size_t payload_size,
                    const RtpCastHeader& rtp_header,
                    bool* duplicate);

  // Extracts a complete encoded frame - will only return a complete and
  // decodable frame. Returns false if no such frames exist.
  // |next_frame| will be set to true if the returned frame is the very
  // next frame. |have_multiple_complete_frames| will be set to true
  // if there are more decodadble frames available.
  bool GetEncodedFrame(transport::EncodedFrame* video_frame,
                       bool* next_frame,
                       bool* have_multiple_complete_frames);

  void ReleaseFrame(uint32 frame_id);

  // Reset framer state to original state and flush all pending buffers.
  void Reset();
  bool TimeToSendNextCastMessage(base::TimeTicks* time_to_send);
  void SendCastMessage();

 private:
  const bool decoder_faster_than_max_frame_rate_;
  FrameList frames_;
  FrameIdMap frame_id_map_;

  scoped_ptr<CastMessageBuilder> cast_msg_builder_;

  DISALLOW_COPY_AND_ASSIGN(Framer);
};

}  //  namespace cast
}  //  namespace media

#endif  // MEDIA_CAST_FRAMER_FRAMER_H_
