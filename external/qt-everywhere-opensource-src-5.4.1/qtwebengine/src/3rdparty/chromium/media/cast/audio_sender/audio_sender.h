// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_AUDIO_SENDER_H_
#define MEDIA_CAST_AUDIO_SENDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_timestamp_helper.h"

namespace media {
namespace cast {

class AudioEncoder;

// Not thread safe. Only called from the main cast thread.
// This class owns all objects related to sending audio, objects that create RTP
// packets, congestion control, audio encoder, parsing and sending of
// RTCP packets.
// Additionally it posts a bunch of delayed tasks to the main thread for various
// timeouts.
class AudioSender : public RtcpSenderFeedback,
                    public base::NonThreadSafe,
                    public base::SupportsWeakPtr<AudioSender> {
 public:
  AudioSender(scoped_refptr<CastEnvironment> cast_environment,
              const AudioSenderConfig& audio_config,
              transport::CastTransportSender* const transport_sender);

  virtual ~AudioSender();

  CastInitializationStatus InitializationResult() const {
    return cast_initialization_status_;
  }

  // Note: It is not guaranteed that |audio_frame| will actually be encoded and
  // sent, if AudioSender detects too many frames in flight.  Therefore, clients
  // should be careful about the rate at which this method is called.
  //
  // Note: It is invalid to call this method if InitializationResult() returns
  // anything but STATUS_AUDIO_INITIALIZED.
  void InsertAudio(scoped_ptr<AudioBus> audio_bus,
                   const base::TimeTicks& recorded_time);

  // Only called from the main cast thread.
  void IncomingRtcpPacket(scoped_ptr<Packet> packet);

 protected:
  // Protected for testability.
  virtual void OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback)
      OVERRIDE;

 private:
  // Schedule and execute periodic sending of RTCP report.
  void ScheduleNextRtcpReport();
  void SendRtcpReport(bool schedule_future_reports);

  // Schedule and execute periodic checks for re-sending packets.  If no
  // acknowledgements have been received for "too long," AudioSender will
  // speculatively re-send certain packets of an unacked frame to kick-start
  // re-transmission.  This is a last resort tactic to prevent the session from
  // getting stuck after a long outage.
  void ScheduleNextResendCheck();
  void ResendCheck();
  void ResendForKickstart();

  // Returns true if there are too many frames in flight, as defined by the
  // configured target playout delay plus simple logic.  When this is true,
  // InsertAudio() will silenty drop frames instead of sending them to the audio
  // encoder.
  bool AreTooManyFramesInFlight() const;

  // Called by the |audio_encoder_| with the next EncodedFrame to send.
  void SendEncodedAudioFrame(scoped_ptr<transport::EncodedFrame> audio_frame);

  const scoped_refptr<CastEnvironment> cast_environment_;

  // The total amount of time between a frame's capture/recording on the sender
  // and its playback on the receiver (i.e., shown to a user).  This is fixed as
  // a value large enough to give the system sufficient time to encode,
  // transmit/retransmit, receive, decode, and render; given its run-time
  // environment (sender/receiver hardware performance, network conditions,
  // etc.).
  const base::TimeDelta target_playout_delay_;

  // Sends encoded frames over the configured transport (e.g., UDP).  In
  // Chromium, this could be a proxy that first sends the frames from a renderer
  // process to the browser process over IPC, with the browser process being
  // responsible for "packetizing" the frames and pushing packets into the
  // network layer.
  transport::CastTransportSender* const transport_sender_;

  // Maximum number of outstanding frames before the encoding and sending of
  // new frames shall halt.
  const int max_unacked_frames_;

  // Encodes AudioBuses into EncodedFrames.
  scoped_ptr<AudioEncoder> audio_encoder_;
  const int configured_encoder_bitrate_;

  // Manages sending/receiving of RTCP packets, including sender/receiver
  // reports.
  Rtcp rtcp_;

  // Records lip-sync (i.e., mapping of RTP <--> NTP timestamps), and
  // extrapolates this mapping to any other point in time.
  RtpTimestampHelper rtp_timestamp_helper_;

  // Counts how many RTCP reports are being "aggressively" sent (i.e., one per
  // frame) at the start of the session.  Once a threshold is reached, RTCP
  // reports are instead sent at the configured interval + random drift.
  int num_aggressive_rtcp_reports_sent_;

  // This is "null" until the first frame is sent.  Thereafter, this tracks the
  // last time any frame was sent or re-sent.
  base::TimeTicks last_send_time_;

  // The ID of the last frame sent.  Logic throughout AudioSender assumes this
  // can safely wrap-around.  This member is invalid until
  // |!last_send_time_.is_null()|.
  uint32 last_sent_frame_id_;

  // The ID of the latest (not necessarily the last) frame that has been
  // acknowledged.  Logic throughout AudioSender assumes this can safely
  // wrap-around.  This member is invalid until |!last_send_time_.is_null()|.
  uint32 latest_acked_frame_id_;

  // Counts the number of duplicate ACK that are being received.  When this
  // number reaches a threshold, the sender will take this as a sign that the
  // receiver hasn't yet received the first packet of the next frame.  In this
  // case, AudioSender will trigger a re-send of the next frame.
  int duplicate_ack_counter_;

  // If this sender is ready for use, this is STATUS_AUDIO_INITIALIZED.
  CastInitializationStatus cast_initialization_status_;

  // This is a "good enough" mapping for finding the RTP timestamp associated
  // with a video frame. The key is the lowest 8 bits of frame id (which is
  // what is sent via RTCP). This map is used for logging purposes.
  RtpTimestamp frame_id_to_rtp_timestamp_[256];

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<AudioSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_AUDIO_SENDER_H_
