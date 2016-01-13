// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/framer/framer.h"

#include "base/logging.h"

namespace media {
namespace cast {

typedef FrameList::const_iterator ConstFrameIterator;

Framer::Framer(base::TickClock* clock,
               RtpPayloadFeedback* incoming_payload_feedback,
               uint32 ssrc,
               bool decoder_faster_than_max_frame_rate,
               int max_unacked_frames)
    : decoder_faster_than_max_frame_rate_(decoder_faster_than_max_frame_rate),
      cast_msg_builder_(
          new CastMessageBuilder(clock,
                                 incoming_payload_feedback,
                                 &frame_id_map_,
                                 ssrc,
                                 decoder_faster_than_max_frame_rate,
                                 max_unacked_frames)) {
  DCHECK(incoming_payload_feedback) << "Invalid argument";
}

Framer::~Framer() {}

bool Framer::InsertPacket(const uint8* payload_data,
                          size_t payload_size,
                          const RtpCastHeader& rtp_header,
                          bool* duplicate) {
  *duplicate = false;
  PacketType packet_type = frame_id_map_.InsertPacket(rtp_header);
  if (packet_type == kTooOldPacket) {
    return false;
  }
  if (packet_type == kDuplicatePacket) {
    VLOG(3) << "Packet already received, ignored: frame "
            << static_cast<int>(rtp_header.frame_id) << ", packet "
            << rtp_header.packet_id;
    *duplicate = true;
    return false;
  }

  // Does this packet belong to a new frame?
  FrameList::iterator it = frames_.find(rtp_header.frame_id);
  if (it == frames_.end()) {
    // New frame.
    linked_ptr<FrameBuffer> frame_buffer(new FrameBuffer());
    frame_buffer->InsertPacket(payload_data, payload_size, rtp_header);
    frames_.insert(std::make_pair(rtp_header.frame_id, frame_buffer));
  } else {
    // Insert packet to existing frame buffer.
    it->second->InsertPacket(payload_data, payload_size, rtp_header);
  }

  return packet_type == kNewPacketCompletingFrame;
}

// This does not release the frame.
bool Framer::GetEncodedFrame(transport::EncodedFrame* frame,
                             bool* next_frame,
                             bool* have_multiple_decodable_frames) {
  *have_multiple_decodable_frames = frame_id_map_.HaveMultipleDecodableFrames();

  uint32 frame_id;
  // Find frame id.
  if (frame_id_map_.NextContinuousFrame(&frame_id)) {
    // We have our next frame.
    *next_frame = true;
  } else {
    // Check if we can skip frames when our decoder is too slow.
    if (!decoder_faster_than_max_frame_rate_)
      return false;

    if (!frame_id_map_.NextFrameAllowingSkippingFrames(&frame_id)) {
      return false;
    }
    *next_frame = false;
  }

  if (*next_frame) {
    VLOG(2) << "ACK frame " << frame_id;
    cast_msg_builder_->CompleteFrameReceived(frame_id);
  }

  ConstFrameIterator it = frames_.find(frame_id);
  DCHECK(it != frames_.end());
  if (it == frames_.end())
    return false;

  return it->second->AssembleEncodedFrame(frame);
}

void Framer::Reset() {
  frame_id_map_.Clear();
  frames_.clear();
  cast_msg_builder_->Reset();
}

void Framer::ReleaseFrame(uint32 frame_id) {
  frame_id_map_.RemoveOldFrames(frame_id);
  frames_.erase(frame_id);

  // We have a frame - remove all frames with lower frame id.
  bool skipped_old_frame = false;
  FrameList::iterator it;
  for (it = frames_.begin(); it != frames_.end();) {
    if (IsOlderFrameId(it->first, frame_id)) {
      frames_.erase(it++);
      skipped_old_frame = true;
    } else {
      ++it;
    }
  }
  if (skipped_old_frame) {
    cast_msg_builder_->UpdateCastMessage();
  }
}

bool Framer::TimeToSendNextCastMessage(base::TimeTicks* time_to_send) {
  return cast_msg_builder_->TimeToSendNextCastMessage(time_to_send);
}

void Framer::SendCastMessage() { cast_msg_builder_->UpdateCastMessage(); }

}  // namespace cast
}  // namespace media
