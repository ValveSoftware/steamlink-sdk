// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/framer/frame_buffer.h"

#include "base/logging.h"

namespace media {
namespace cast {

FrameBuffer::FrameBuffer()
    : frame_id_(0),
      max_packet_id_(0),
      num_packets_received_(0),
      is_key_frame_(false),
      total_data_size_(0),
      last_referenced_frame_id_(0),
      packets_() {}

FrameBuffer::~FrameBuffer() {}

void FrameBuffer::InsertPacket(const uint8* payload_data,
                               size_t payload_size,
                               const RtpCastHeader& rtp_header) {
  // Is this the first packet in the frame?
  if (packets_.empty()) {
    frame_id_ = rtp_header.frame_id;
    max_packet_id_ = rtp_header.max_packet_id;
    is_key_frame_ = rtp_header.is_key_frame;
    if (is_key_frame_)
      DCHECK_EQ(rtp_header.frame_id, rtp_header.reference_frame_id);
    last_referenced_frame_id_ = rtp_header.reference_frame_id;
    rtp_timestamp_ = rtp_header.rtp_timestamp;
  }
  // Is this the correct frame?
  if (rtp_header.frame_id != frame_id_)
    return;

  // Insert every packet only once.
  if (packets_.find(rtp_header.packet_id) != packets_.end()) {
    return;
  }

  std::vector<uint8> data;
  std::pair<PacketMap::iterator, bool> retval =
      packets_.insert(make_pair(rtp_header.packet_id, data));

  // Insert the packet.
  retval.first->second.resize(payload_size);
  std::copy(
      payload_data, payload_data + payload_size, retval.first->second.begin());

  ++num_packets_received_;
  total_data_size_ += payload_size;
}

bool FrameBuffer::Complete() const {
  return num_packets_received_ - 1 == max_packet_id_;
}

bool FrameBuffer::AssembleEncodedFrame(transport::EncodedFrame* frame) const {
  if (!Complete())
    return false;

  // Frame is complete -> construct.
  if (is_key_frame_)
    frame->dependency = transport::EncodedFrame::KEY;
  else if (frame_id_ == last_referenced_frame_id_)
    frame->dependency = transport::EncodedFrame::INDEPENDENT;
  else
    frame->dependency = transport::EncodedFrame::DEPENDENT;
  frame->frame_id = frame_id_;
  frame->referenced_frame_id = last_referenced_frame_id_;
  frame->rtp_timestamp = rtp_timestamp_;

  // Build the data vector.
  frame->data.clear();
  frame->data.reserve(total_data_size_);
  PacketMap::const_iterator it;
  for (it = packets_.begin(); it != packets_.end(); ++it)
    frame->data.insert(frame->data.end(), it->second.begin(), it->second.end());
  return true;
}

}  // namespace cast
}  // namespace media
