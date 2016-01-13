// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_
#define MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace media {
namespace cast {

static const uint32 kFrameIdUnknown = 0xFFFFFFFF;

typedef uint32 RtpTimestamp;

enum CastLoggingEvent {
  UNKNOWN,
  // Sender side frame events.
  FRAME_CAPTURE_BEGIN,
  FRAME_CAPTURE_END,
  FRAME_ENCODED,
  FRAME_ACK_RECEIVED,
  // Receiver side frame events.
  FRAME_ACK_SENT,
  FRAME_DECODED,
  FRAME_PLAYOUT,
  // Sender side packet events.
  PACKET_SENT_TO_NETWORK,
  PACKET_RETRANSMITTED,
  PACKET_RTX_REJECTED,
  // Receiver side packet events.
  PACKET_RECEIVED,
  kNumOfLoggingEvents = PACKET_RECEIVED
};

const char* CastLoggingToString(CastLoggingEvent event);

// CastLoggingEvent are classified into one of three following types.
enum EventMediaType {
  AUDIO_EVENT,
  VIDEO_EVENT,
  UNKNOWN_EVENT,
  EVENT_MEDIA_TYPE_LAST = UNKNOWN_EVENT
};

struct FrameEvent {
  FrameEvent();
  ~FrameEvent();

  RtpTimestamp rtp_timestamp;
  uint32 frame_id;

  // Size of encoded frame. Only set for FRAME_ENCODED event.
  size_t size;

  // Time of event logged.
  base::TimeTicks timestamp;

  CastLoggingEvent type;

  EventMediaType media_type;

  // Render / playout delay. Only set for FRAME_PLAYOUT events.
  base::TimeDelta delay_delta;

  // Whether the frame is a key frame. Only set for video FRAME_ENCODED event.
  bool key_frame;

  // The requested target bitrate of the encoder at the time the frame is
  // encoded. Only set for video FRAME_ENCODED event.
  int target_bitrate;
};

struct PacketEvent {
  PacketEvent();
  ~PacketEvent();

  RtpTimestamp rtp_timestamp;
  uint32 frame_id;
  uint16 max_packet_id;
  uint16 packet_id;
  size_t size;

  // Time of event logged.
  base::TimeTicks timestamp;
  CastLoggingEvent type;
  EventMediaType media_type;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_
