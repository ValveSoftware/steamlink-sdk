// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_STATS_EVENT_SUBSCRIBER_H_
#define MEDIA_CAST_LOGGING_STATS_EVENT_SUBSCRIBER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/raw_event_subscriber.h"
#include "media/cast/logging/receiver_time_offset_estimator.h"

namespace base {
class DictionaryValue;
}

namespace media {
namespace cast {

class StatsEventSubscriberTest;

// A RawEventSubscriber implementation that subscribes to events,
// and aggregates them into stats.
class StatsEventSubscriber : public RawEventSubscriber {
 public:
  StatsEventSubscriber(EventMediaType event_media_type,
                       base::TickClock* clock,
                       ReceiverTimeOffsetEstimator* offset_estimator);

  virtual ~StatsEventSubscriber();

  // RawReventSubscriber implementations.
  virtual void OnReceiveFrameEvent(const FrameEvent& frame_event) OVERRIDE;
  virtual void OnReceivePacketEvent(const PacketEvent& packet_event) OVERRIDE;

  // Returns stats as a DictionaryValue. The dictionary contains one entry -
  // "audio" or "video" pointing to an inner dictionary.
  // The inner dictionary consists of string - double entries, where the string
  // describes the name of the stat, and the double describes
  // the value of the stat. See CastStat and StatsMap below.
  scoped_ptr<base::DictionaryValue> GetStats() const;

  // Resets stats in this object.
  void Reset();

 private:
  friend class StatsEventSubscriberTest;
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, EmptyStats);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, Capture);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, Encode);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, Decode);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, PlayoutDelay);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, E2ELatency);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, Packets);

  // Generic statistics given the raw data. More specific data (e.g. frame rate
  // and bit rate) can be computed given the basic metrics.
  // Some of the metrics will only be set when applicable, e.g. delay and size.
  struct FrameLogStats {
    FrameLogStats();
    ~FrameLogStats();
    int event_counter;
    size_t sum_size;
    base::TimeDelta sum_delay;
  };

  struct PacketLogStats {
    PacketLogStats();
    ~PacketLogStats();
    int event_counter;
    size_t sum_size;
  };

  enum CastStat {
    // Capture frame rate.
    CAPTURE_FPS,
    // Encode frame rate.
    ENCODE_FPS,
    // Decode frame rate.
    DECODE_FPS,
    // Average encode duration in milliseconds.
    // TODO(imcheng): This stat is not populated yet because we do not have
    // the time when encode started. Record it in FRAME_ENCODED event.
    AVG_ENCODE_TIME_MS,
    // Average playout delay in milliseconds, with target delay already
    // accounted for. Ideally, every frame should have a playout delay of 0.
    AVG_PLAYOUT_DELAY_MS,
    // Duration from when a packet is transmitted to when it is received.
    // This measures latency from sender to receiver.
    AVG_NETWORK_LATENCY_MS,
    // Duration from when a frame is captured to when it should be played out.
    AVG_E2E_LATENCY_MS,
    // Encode bitrate in kbps.
    ENCODE_KBPS,
    // Packet transmission bitrate in kbps.
    TRANSMISSION_KBPS,
    // Packet retransmission bitrate in kbps.
    RETRANSMISSION_KBPS,
    // Fraction of packet loss.
    PACKET_LOSS_FRACTION,
    // Duration in milliseconds since last receiver response.
    MS_SINCE_LAST_RECEIVER_RESPONSE
  };

  typedef std::map<CastStat, double> StatsMap;
  typedef std::map<RtpTimestamp, base::TimeTicks> FrameEventTimeMap;
  typedef std::map<
      std::pair<RtpTimestamp, uint16>,
      std::pair<base::TimeTicks, CastLoggingEvent> >
      PacketEventTimeMap;
  typedef std::map<CastLoggingEvent, FrameLogStats> FrameStatsMap;
  typedef std::map<CastLoggingEvent, PacketLogStats> PacketStatsMap;

  static const char* CastStatToString(CastStat stat);

  // Assigns |stats_map| with stats data. Used for testing.
  void GetStatsInternal(StatsMap* stats_map) const;

  bool GetReceiverOffset(base::TimeDelta* offset);
  void RecordFrameCapturedTime(const FrameEvent& frame_event);
  void RecordE2ELatency(const FrameEvent& frame_event);
  void RecordPacketSentTime(const PacketEvent& packet_event);
  void ErasePacketSentTime(const PacketEvent& packet_event);
  void RecordNetworkLatency(const PacketEvent& packet_event);
  void UpdateLastResponseTime(base::TimeTicks receiver_time);

  void PopulateFpsStat(base::TimeTicks now,
                       CastLoggingEvent event,
                       CastStat stat,
                       StatsMap* stats_map) const;
  void PopulatePlayoutDelayStat(StatsMap* stats_map) const;
  void PopulateFrameBitrateStat(base::TimeTicks now, StatsMap* stats_map) const;
  void PopulatePacketBitrateStat(base::TimeTicks now,
                                 CastLoggingEvent event,
                                 CastStat stat,
                                 StatsMap* stats_map) const;
  void PopulatePacketLossPercentageStat(StatsMap* stats_map) const;

  const EventMediaType event_media_type_;

  // Not owned by this class.
  base::TickClock* const clock_;

  // Not owned by this class.
  ReceiverTimeOffsetEstimator* const offset_estimator_;

  FrameStatsMap frame_stats_;
  PacketStatsMap packet_stats_;

  base::TimeDelta total_network_latency_;
  int network_latency_datapoints_;
  base::TimeDelta total_e2e_latency_;
  int e2e_latency_datapoints_;

  base::TimeTicks last_response_received_time_;

  // Fixed size map to record when recent frames were captured.
  FrameEventTimeMap frame_captured_times_;

  // Fixed size map to record when recent packets were sent.
  PacketEventTimeMap packet_sent_times_;

  // Sender time assigned on creation and |Reset()|.
  base::TimeTicks start_time_;

  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(StatsEventSubscriber);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_STATS_EVENT_SUBSCRIBER_H_
