// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOGGING_RAW_H_
#define MEDIA_CAST_LOGGING_LOGGING_RAW_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/raw_event_subscriber.h"

namespace media {
namespace cast {

// This class is not thread safe, and should only be called from the main
// thread.
class LoggingRaw : public base::NonThreadSafe {
 public:
  LoggingRaw();
  ~LoggingRaw();

  // Inform of new event: two types of events: frame and packet.
  // Frame events can be inserted with different parameters.
  void InsertFrameEvent(const base::TimeTicks& time_of_event,
                        CastLoggingEvent event, EventMediaType event_media_type,
                        uint32 rtp_timestamp, uint32 frame_id);

  // This function is only applicable for FRAME_ENCODED event.
  // |size| - Size of encoded frame.
  // |key_frame| - Whether the frame is a key frame. This field is only
  //               applicable for video event.
  // |target_bitrate| - The target bitrate of the encoder the time the frame
  // was encoded. Only applicable for video event.
  void InsertEncodedFrameEvent(const base::TimeTicks& time_of_event,
                                CastLoggingEvent event,
                                EventMediaType event_media_type,
                                uint32 rtp_timestamp, uint32 frame_id,
                                int size, bool key_frame,
                                int target_bitrate);

  // Render/playout delay
  // This function is only applicable for FRAME_PLAYOUT event.
  void InsertFrameEventWithDelay(const base::TimeTicks& time_of_event,
                                 CastLoggingEvent event,
                                 EventMediaType event_media_type,
                                 uint32 rtp_timestamp,
                                 uint32 frame_id, base::TimeDelta delay);

  // Insert a packet event.
  void InsertPacketEvent(const base::TimeTicks& time_of_event,
                         CastLoggingEvent event,
                         EventMediaType event_media_type, uint32 rtp_timestamp,
                         uint32 frame_id, uint16 packet_id,
                         uint16 max_packet_id, size_t size);

  // Adds |subscriber| so that it will start receiving events on main thread.
  // Note that this class does not own |subscriber|.
  // It is a no-op to add a subscriber that already exists.
  void AddSubscriber(RawEventSubscriber* subscriber);

  // Removes |subscriber| so that it will stop receiving events.
  // Note that this class does NOT own the subscribers. This function MUST be
  // called before |subscriber| is destroyed if it was previously added.
  // It is a no-op to remove a subscriber that doesn't exist.
  void RemoveSubscriber(RawEventSubscriber* subscriber);

 private:
  void InsertBaseFrameEvent(const base::TimeTicks& time_of_event,
                            CastLoggingEvent event,
                            EventMediaType event_media_type,
                            uint32 frame_id, uint32 rtp_timestamp,
                            base::TimeDelta delay, int size, bool key_frame,
                            int target_bitrate);

  // List of subscriber pointers. This class does not own the subscribers.
  std::vector<RawEventSubscriber*> subscribers_;

  DISALLOW_COPY_AND_ASSIGN(LoggingRaw);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_RAW_H_
