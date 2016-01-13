// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"

#include <utility>

namespace media {
namespace cast {

ReceiverRtcpEventSubscriber::ReceiverRtcpEventSubscriber(
    const size_t max_size_to_retain, EventMediaType type)
    : max_size_to_retain_(max_size_to_retain), type_(type) {
  DCHECK(max_size_to_retain_ > 0u);
  DCHECK(type_ == AUDIO_EVENT || type_ == VIDEO_EVENT);
}

ReceiverRtcpEventSubscriber::~ReceiverRtcpEventSubscriber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ReceiverRtcpEventSubscriber::OnReceiveFrameEvent(
    const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ShouldProcessEvent(frame_event.type, frame_event.media_type)) {
    RtcpEvent rtcp_event;
    switch (frame_event.type) {
      case FRAME_PLAYOUT:
        rtcp_event.delay_delta = frame_event.delay_delta;
      case FRAME_ACK_SENT:
      case FRAME_DECODED:
        rtcp_event.type = frame_event.type;
        rtcp_event.timestamp = frame_event.timestamp;
        rtcp_events_.insert(
            std::make_pair(frame_event.rtp_timestamp, rtcp_event));
        break;
      default:
        break;
    }
  }

  TruncateMapIfNeeded();

  DCHECK(rtcp_events_.size() <= max_size_to_retain_);
}

void ReceiverRtcpEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ShouldProcessEvent(packet_event.type, packet_event.media_type)) {
    RtcpEvent rtcp_event;
    if (packet_event.type == PACKET_RECEIVED) {
      rtcp_event.type = packet_event.type;
      rtcp_event.timestamp = packet_event.timestamp;
      rtcp_event.packet_id = packet_event.packet_id;
      rtcp_events_.insert(
          std::make_pair(packet_event.rtp_timestamp, rtcp_event));
    }
  }

  TruncateMapIfNeeded();

  DCHECK(rtcp_events_.size() <= max_size_to_retain_);
}

void ReceiverRtcpEventSubscriber::GetRtcpEventsAndReset(
    RtcpEventMultiMap* rtcp_events) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(rtcp_events);
  rtcp_events->swap(rtcp_events_);
  rtcp_events_.clear();
}

void ReceiverRtcpEventSubscriber::TruncateMapIfNeeded() {
  // If map size has exceeded |max_size_to_retain_|, remove entry with
  // the smallest RTP timestamp.
  if (rtcp_events_.size() > max_size_to_retain_) {
    DVLOG(3) << "RTCP event map exceeded size limit; "
             << "removing oldest entry";
    // This is fine since we only insert elements one at a time.
    rtcp_events_.erase(rtcp_events_.begin());
  }
}

bool ReceiverRtcpEventSubscriber::ShouldProcessEvent(
    CastLoggingEvent event_type, EventMediaType event_media_type) {
  return type_ == event_media_type &&
      (event_type == FRAME_ACK_SENT || event_type == FRAME_DECODED ||
       event_type == FRAME_PLAYOUT || event_type == PACKET_RECEIVED);
}

}  // namespace cast
}  // namespace media
