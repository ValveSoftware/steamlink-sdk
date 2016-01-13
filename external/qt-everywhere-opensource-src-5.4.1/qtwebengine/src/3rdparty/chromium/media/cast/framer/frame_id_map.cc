// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/framer/frame_id_map.h"

#include "base/logging.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

FrameInfo::FrameInfo(uint32 frame_id,
                     uint32 referenced_frame_id,
                     uint16 max_packet_id,
                     bool key_frame)
    : is_key_frame_(key_frame),
      frame_id_(frame_id),
      referenced_frame_id_(referenced_frame_id),
      max_received_packet_id_(0) {
  // Create the set with all packets missing.
  for (uint16 i = 0; i <= max_packet_id; i++) {
    missing_packets_.insert(i);
  }
}

FrameInfo::~FrameInfo() {}

PacketType FrameInfo::InsertPacket(uint16 packet_id) {
  if (missing_packets_.find(packet_id) == missing_packets_.end()) {
    return kDuplicatePacket;
  }
  // Update the last received packet id.
  if (IsNewerPacketId(packet_id, max_received_packet_id_)) {
    max_received_packet_id_ = packet_id;
  }
  missing_packets_.erase(packet_id);
  return missing_packets_.empty() ? kNewPacketCompletingFrame : kNewPacket;
}

bool FrameInfo::Complete() const { return missing_packets_.empty(); }

void FrameInfo::GetMissingPackets(bool newest_frame,
                                  PacketIdSet* missing_packets) const {
  if (newest_frame) {
    // Missing packets capped by max_received_packet_id_.
    PacketIdSet::const_iterator it_after_last_received =
        missing_packets_.lower_bound(max_received_packet_id_);
    missing_packets->insert(missing_packets_.begin(), it_after_last_received);
  } else {
    missing_packets->insert(missing_packets_.begin(), missing_packets_.end());
  }
}

FrameIdMap::FrameIdMap()
    : waiting_for_key_(true),
      last_released_frame_(kStartFrameId),
      newest_frame_id_(kStartFrameId) {}

FrameIdMap::~FrameIdMap() {}

PacketType FrameIdMap::InsertPacket(const RtpCastHeader& rtp_header) {
  uint32 frame_id = rtp_header.frame_id;
  uint32 reference_frame_id;
  reference_frame_id = rtp_header.reference_frame_id;

  if (rtp_header.is_key_frame && waiting_for_key_) {
    last_released_frame_ = static_cast<uint32>(frame_id - 1);
    waiting_for_key_ = false;
  }

  VLOG(3) << "InsertPacket frame:" << frame_id
          << " packet:" << static_cast<int>(rtp_header.packet_id)
          << " max packet:" << static_cast<int>(rtp_header.max_packet_id);

  if (IsOlderFrameId(frame_id, last_released_frame_) && !waiting_for_key_) {
    return kTooOldPacket;
  }

  // Update the last received frame id.
  if (IsNewerFrameId(frame_id, newest_frame_id_)) {
    newest_frame_id_ = frame_id;
  }

  // Does this packet belong to a new frame?
  FrameMap::iterator it = frame_map_.find(frame_id);
  PacketType packet_type;
  if (it == frame_map_.end()) {
    // New frame.
    linked_ptr<FrameInfo> frame_info(new FrameInfo(frame_id,
                                                   reference_frame_id,
                                                   rtp_header.max_packet_id,
                                                   rtp_header.is_key_frame));
    std::pair<FrameMap::iterator, bool> retval =
        frame_map_.insert(std::make_pair(frame_id, frame_info));

    packet_type = retval.first->second->InsertPacket(rtp_header.packet_id);
  } else {
    // Insert packet to existing frame.
    packet_type = it->second->InsertPacket(rtp_header.packet_id);
  }
  return packet_type;
}

void FrameIdMap::RemoveOldFrames(uint32 frame_id) {
  FrameMap::iterator it = frame_map_.begin();

  while (it != frame_map_.end()) {
    if (IsNewerFrameId(it->first, frame_id)) {
      ++it;
    } else {
      // Older or equal; erase.
      frame_map_.erase(it++);
    }
  }
  last_released_frame_ = frame_id;
}

void FrameIdMap::Clear() {
  frame_map_.clear();
  waiting_for_key_ = true;
  last_released_frame_ = kStartFrameId;
  newest_frame_id_ = kStartFrameId;
}

uint32 FrameIdMap::NewestFrameId() const { return newest_frame_id_; }

bool FrameIdMap::NextContinuousFrame(uint32* frame_id) const {
  FrameMap::const_iterator it;

  for (it = frame_map_.begin(); it != frame_map_.end(); ++it) {
    if (it->second->Complete() && ContinuousFrame(it->second.get())) {
      *frame_id = it->first;
      return true;
    }
  }
  return false;
}

bool FrameIdMap::HaveMultipleDecodableFrames() const {
  // Find the oldest decodable frame.
  FrameMap::const_iterator it;
  bool found_one = false;
  for (it = frame_map_.begin(); it != frame_map_.end(); ++it) {
    if (it->second->Complete() && DecodableFrame(it->second.get())) {
      if (found_one) {
        return true;
      } else {
        found_one = true;
      }
    }
  }
  return false;
}

uint32 FrameIdMap::LastContinuousFrame() const {
  uint32 last_continuous_frame_id = last_released_frame_;
  uint32 next_expected_frame = last_released_frame_;

  FrameMap::const_iterator it;

  do {
    next_expected_frame++;
    it = frame_map_.find(next_expected_frame);
    if (it == frame_map_.end())
      break;
    if (!it->second->Complete())
      break;

    // We found the next continuous frame.
    last_continuous_frame_id = it->first;
  } while (next_expected_frame != newest_frame_id_);
  return last_continuous_frame_id;
}

bool FrameIdMap::NextFrameAllowingSkippingFrames(uint32* frame_id) const {
  // Find the oldest decodable frame.
  FrameMap::const_iterator it_best_match = frame_map_.end();
  FrameMap::const_iterator it;
  for (it = frame_map_.begin(); it != frame_map_.end(); ++it) {
    if (it->second->Complete() && DecodableFrame(it->second.get())) {
      if (it_best_match == frame_map_.end() ||
          IsOlderFrameId(it->first, it_best_match->first)) {
        it_best_match = it;
      }
    }
  }
  if (it_best_match == frame_map_.end())
    return false;

  *frame_id = it_best_match->first;
  return true;
}

bool FrameIdMap::Empty() const { return frame_map_.empty(); }

int FrameIdMap::NumberOfCompleteFrames() const {
  int count = 0;
  FrameMap::const_iterator it;
  for (it = frame_map_.begin(); it != frame_map_.end(); ++it) {
    if (it->second->Complete()) {
      ++count;
    }
  }
  return count;
}

bool FrameIdMap::FrameExists(uint32 frame_id) const {
  return frame_map_.end() != frame_map_.find(frame_id);
}

void FrameIdMap::GetMissingPackets(uint32 frame_id,
                                   bool last_frame,
                                   PacketIdSet* missing_packets) const {
  FrameMap::const_iterator it = frame_map_.find(frame_id);
  if (it == frame_map_.end())
    return;

  it->second->GetMissingPackets(last_frame, missing_packets);
}

bool FrameIdMap::ContinuousFrame(FrameInfo* frame) const {
  DCHECK(frame);
  if (waiting_for_key_ && !frame->is_key_frame())
    return false;
  return static_cast<uint32>(last_released_frame_ + 1) == frame->frame_id();
}

bool FrameIdMap::DecodableFrame(FrameInfo* frame) const {
  if (frame->is_key_frame())
    return true;
  if (waiting_for_key_ && !frame->is_key_frame())
    return false;
  // Self-reference?
  if (frame->referenced_frame_id() == frame->frame_id())
    return true;

  // Current frame is not necessarily referencing the last frame.
  // Do we have the reference frame?
  if (IsOlderFrameId(frame->referenced_frame_id(), last_released_frame_)) {
    return true;
  }
  return frame->referenced_frame_id() == last_released_frame_;
}

}  //  namespace cast
}  //  namespace media
