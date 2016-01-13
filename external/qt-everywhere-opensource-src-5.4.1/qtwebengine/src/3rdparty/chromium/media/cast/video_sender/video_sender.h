// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_VIDEO_SENDER_VIDEO_SENDER_H_
#define MEDIA_CAST_VIDEO_SENDER_VIDEO_SENDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/congestion_control/congestion_control.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_timestamp_helper.h"

namespace media {

class VideoFrame;

namespace cast {

class LocalVideoEncoderCallback;
class VideoEncoder;

namespace transport {
class CastTransportSender;
}

// Not thread safe. Only called from the main cast thread.
// This class owns all objects related to sending video, objects that create RTP
// packets, congestion control, video encoder, parsing and sending of
// RTCP packets.
// Additionally it posts a bunch of delayed tasks to the main thread for various
// timeouts.
class VideoSender : public RtcpSenderFeedback,
                    public base::NonThreadSafe,
                    public base::SupportsWeakPtr<VideoSender> {
 public:
  VideoSender(scoped_refptr<CastEnvironment> cast_environment,
              const VideoSenderConfig& video_config,
              const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
              const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
              transport::CastTransportSender* const transport_sender);

  virtual ~VideoSender();

  CastInitializationStatus InitializationResult() const {
    return cast_initialization_status_;
  }

  // Note: It is not guaranteed that |video_frame| will actually be encoded and
  // sent, if VideoSender detects too many frames in flight.  Therefore, clients
  // should be careful about the rate at which this method is called.
  //
  // Note: It is invalid to call this method if InitializationResult() returns
  // anything but STATUS_VIDEO_INITIALIZED.
  void InsertRawVideoFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                           const base::TimeTicks& capture_time);

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
  // acknowledgements have been received for "too long," VideoSender will
  // speculatively re-send certain packets of an unacked frame to kick-start
  // re-transmission.  This is a last resort tactic to prevent the session from
  // getting stuck after a long outage.
  void ScheduleNextResendCheck();
  void ResendCheck();
  void ResendForKickstart();

  // Returns true if there are too many frames in flight, as defined by the
  // configured target playout delay plus simple logic.  When this is true,
  // InsertRawVideoFrame() will silenty drop frames instead of sending them to
  // the video encoder.
  bool AreTooManyFramesInFlight() const;

  // Called by the |video_encoder_| with the next EncodeFrame to send.
  void SendEncodedVideoFrame(int requested_bitrate_before_encode,
                             scoped_ptr<transport::EncodedFrame> encoded_frame);

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

  // Encodes media::VideoFrame images into EncodedFrames.  Per configuration,
  // this will point to either the internal software-based encoder or a proxy to
  // a hardware-based encoder.
  scoped_ptr<VideoEncoder> video_encoder_;

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

  // The number of frames currently being processed in |video_encoder_|.
  int frames_in_encoder_;

  // This is "null" until the first frame is sent.  Thereafter, this tracks the
  // last time any frame was sent or re-sent.
  base::TimeTicks last_send_time_;

  // The ID of the last frame sent.  Logic throughout VideoSender assumes this
  // can safely wrap-around.  This member is invalid until
  // |!last_send_time_.is_null()|.
  uint32 last_sent_frame_id_;

  // The ID of the latest (not necessarily the last) frame that has been
  // acknowledged.  Logic throughout VideoSender assumes this can safely
  // wrap-around.  This member is invalid until |!last_send_time_.is_null()|.
  uint32 latest_acked_frame_id_;

  // Counts the number of duplicate ACK that are being received.  When this
  // number reaches a threshold, the sender will take this as a sign that the
  // receiver hasn't yet received the first packet of the next frame.  In this
  // case, VideoSender will trigger a re-send of the next frame.
  int duplicate_ack_counter_;

  // When we get close to the max number of un-acked frames, we set lower
  // the bitrate drastically to ensure that we catch up. Without this we
  // risk getting stuck in a catch-up state forever.
  CongestionControl congestion_control_;

  // If this sender is ready for use, this is STATUS_VIDEO_INITIALIZED.
  CastInitializationStatus cast_initialization_status_;

  // This is a "good enough" mapping for finding the RTP timestamp associated
  // with a video frame. The key is the lowest 8 bits of frame id (which is
  // what is sent via RTCP). This map is used for logging purposes.
  RtpTimestamp frame_id_to_rtp_timestamp_[256];

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_SENDER_VIDEO_SENDER_H_
