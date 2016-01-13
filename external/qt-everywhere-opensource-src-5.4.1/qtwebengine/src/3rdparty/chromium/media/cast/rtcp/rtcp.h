// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_RTCP_H_
#define MEDIA_CAST_RTCP_RTCP_H_

#include <map>
#include <queue>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/base/clock_drift_smoother.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/cast_transport_sender.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {

class LocalRtcpReceiverFeedback;
class LocalRtcpRttFeedback;
class PacedPacketSender;
class RtcpReceiver;
class RtcpSender;

typedef std::pair<uint32, base::TimeTicks> RtcpSendTimePair;
typedef std::map<uint32, base::TimeTicks> RtcpSendTimeMap;
typedef std::queue<RtcpSendTimePair> RtcpSendTimeQueue;

class RtcpSenderFeedback {
 public:
  virtual void OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) = 0;

  virtual ~RtcpSenderFeedback() {}
};

class RtpReceiverStatistics {
 public:
  virtual void GetStatistics(uint8* fraction_lost,
                             uint32* cumulative_lost,  // 24 bits valid.
                             uint32* extended_high_sequence_number,
                             uint32* jitter) = 0;

  virtual ~RtpReceiverStatistics() {}
};

class Rtcp {
 public:
  // Rtcp accepts two transports, one to be used by Cast senders
  // (CastTransportSender) only, and the other (PacedPacketSender) should only
  // be used by the Cast receivers and test applications.
  Rtcp(scoped_refptr<CastEnvironment> cast_environment,
       RtcpSenderFeedback* sender_feedback,
       transport::CastTransportSender* const transport_sender,  // Send-side.
       transport::PacedPacketSender* paced_packet_sender,       // Receive side.
       RtpReceiverStatistics* rtp_receiver_statistics,
       RtcpMode rtcp_mode,
       const base::TimeDelta& rtcp_interval,
       uint32 local_ssrc,
       uint32 remote_ssrc,
       const std::string& c_name,
       EventMediaType event_media_type);

  virtual ~Rtcp();

  static bool IsRtcpPacket(const uint8* rtcp_buffer, size_t length);

  static uint32 GetSsrcOfSender(const uint8* rtcp_buffer, size_t length);

  base::TimeTicks TimeToSendNextRtcpReport();

  // Send a RTCP sender report.
  // |current_time| is the current time reported by a tick clock.
  // |current_time_as_rtp_timestamp| is the corresponding RTP timestamp.
  void SendRtcpFromRtpSender(base::TimeTicks current_time,
                             uint32 current_time_as_rtp_timestamp);

  // |cast_message| and |rtcp_events| is optional; if |cast_message| is
  // provided the RTCP receiver report will append a Cast message containing
  // Acks and Nacks; if |rtcp_events| is provided the RTCP receiver report
  // will append the log messages.
  void SendRtcpFromRtpReceiver(
      const RtcpCastMessage* cast_message,
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap* rtcp_events);

  void IncomingRtcpPacket(const uint8* rtcp_buffer, size_t length);

  // TODO(miu): Clean up this method and downstream code: Only VideoSender uses
  // this (for congestion control), and only the |rtt| and |avg_rtt| values, and
  // it's not clear that any of the downstream code is doing the right thing
  // with this data.
  bool Rtt(base::TimeDelta* rtt,
           base::TimeDelta* avg_rtt,
           base::TimeDelta* min_rtt,
           base::TimeDelta* max_rtt) const;

  bool is_rtt_available() const { return number_of_rtt_in_avg_ > 0; }

  // If available, returns true and sets the output arguments to the latest
  // lip-sync timestamps gleaned from the sender reports.  While the sender
  // provides reference NTP times relative to its own wall clock, the
  // |reference_time| returned here has been translated to the local
  // CastEnvironment clock.
  bool GetLatestLipSyncTimes(uint32* rtp_timestamp,
                             base::TimeTicks* reference_time) const;

  // Set the history size to record Cast receiver events. The event history is
  // used to remove duplicates. The history will store at most |size| events.
  void SetCastReceiverEventHistorySize(size_t size);

  // Update the target delay. Will be added to every report sent back to the
  // sender.
  // TODO(miu): Remove this deprecated functionality. The sender ignores this.
  void SetTargetDelay(base::TimeDelta target_delay);

  void OnReceivedReceiverLog(const RtcpReceiverLogMessage& receiver_log);

 protected:
  void OnReceivedNtp(uint32 ntp_seconds, uint32 ntp_fraction);
  void OnReceivedLipSyncInfo(uint32 rtp_timestamp,
                             uint32 ntp_seconds,
                             uint32 ntp_fraction);

 private:
  friend class LocalRtcpRttFeedback;
  friend class LocalRtcpReceiverFeedback;

  void OnReceivedDelaySinceLastReport(uint32 receivers_ssrc,
                                      uint32 last_report,
                                      uint32 delay_since_last_report);

  void OnReceivedSendReportRequest();

  void UpdateRtt(const base::TimeDelta& sender_delay,
                 const base::TimeDelta& receiver_delay);

  void UpdateNextTimeToSendRtcp();

  void SaveLastSentNtpTime(const base::TimeTicks& now,
                           uint32 last_ntp_seconds,
                           uint32 last_ntp_fraction);

  scoped_refptr<CastEnvironment> cast_environment_;
  transport::CastTransportSender* const transport_sender_;
  const base::TimeDelta rtcp_interval_;
  const RtcpMode rtcp_mode_;
  const uint32 local_ssrc_;
  const uint32 remote_ssrc_;
  const std::string c_name_;
  const EventMediaType event_media_type_;

  // Not owned by this class.
  RtpReceiverStatistics* const rtp_receiver_statistics_;

  scoped_ptr<LocalRtcpRttFeedback> rtt_feedback_;
  scoped_ptr<LocalRtcpReceiverFeedback> receiver_feedback_;
  scoped_ptr<RtcpSender> rtcp_sender_;
  scoped_ptr<RtcpReceiver> rtcp_receiver_;

  base::TimeTicks next_time_to_send_rtcp_;
  RtcpSendTimeMap last_reports_sent_map_;
  RtcpSendTimeQueue last_reports_sent_queue_;

  // The truncated (i.e., 64-->32-bit) NTP timestamp provided in the last report
  // from the remote peer, along with the local time at which the report was
  // received.  These values are used for ping-pong'ing NTP timestamps between
  // the peers so that they can estimate the network's round-trip time.
  uint32 last_report_truncated_ntp_;
  base::TimeTicks time_last_report_received_;

  // Maintains a smoothed offset between the local clock and the remote clock.
  // Calling this member's Current() method is only valid if
  // |time_last_report_received_| is not "null."
  ClockDriftSmoother local_clock_ahead_by_;

  // Latest "lip sync" info from the sender.  The sender provides the RTP
  // timestamp of some frame of its choosing and also a corresponding reference
  // NTP timestamp sampled from a clock common to all media streams.  It is
  // expected that the sender will update this data regularly and in a timely
  // manner (e.g., about once per second).
  uint32 lip_sync_rtp_timestamp_;
  uint64 lip_sync_ntp_timestamp_;

  base::TimeDelta rtt_;
  base::TimeDelta min_rtt_;
  base::TimeDelta max_rtt_;
  int number_of_rtt_in_avg_;
  double avg_rtt_ms_;
  uint16 target_delay_ms_;

  DISALLOW_COPY_AND_ASSIGN(Rtcp);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTCP_RTCP_H_
