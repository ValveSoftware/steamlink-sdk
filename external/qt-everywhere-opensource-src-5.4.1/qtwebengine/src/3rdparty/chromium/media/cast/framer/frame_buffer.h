// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_FRAMER_FRAME_BUFFER
#define MEDIA_CAST_FRAMER_FRAME_BUFFER

#include <map>
#include <vector>

#include "media/cast/cast_config.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

typedef std::map<uint16, std::vector<uint8> > PacketMap;

class FrameBuffer {
 public:
  FrameBuffer();
  ~FrameBuffer();
  void InsertPacket(const uint8* payload_data,
                    size_t payload_size,
                    const RtpCastHeader& rtp_header);
  bool Complete() const;

  // If a frame is complete, sets the frame IDs and RTP timestamp in |frame|,
  // and also copies the data from all packets into the data field in |frame|.
  // Returns true if the frame was complete; false if incomplete and |frame|
  // remains unchanged.
  bool AssembleEncodedFrame(transport::EncodedFrame* frame) const;

  bool is_key_frame() const { return is_key_frame_; }

  uint32 last_referenced_frame_id() const { return last_referenced_frame_id_; }

 private:
  uint32 frame_id_;
  uint16 max_packet_id_;
  uint16 num_packets_received_;
  bool is_key_frame_;
  size_t total_data_size_;
  uint32 last_referenced_frame_id_;
  uint32 rtp_timestamp_;
  PacketMap packets_;

  DISALLOW_COPY_AND_ASSIGN(FrameBuffer);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_FRAMER_FRAME_BUFFER
