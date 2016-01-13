// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/stats_event_subscriber.h"

#include "base/logging.h"
#include "base/values.h"

#define STAT_ENUM_TO_STRING(enum) \
  case enum:                      \
    return #enum

namespace media {
namespace cast {

namespace {

using media::cast::CastLoggingEvent;
using media::cast::EventMediaType;

const size_t kMaxFrameEventTimeMapSize = 100;
const size_t kMaxPacketEventTimeMapSize = 1000;

bool IsReceiverEvent(CastLoggingEvent event) {
  return event == FRAME_DECODED
      || event == FRAME_PLAYOUT
      || event == FRAME_ACK_SENT
      || event == PACKET_RECEIVED;
}

}  // namespace

StatsEventSubscriber::StatsEventSubscriber(
    EventMediaType event_media_type,
    base::TickClock* clock,
    ReceiverTimeOffsetEstimator* offset_estimator)
    : event_media_type_(event_media_type),
      clock_(clock),
      offset_estimator_(offset_estimator),
      network_latency_datapoints_(0),
      e2e_latency_datapoints_(0) {
  DCHECK(event_media_type == AUDIO_EVENT || event_media_type == VIDEO_EVENT);
  base::TimeTicks now = clock_->NowTicks();
  start_time_ = now;
  last_response_received_time_ = base::TimeTicks();
}

StatsEventSubscriber::~StatsEventSubscriber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void StatsEventSubscriber::OnReceiveFrameEvent(const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CastLoggingEvent type = frame_event.type;
  if (frame_event.media_type != event_media_type_)
    return;

  FrameStatsMap::iterator it = frame_stats_.find(type);
  if (it == frame_stats_.end()) {
    FrameLogStats stats;
    stats.event_counter = 1;
    stats.sum_size = frame_event.size;
    stats.sum_delay = frame_event.delay_delta;
    frame_stats_.insert(std::make_pair(type, stats));
  } else {
    ++(it->second.event_counter);
    it->second.sum_size += frame_event.size;
    it->second.sum_delay += frame_event.delay_delta;
  }

  if (type == FRAME_CAPTURE_BEGIN) {
    RecordFrameCapturedTime(frame_event);
  } else if (type == FRAME_PLAYOUT) {
    RecordE2ELatency(frame_event);
  }

  if (IsReceiverEvent(type))
    UpdateLastResponseTime(frame_event.timestamp);
}

void StatsEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CastLoggingEvent type = packet_event.type;
  if (packet_event.media_type != event_media_type_)
    return;

  PacketStatsMap::iterator it = packet_stats_.find(type);
  if (it == packet_stats_.end()) {
    PacketLogStats stats;
    stats.event_counter = 1;
    stats.sum_size = packet_event.size;
    packet_stats_.insert(std::make_pair(type, stats));
  } else {
    ++(it->second.event_counter);
    it->second.sum_size += packet_event.size;
  }

  if (type == PACKET_SENT_TO_NETWORK ||
      type == PACKET_RECEIVED) {
    RecordNetworkLatency(packet_event);
  } else if (type == PACKET_RETRANSMITTED) {
    // We only measure network latency using packets that doesn't have to be
    // retransmitted as there is precisely one sent-receive timestamp pairs.
    ErasePacketSentTime(packet_event);
  }

  if (IsReceiverEvent(type))
    UpdateLastResponseTime(packet_event.timestamp);
}

scoped_ptr<base::DictionaryValue> StatsEventSubscriber::GetStats() const {
  StatsMap stats_map;
  GetStatsInternal(&stats_map);
  scoped_ptr<base::DictionaryValue> ret(new base::DictionaryValue);

  scoped_ptr<base::DictionaryValue> stats(new base::DictionaryValue);
  for (StatsMap::const_iterator it = stats_map.begin(); it != stats_map.end();
       ++it) {
    stats->SetDouble(CastStatToString(it->first), it->second);
  }

  ret->Set(event_media_type_ == AUDIO_EVENT ? "audio" : "video",
           stats.release());

  return ret.Pass();
}

void StatsEventSubscriber::Reset() {
  DCHECK(thread_checker_.CalledOnValidThread());

  frame_stats_.clear();
  packet_stats_.clear();
  total_network_latency_ = base::TimeDelta();
  network_latency_datapoints_ = 0;
  total_e2e_latency_ = base::TimeDelta();
  e2e_latency_datapoints_ = 0;
  frame_captured_times_.clear();
  packet_sent_times_.clear();
  start_time_ = clock_->NowTicks();
  last_response_received_time_ = base::TimeTicks();
}

// static
const char* StatsEventSubscriber::CastStatToString(CastStat stat) {
  switch (stat) {
    STAT_ENUM_TO_STRING(CAPTURE_FPS);
    STAT_ENUM_TO_STRING(ENCODE_FPS);
    STAT_ENUM_TO_STRING(DECODE_FPS);
    STAT_ENUM_TO_STRING(AVG_ENCODE_TIME_MS);
    STAT_ENUM_TO_STRING(AVG_PLAYOUT_DELAY_MS);
    STAT_ENUM_TO_STRING(AVG_NETWORK_LATENCY_MS);
    STAT_ENUM_TO_STRING(AVG_E2E_LATENCY_MS);
    STAT_ENUM_TO_STRING(ENCODE_KBPS);
    STAT_ENUM_TO_STRING(TRANSMISSION_KBPS);
    STAT_ENUM_TO_STRING(RETRANSMISSION_KBPS);
    STAT_ENUM_TO_STRING(PACKET_LOSS_FRACTION);
    STAT_ENUM_TO_STRING(MS_SINCE_LAST_RECEIVER_RESPONSE);
  }
  NOTREACHED();
  return "";
}

void StatsEventSubscriber::GetStatsInternal(StatsMap* stats_map) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  stats_map->clear();

  base::TimeTicks end_time = clock_->NowTicks();

  PopulateFpsStat(
      end_time, FRAME_CAPTURE_BEGIN, CAPTURE_FPS, stats_map);
  PopulateFpsStat(
      end_time, FRAME_ENCODED, ENCODE_FPS, stats_map);
  PopulateFpsStat(
      end_time, FRAME_DECODED, DECODE_FPS, stats_map);
  PopulatePlayoutDelayStat(stats_map);
  PopulateFrameBitrateStat(end_time, stats_map);
  PopulatePacketBitrateStat(end_time,
                            PACKET_SENT_TO_NETWORK,
                            TRANSMISSION_KBPS,
                            stats_map);
  PopulatePacketBitrateStat(end_time,
                            PACKET_RETRANSMITTED,
                            RETRANSMISSION_KBPS,
                            stats_map);
  PopulatePacketLossPercentageStat(stats_map);

  if (network_latency_datapoints_ > 0) {
    double avg_network_latency_ms =
        total_network_latency_.InMillisecondsF() /
        network_latency_datapoints_;
    stats_map->insert(
        std::make_pair(AVG_NETWORK_LATENCY_MS, avg_network_latency_ms));
  }

  if (e2e_latency_datapoints_ > 0) {
    double avg_e2e_latency_ms =
        total_e2e_latency_.InMillisecondsF() / e2e_latency_datapoints_;
    stats_map->insert(std::make_pair(AVG_E2E_LATENCY_MS, avg_e2e_latency_ms));
  }

  if (!last_response_received_time_.is_null()) {
    stats_map->insert(
        std::make_pair(MS_SINCE_LAST_RECEIVER_RESPONSE,
        (end_time - last_response_received_time_).InMillisecondsF()));
  }
}

bool StatsEventSubscriber::GetReceiverOffset(base::TimeDelta* offset) {
  base::TimeDelta receiver_offset_lower_bound;
  base::TimeDelta receiver_offset_upper_bound;
  if (!offset_estimator_->GetReceiverOffsetBounds(
          &receiver_offset_lower_bound, &receiver_offset_upper_bound)) {
    return false;
  }

  *offset = (receiver_offset_lower_bound + receiver_offset_upper_bound) / 2;
  return true;
}

void StatsEventSubscriber::RecordFrameCapturedTime(
    const FrameEvent& frame_event) {
  frame_captured_times_.insert(
      std::make_pair(frame_event.rtp_timestamp, frame_event.timestamp));
  if (frame_captured_times_.size() > kMaxFrameEventTimeMapSize)
    frame_captured_times_.erase(frame_captured_times_.begin());
}

void StatsEventSubscriber::RecordE2ELatency(const FrameEvent& frame_event) {
  base::TimeDelta receiver_offset;
  if (!GetReceiverOffset(&receiver_offset))
    return;

  FrameEventTimeMap::iterator it =
      frame_captured_times_.find(frame_event.rtp_timestamp);
  if (it == frame_captured_times_.end())
    return;

  // Playout time is event time + playout delay.
  base::TimeTicks playout_time =
      frame_event.timestamp + frame_event.delay_delta - receiver_offset;
  total_e2e_latency_ += playout_time - it->second;
  e2e_latency_datapoints_++;
}

void StatsEventSubscriber::UpdateLastResponseTime(
    base::TimeTicks receiver_time) {
  base::TimeDelta receiver_offset;
  if (!GetReceiverOffset(&receiver_offset))
    return;
  base::TimeTicks sender_time = receiver_time - receiver_offset;
  last_response_received_time_ = sender_time;
}

void StatsEventSubscriber::ErasePacketSentTime(
    const PacketEvent& packet_event) {
  std::pair<RtpTimestamp, uint16> key(
      std::make_pair(packet_event.rtp_timestamp, packet_event.packet_id));
  packet_sent_times_.erase(key);
}

void StatsEventSubscriber::RecordNetworkLatency(
    const PacketEvent& packet_event) {
  base::TimeDelta receiver_offset;
  if (!GetReceiverOffset(&receiver_offset))
    return;

  std::pair<RtpTimestamp, uint16> key(
      std::make_pair(packet_event.rtp_timestamp, packet_event.packet_id));
  PacketEventTimeMap::iterator it = packet_sent_times_.find(key);
  if (it == packet_sent_times_.end()) {
    std::pair<RtpTimestamp, uint16> key(
        std::make_pair(packet_event.rtp_timestamp, packet_event.packet_id));
    std::pair<base::TimeTicks, CastLoggingEvent> value =
        std::make_pair(packet_event.timestamp, packet_event.type);
    packet_sent_times_.insert(std::make_pair(key, value));
    if (packet_sent_times_.size() > kMaxPacketEventTimeMapSize)
      packet_sent_times_.erase(packet_sent_times_.begin());
  } else {
    std::pair<base::TimeTicks, CastLoggingEvent> value = it->second;
    CastLoggingEvent recorded_type = value.second;
    bool match = false;
    base::TimeTicks packet_sent_time;
    base::TimeTicks packet_received_time;
    if (recorded_type == PACKET_SENT_TO_NETWORK &&
        packet_event.type == PACKET_RECEIVED) {
      packet_sent_time = value.first;
      packet_received_time = packet_event.timestamp;
      match = true;
    } else if (recorded_type == PACKET_RECEIVED &&
        packet_event.type == PACKET_SENT_TO_NETWORK) {
      packet_sent_time = packet_event.timestamp;
      packet_received_time = value.first;
      match = true;
    }
    if (match) {
      // Subtract by offset.
      packet_received_time -= receiver_offset;

      total_network_latency_ += packet_received_time - packet_sent_time;
      network_latency_datapoints_++;
      packet_sent_times_.erase(it);
    }
  }
}

void StatsEventSubscriber::PopulateFpsStat(base::TimeTicks end_time,
                                           CastLoggingEvent event,
                                           CastStat stat,
                                           StatsMap* stats_map) const {
  FrameStatsMap::const_iterator it = frame_stats_.find(event);
  if (it != frame_stats_.end()) {
    double fps = 0.0;
    base::TimeDelta duration = (end_time - start_time_);
    int count = it->second.event_counter;
    if (duration > base::TimeDelta())
      fps = count / duration.InSecondsF();
    stats_map->insert(std::make_pair(stat, fps));
  }
}

void StatsEventSubscriber::PopulatePlayoutDelayStat(StatsMap* stats_map) const {
  FrameStatsMap::const_iterator it = frame_stats_.find(FRAME_PLAYOUT);
  if (it != frame_stats_.end()) {
    double avg_delay_ms = 0.0;
    base::TimeDelta sum_delay = it->second.sum_delay;
    int count = it->second.event_counter;
    if (count != 0)
      avg_delay_ms = sum_delay.InMillisecondsF() / count;
    stats_map->insert(std::make_pair(AVG_PLAYOUT_DELAY_MS, avg_delay_ms));
  }
}

void StatsEventSubscriber::PopulateFrameBitrateStat(base::TimeTicks end_time,
                                                    StatsMap* stats_map) const {
  FrameStatsMap::const_iterator it = frame_stats_.find(FRAME_ENCODED);
  if (it != frame_stats_.end()) {
    double kbps = 0.0;
    base::TimeDelta duration = end_time - start_time_;
    if (duration > base::TimeDelta()) {
      kbps = it->second.sum_size / duration.InMillisecondsF() * 8;
    }

    stats_map->insert(std::make_pair(ENCODE_KBPS, kbps));
  }
}

void StatsEventSubscriber::PopulatePacketBitrateStat(
    base::TimeTicks end_time,
    CastLoggingEvent event,
    CastStat stat,
    StatsMap* stats_map) const {
  PacketStatsMap::const_iterator it = packet_stats_.find(event);
  if (it != packet_stats_.end()) {
    double kbps = 0;
    base::TimeDelta duration = end_time - start_time_;
    if (duration > base::TimeDelta()) {
      kbps = it->second.sum_size / duration.InMillisecondsF() * 8;
    }

    stats_map->insert(std::make_pair(stat, kbps));
  }
}

void StatsEventSubscriber::PopulatePacketLossPercentageStat(
    StatsMap* stats_map) const {
  // We assume that retransmission means that the packet's previous
  // (re)transmission was lost.
  // This means the percentage of packet loss is
  // (# of retransmit events) / (# of transmit + retransmit events).
  PacketStatsMap::const_iterator sent_it =
      packet_stats_.find(PACKET_SENT_TO_NETWORK);
  if (sent_it == packet_stats_.end())
    return;
  PacketStatsMap::const_iterator retransmitted_it =
      packet_stats_.find(PACKET_RETRANSMITTED);
  int sent_count = sent_it->second.event_counter;
  int retransmitted_count = 0;
  if (retransmitted_it != packet_stats_.end())
    retransmitted_count = retransmitted_it->second.event_counter;
  double packet_loss_fraction = static_cast<double>(retransmitted_count) /
                                (sent_count + retransmitted_count);
  stats_map->insert(
      std::make_pair(PACKET_LOSS_FRACTION, packet_loss_fraction));
}

StatsEventSubscriber::FrameLogStats::FrameLogStats()
    : event_counter(0), sum_size(0) {}
StatsEventSubscriber::FrameLogStats::~FrameLogStats() {}

StatsEventSubscriber::PacketLogStats::PacketLogStats()
    : event_counter(0), sum_size(0) {}
StatsEventSubscriber::PacketLogStats::~PacketLogStats() {}

}  // namespace cast
}  // namespace media
