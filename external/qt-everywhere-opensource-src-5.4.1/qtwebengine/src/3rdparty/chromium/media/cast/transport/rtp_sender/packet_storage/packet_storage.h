// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_RTP_SENDER_PACKET_STORAGE_PACKET_STORAGE_H_
#define MEDIA_CAST_TRANSPORT_RTP_SENDER_PACKET_STORAGE_PACKET_STORAGE_H_

#include <deque>
#include <list>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {
namespace transport {

// Stores a list of frames. Each frame consists a list of packets.
typedef std::deque<SendPacketVector> FrameQueue;

class PacketStorage {
 public:
  explicit PacketStorage(size_t stored_frames);
  virtual ~PacketStorage();

  // Returns true if this class is configured correctly.
  // (stored frames > 0 && stored_frames < kMaxStoredFrames)
  bool IsValid() const;

  // Store all of the packets for a frame.
  void StoreFrame(uint32 frame_id, const SendPacketVector& packets);

  // Returns a list of packets for a frame indexed by a 8-bits ID.
  // It is the lowest 8 bits of a frame ID.
  // Returns NULL if the frame cannot be found.
  const SendPacketVector* GetFrame8(uint8 frame_id_8bits) const;

  // Get the number of stored frames.
  size_t GetNumberOfStoredFrames() const;

 private:
  const size_t max_stored_frames_;
  FrameQueue frames_;
  uint32 first_frame_id_in_list_;
  uint32 last_frame_id_in_list_;

  DISALLOW_COPY_AND_ASSIGN(PacketStorage);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_RTP_SENDER_PACKET_STORAGE_PACKET_STORAGE_H_
