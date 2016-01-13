// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_FRAMER_FRAME_ID_MAP_H_
#define MEDIA_CAST_FRAMER_FRAME_ID_MAP_H_

#include <map>
#include <set>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

class FrameInfo {
 public:
  FrameInfo(uint32 frame_id,
            uint32 referenced_frame_id,
            uint16 max_packet_id,
            bool key_frame);
  ~FrameInfo();

  PacketType InsertPacket(uint16 packet_id);
  bool Complete() const;
  void GetMissingPackets(bool newest_frame, PacketIdSet* missing_packets) const;

  bool is_key_frame() const { return is_key_frame_; }
  uint32 frame_id() const { return frame_id_; }
  uint32 referenced_frame_id() const { return referenced_frame_id_; }

 private:
  const bool is_key_frame_;
  const uint32 frame_id_;
  const uint32 referenced_frame_id_;

  uint16 max_received_packet_id_;
  PacketIdSet missing_packets_;

  DISALLOW_COPY_AND_ASSIGN(FrameInfo);
};

typedef std::map<uint32, linked_ptr<FrameInfo> > FrameMap;

class FrameIdMap {
 public:
  FrameIdMap();
  ~FrameIdMap();

  PacketType InsertPacket(const RtpCastHeader& rtp_header);

  bool Empty() const;
  bool FrameExists(uint32 frame_id) const;
  uint32 NewestFrameId() const;

  void RemoveOldFrames(uint32 frame_id);
  void Clear();

  // Identifies the next frame to be released (rendered).
  bool NextContinuousFrame(uint32* frame_id) const;
  uint32 LastContinuousFrame() const;

  bool NextFrameAllowingSkippingFrames(uint32* frame_id) const;
  bool HaveMultipleDecodableFrames() const;

  int NumberOfCompleteFrames() const;
  void GetMissingPackets(uint32 frame_id,
                         bool last_frame,
                         PacketIdSet* missing_packets) const;

 private:
  bool ContinuousFrame(FrameInfo* frame) const;
  bool DecodableFrame(FrameInfo* frame) const;

  FrameMap frame_map_;
  bool waiting_for_key_;
  uint32 last_released_frame_;
  uint32 newest_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(FrameIdMap);
};

}  //  namespace cast
}  //  namespace media

#endif  // MEDIA_CAST_FRAMER_FRAME_ID_MAP_H_
