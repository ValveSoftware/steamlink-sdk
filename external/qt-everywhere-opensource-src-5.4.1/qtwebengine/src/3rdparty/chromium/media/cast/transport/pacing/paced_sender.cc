// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/pacing/paced_sender.h"

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace media {
namespace cast {
namespace transport {

namespace {

static const int64 kPacingIntervalMs = 10;
// Each frame will be split into no more than kPacingMaxBurstsPerFrame
// bursts of packets.
static const size_t kPacingMaxBurstsPerFrame = 3;
static const size_t kTargetBurstSize = 10;
static const size_t kMaxBurstSize = 20;
static const size_t kMaxDedupeWindowMs = 500;

}  // namespace

// static
PacketKey PacedPacketSender::MakePacketKey(const base::TimeTicks& ticks,
                                           uint32 ssrc,
                                           uint16 packet_id) {
  return std::make_pair(ticks, std::make_pair(ssrc, packet_id));
}

PacedSender::PacedSender(
    base::TickClock* clock,
    LoggingImpl* logging,
    PacketSender* transport,
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner)
    : clock_(clock),
      logging_(logging),
      transport_(transport),
      transport_task_runner_(transport_task_runner),
      audio_ssrc_(0),
      video_ssrc_(0),
      max_burst_size_(kTargetBurstSize),
      next_max_burst_size_(kTargetBurstSize),
      next_next_max_burst_size_(kTargetBurstSize),
      current_burst_size_(0),
      state_(State_Unblocked),
      weak_factory_(this) {
}

PacedSender::~PacedSender() {}

void PacedSender::RegisterAudioSsrc(uint32 audio_ssrc) {
  audio_ssrc_ = audio_ssrc;
}

void PacedSender::RegisterVideoSsrc(uint32 video_ssrc) {
  video_ssrc_ = video_ssrc;
}

bool PacedSender::SendPackets(const SendPacketVector& packets) {
  if (packets.empty()) {
    return true;
  }
  for (size_t i = 0; i < packets.size(); i++) {
    packet_list_[packets[i].first] =
        make_pair(PacketType_Normal, packets[i].second);
  }
  if (state_ == State_Unblocked) {
    SendStoredPackets();
  }
  return true;
}

bool PacedSender::ResendPackets(const SendPacketVector& packets,
                                base::TimeDelta dedupe_window) {
  if (packets.empty()) {
    return true;
  }
  base::TimeTicks now = clock_->NowTicks();
  for (size_t i = 0; i < packets.size(); i++) {
    std::map<PacketKey, base::TimeTicks>::const_iterator j =
        sent_time_.find(packets[i].first);

    if (j != sent_time_.end() && now - j->second < dedupe_window) {
      LogPacketEvent(packets[i].second->data, PACKET_RTX_REJECTED);
      continue;
    }

    packet_list_[packets[i].first] =
        make_pair(PacketType_Resend, packets[i].second);
  }
  if (state_ == State_Unblocked) {
    SendStoredPackets();
  }
  return true;
}

bool PacedSender::SendRtcpPacket(uint32 ssrc, PacketRef packet) {
  if (state_ == State_TransportBlocked) {
    packet_list_[PacedPacketSender::MakePacketKey(base::TimeTicks(), ssrc, 0)] =
        make_pair(PacketType_RTCP, packet);
  } else {
    // We pass the RTCP packets straight through.
    if (!transport_->SendPacket(
            packet,
            base::Bind(&PacedSender::SendStoredPackets,
                       weak_factory_.GetWeakPtr()))) {
      state_ = State_TransportBlocked;
    }

  }
  return true;
}

void PacedSender::CancelSendingPacket(const PacketKey& packet_key) {
  packet_list_.erase(packet_key);
}

PacketRef PacedSender::GetNextPacket(PacketType* packet_type,
                                     PacketKey* packet_key) {
  std::map<PacketKey, std::pair<PacketType, PacketRef> >::iterator i;
  i = packet_list_.begin();
  DCHECK(i != packet_list_.end());
  *packet_type = i->second.first;
  *packet_key = i->first;
  PacketRef ret = i->second.second;
  packet_list_.erase(i);
  return ret;
}

bool PacedSender::empty() const {
  return packet_list_.empty();
}

size_t PacedSender::size() const {
  return packet_list_.size();
}

// This function can be called from three places:
// 1. User called one of the Send* functions and we were in an unblocked state.
// 2. state_ == State_TransportBlocked and the transport is calling us to
//    let us know that it's ok to send again.
// 3. state_ == State_BurstFull and there are still packets to send. In this
//    case we called PostDelayedTask on this function to start a new burst.
void PacedSender::SendStoredPackets() {
  State previous_state = state_;
  state_ = State_Unblocked;
  if (empty()) {
    return;
  }

  base::TimeTicks now = clock_->NowTicks();
  // I don't actually trust that PostDelayTask(x - now) will mean that
  // now >= x when the call happens, so check if the previous state was
  // State_BurstFull too.
  if (now >= burst_end_ || previous_state == State_BurstFull) {
    // Start a new burst.
    current_burst_size_ = 0;
    burst_end_ = now + base::TimeDelta::FromMilliseconds(kPacingIntervalMs);

    // The goal here is to try to send out the queued packets over the next
    // three bursts, while trying to keep the burst size below 10 if possible.
    // We have some evidence that sending more than 12 packets in a row doesn't
    // work very well, but we don't actually know why yet. Sending out packets
    // sooner is better than sending out packets later as that gives us more
    // time to re-send them if needed. So if we have less than 30 packets, just
    // send 10 at a time. If we have less than 60 packets, send n / 3 at a time.
    // if we have more than 60, we send 20 at a time. 20 packets is ~24Mbit/s
    // which is more bandwidth than the cast library should need, and sending
    // out more data per second is unlikely to be helpful.
    size_t max_burst_size = std::min(
        kMaxBurstSize,
        std::max(kTargetBurstSize, size() / kPacingMaxBurstsPerFrame));

    // If the queue is long, issue a warning. Try to limit the number of
    // warnings issued by only issuing the warning when the burst size
    // grows. Otherwise we might get 100 warnings per second.
    if (max_burst_size > next_next_max_burst_size_ && size() > 100) {
      LOG(WARNING) << "Packet queue is very long:" << size();
    }

    max_burst_size_ = std::max(next_max_burst_size_, max_burst_size);
    next_max_burst_size_ = std::max(next_next_max_burst_size_, max_burst_size);
    next_next_max_burst_size_ = max_burst_size;
  }

  base::Closure cb = base::Bind(&PacedSender::SendStoredPackets,
                                weak_factory_.GetWeakPtr());
  while (!empty()) {
    if (current_burst_size_ >= max_burst_size_) {
      transport_task_runner_->PostDelayedTask(FROM_HERE,
                                              cb,
                                              burst_end_ - now);
      state_ = State_BurstFull;
      return;
    }
    PacketType packet_type;
    PacketKey packet_key;
    PacketRef packet = GetNextPacket(&packet_type, &packet_key);
    sent_time_[packet_key] = now;
    sent_time_buffer_[packet_key] = now;

    switch (packet_type) {
      case PacketType_Resend:
        LogPacketEvent(packet->data, PACKET_RETRANSMITTED);
        break;
      case PacketType_Normal:
        LogPacketEvent(packet->data, PACKET_SENT_TO_NETWORK);
        break;
      case PacketType_RTCP:
        break;
    }
    if (!transport_->SendPacket(packet, cb)) {
      state_ = State_TransportBlocked;
      return;
    }
    current_burst_size_++;
  }
  // Keep ~0.5 seconds of data (1000 packets)
  if (sent_time_buffer_.size() >=
      kMaxBurstSize * kMaxDedupeWindowMs / kPacingIntervalMs) {
    sent_time_.swap(sent_time_buffer_);
    sent_time_buffer_.clear();
  }
  DCHECK_LE(sent_time_buffer_.size(),
            kMaxBurstSize * kMaxDedupeWindowMs / kPacingIntervalMs);
  DCHECK_LE(sent_time_.size(),
            2 * kMaxBurstSize * kMaxDedupeWindowMs / kPacingIntervalMs);
  state_ = State_Unblocked;
}

void PacedSender::LogPacketEvent(const Packet& packet, CastLoggingEvent event) {
  // Get SSRC from packet and compare with the audio_ssrc / video_ssrc to see
  // if the packet is audio or video.
  DCHECK_GE(packet.size(), 12u);
  base::BigEndianReader reader(reinterpret_cast<const char*>(&packet[8]), 4);
  uint32 ssrc;
  bool success = reader.ReadU32(&ssrc);
  DCHECK(success);
  bool is_audio;
  if (ssrc == audio_ssrc_) {
    is_audio = true;
  } else if (ssrc == video_ssrc_) {
    is_audio = false;
  } else {
    DVLOG(3) << "Got unknown ssrc " << ssrc << " when logging packet event";
    return;
  }

  EventMediaType media_type = is_audio ? AUDIO_EVENT : VIDEO_EVENT;
  logging_->InsertSinglePacketEvent(clock_->NowTicks(), event, media_type,
      packet);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
