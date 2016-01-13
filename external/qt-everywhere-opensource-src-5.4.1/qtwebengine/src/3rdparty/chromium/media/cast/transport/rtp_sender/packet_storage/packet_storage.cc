// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/rtp_sender/packet_storage/packet_storage.h"

#include <string>

#include "base/logging.h"

namespace media {
namespace cast {
namespace transport {

PacketStorage::PacketStorage(size_t stored_frames)
    : max_stored_frames_(stored_frames),
      first_frame_id_in_list_(0),
      last_frame_id_in_list_(0) {
}

PacketStorage::~PacketStorage() {
}

bool PacketStorage::IsValid() const {
  return max_stored_frames_ > 0 &&
      static_cast<int>(max_stored_frames_) <= kMaxUnackedFrames;
}

size_t PacketStorage::GetNumberOfStoredFrames() const {
  return frames_.size();
}

void PacketStorage::StoreFrame(uint32 frame_id,
                               const SendPacketVector& packets) {
  if (frames_.empty()) {
    first_frame_id_in_list_ = frame_id;
  } else {
    // Make sure frame IDs are consecutive.
    DCHECK_EQ(last_frame_id_in_list_ + 1, frame_id);
  }

  // Save new frame to the end of the list.
  last_frame_id_in_list_ = frame_id;
  frames_.push_back(packets);

  // Evict the oldest frame if the list is too long.
  if (frames_.size() > max_stored_frames_) {
    frames_.pop_front();
    ++first_frame_id_in_list_;
  }
}

const SendPacketVector* PacketStorage::GetFrame8(uint8 frame_id_8bits) const {
  // The requested frame ID has only 8-bits so convert the first frame ID
  // in list to match.
  uint8 index_8bits = first_frame_id_in_list_ & 0xFF;
  index_8bits = frame_id_8bits - index_8bits;
  if (index_8bits >= frames_.size())
    return NULL;
  return &(frames_[index_8bits]);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
