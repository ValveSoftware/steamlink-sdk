// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/audio_sender/audio_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/audio_sender/audio_encoder.h"
#include "media/cast/cast_defines.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/transport/cast_transport_config.h"

namespace media {
namespace cast {
namespace {

const int kNumAggressiveReportsSentAtStart = 100;
const int kMinSchedulingDelayMs = 1;

// TODO(miu): This should be specified in AudioSenderConfig, but currently it is
// fixed to 100 FPS (i.e., 10 ms per frame), and AudioEncoder assumes this as
// well.
const int kAudioFrameRate = 100;

// Helper function to compute the maximum unacked audio frames that is sent.
int GetMaxUnackedFrames(base::TimeDelta target_delay) {
  // As long as it doesn't go over |kMaxUnackedFrames|, it is okay to send more
  // audio data than the target delay would suggest. Audio packets are tiny and
  // receiver has the ability to drop any one of the packets.
  // We send up to three times of the target delay of audio frames.
  int frames =
      1 + 3 * target_delay * kAudioFrameRate / base::TimeDelta::FromSeconds(1);
  return std::min(kMaxUnackedFrames, frames);
}
}  // namespace

AudioSender::AudioSender(scoped_refptr<CastEnvironment> cast_environment,
                         const AudioSenderConfig& audio_config,
                         transport::CastTransportSender* const transport_sender)
    : cast_environment_(cast_environment),
      target_playout_delay_(base::TimeDelta::FromMilliseconds(
          audio_config.rtp_config.max_delay_ms)),
      transport_sender_(transport_sender),
      max_unacked_frames_(GetMaxUnackedFrames(target_playout_delay_)),
      configured_encoder_bitrate_(audio_config.bitrate),
      rtcp_(cast_environment,
            this,
            transport_sender_,
            NULL,  // paced sender.
            NULL,
            audio_config.rtcp_mode,
            base::TimeDelta::FromMilliseconds(audio_config.rtcp_interval),
            audio_config.rtp_config.ssrc,
            audio_config.incoming_feedback_ssrc,
            audio_config.rtcp_c_name,
            AUDIO_EVENT),
      rtp_timestamp_helper_(audio_config.frequency),
      num_aggressive_rtcp_reports_sent_(0),
      last_sent_frame_id_(0),
      latest_acked_frame_id_(0),
      duplicate_ack_counter_(0),
      cast_initialization_status_(STATUS_AUDIO_UNINITIALIZED),
      weak_factory_(this) {
  VLOG(1) << "max_unacked_frames " << max_unacked_frames_;
  DCHECK_GT(max_unacked_frames_, 0);

  if (!audio_config.use_external_encoder) {
    audio_encoder_.reset(
        new AudioEncoder(cast_environment,
                         audio_config,
                         base::Bind(&AudioSender::SendEncodedAudioFrame,
                                    weak_factory_.GetWeakPtr())));
    cast_initialization_status_ = audio_encoder_->InitializationResult();
  } else {
    NOTREACHED();  // No support for external audio encoding.
    cast_initialization_status_ = STATUS_AUDIO_UNINITIALIZED;
  }

  media::cast::transport::CastTransportAudioConfig transport_config;
  transport_config.codec = audio_config.codec;
  transport_config.rtp.config = audio_config.rtp_config;
  transport_config.frequency = audio_config.frequency;
  transport_config.channels = audio_config.channels;
  transport_config.rtp.max_outstanding_frames = max_unacked_frames_;
  transport_sender_->InitializeAudio(transport_config);

  rtcp_.SetCastReceiverEventHistorySize(kReceiverRtcpEventHistorySize);

  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));
}

AudioSender::~AudioSender() {}

void AudioSender::InsertAudio(scoped_ptr<AudioBus> audio_bus,
                              const base::TimeTicks& recorded_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (cast_initialization_status_ != STATUS_AUDIO_INITIALIZED) {
    NOTREACHED();
    return;
  }
  DCHECK(audio_encoder_.get()) << "Invalid internal state";

  if (AreTooManyFramesInFlight()) {
    VLOG(1) << "Dropping frame due to too many frames currently in-flight.";
    return;
  }

  audio_encoder_->InsertAudio(audio_bus.Pass(), recorded_time);
}

void AudioSender::SendEncodedAudioFrame(
    scoped_ptr<transport::EncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  const uint32 frame_id = encoded_frame->frame_id;

  const bool is_first_frame_to_be_sent = last_send_time_.is_null();
  last_send_time_ = cast_environment_->Clock()->NowTicks();
  last_sent_frame_id_ = frame_id;
  // If this is the first frame about to be sent, fake the value of
  // |latest_acked_frame_id_| to indicate the receiver starts out all caught up.
  // Also, schedule the periodic frame re-send checks.
  if (is_first_frame_to_be_sent) {
    latest_acked_frame_id_ = frame_id - 1;
    ScheduleNextResendCheck();
  }

  cast_environment_->Logging()->InsertEncodedFrameEvent(
      last_send_time_, FRAME_ENCODED, AUDIO_EVENT, encoded_frame->rtp_timestamp,
      frame_id, static_cast<int>(encoded_frame->data.size()),
      encoded_frame->dependency == transport::EncodedFrame::KEY,
      configured_encoder_bitrate_);
  // Only use lowest 8 bits as key.
  frame_id_to_rtp_timestamp_[frame_id & 0xff] = encoded_frame->rtp_timestamp;

  DCHECK(!encoded_frame->reference_time.is_null());
  rtp_timestamp_helper_.StoreLatestTime(encoded_frame->reference_time,
                                        encoded_frame->rtp_timestamp);

  // At the start of the session, it's important to send reports before each
  // frame so that the receiver can properly compute playout times.  The reason
  // more than one report is sent is because transmission is not guaranteed,
  // only best effort, so we send enough that one should almost certainly get
  // through.
  if (num_aggressive_rtcp_reports_sent_ < kNumAggressiveReportsSentAtStart) {
    // SendRtcpReport() will schedule future reports to be made if this is the
    // last "aggressive report."
    ++num_aggressive_rtcp_reports_sent_;
    const bool is_last_aggressive_report =
        (num_aggressive_rtcp_reports_sent_ == kNumAggressiveReportsSentAtStart);
    VLOG_IF(1, is_last_aggressive_report) << "Sending last aggressive report.";
    SendRtcpReport(is_last_aggressive_report);
  }

  transport_sender_->InsertCodedAudioFrame(*encoded_frame);
}

void AudioSender::IncomingRtcpPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  rtcp_.IncomingRtcpPacket(&packet->front(), packet->size());
}

void AudioSender::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next =
      rtcp_.TimeToSendNextRtcpReport() - cast_environment_->Clock()->NowTicks();

  time_to_next = std::max(
      time_to_next, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&AudioSender::SendRtcpReport,
                 weak_factory_.GetWeakPtr(),
                 true),
      time_to_next);
}

void AudioSender::SendRtcpReport(bool schedule_future_reports) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  uint32 now_as_rtp_timestamp = 0;
  if (rtp_timestamp_helper_.GetCurrentTimeAsRtpTimestamp(
          now, &now_as_rtp_timestamp)) {
    rtcp_.SendRtcpFromRtpSender(now, now_as_rtp_timestamp);
  } else {
    // |rtp_timestamp_helper_| should have stored a mapping by this point.
    NOTREACHED();
  }
  if (schedule_future_reports)
    ScheduleNextRtcpReport();
}

void AudioSender::ScheduleNextResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  base::TimeDelta time_to_next =
      last_send_time_ - cast_environment_->Clock()->NowTicks() +
      target_playout_delay_;
  time_to_next = std::max(
      time_to_next, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));
  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&AudioSender::ResendCheck, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void AudioSender::ResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  const base::TimeDelta time_since_last_send =
      cast_environment_->Clock()->NowTicks() - last_send_time_;
  if (time_since_last_send > target_playout_delay_) {
    if (latest_acked_frame_id_ == last_sent_frame_id_) {
      // Last frame acked, no point in doing anything
    } else {
      VLOG(1) << "ACK timeout; last acked frame: " << latest_acked_frame_id_;
      ResendForKickstart();
    }
  }
  ScheduleNextResendCheck();
}

void AudioSender::OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  if (rtcp_.is_rtt_available()) {
    // Having the RTT values implies the receiver sent back a receiver report
    // based on it having received a report from here.  Therefore, ensure this
    // sender stops aggressively sending reports.
    if (num_aggressive_rtcp_reports_sent_ < kNumAggressiveReportsSentAtStart) {
      VLOG(1) << "No longer a need to send reports aggressively (sent "
              << num_aggressive_rtcp_reports_sent_ << ").";
      num_aggressive_rtcp_reports_sent_ = kNumAggressiveReportsSentAtStart;
      ScheduleNextRtcpReport();
    }
  }

  if (last_send_time_.is_null())
    return;  // Cannot get an ACK without having first sent a frame.

  if (cast_feedback.missing_frames_and_packets_.empty()) {
    // We only count duplicate ACKs when we have sent newer frames.
    if (latest_acked_frame_id_ == cast_feedback.ack_frame_id_ &&
        latest_acked_frame_id_ != last_sent_frame_id_) {
      duplicate_ack_counter_++;
    } else {
      duplicate_ack_counter_ = 0;
    }
    // TODO(miu): The values "2" and "3" should be derived from configuration.
    if (duplicate_ack_counter_ >= 2 && duplicate_ack_counter_ % 3 == 2) {
      VLOG(1) << "Received duplicate ACK for frame " << latest_acked_frame_id_;
      ResendForKickstart();
    }
  } else {
    // Only count duplicated ACKs if there is no NACK request in between.
    // This is to avoid aggresive resend.
    duplicate_ack_counter_ = 0;

    base::TimeDelta rtt;
    base::TimeDelta avg_rtt;
    base::TimeDelta min_rtt;
    base::TimeDelta max_rtt;
    rtcp_.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt);

    // A NACK is also used to cancel pending re-transmissions.
    transport_sender_->ResendPackets(
        true, cast_feedback.missing_frames_and_packets_, false, min_rtt);
  }

  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();

  const RtpTimestamp rtp_timestamp =
      frame_id_to_rtp_timestamp_[cast_feedback.ack_frame_id_ & 0xff];
  cast_environment_->Logging()->InsertFrameEvent(now,
                                                 FRAME_ACK_RECEIVED,
                                                 AUDIO_EVENT,
                                                 rtp_timestamp,
                                                 cast_feedback.ack_frame_id_);

  const bool is_acked_out_of_order =
      static_cast<int32>(cast_feedback.ack_frame_id_ -
                             latest_acked_frame_id_) < 0;
  VLOG(2) << "Received ACK" << (is_acked_out_of_order ? " out-of-order" : "")
          << " for frame " << cast_feedback.ack_frame_id_;
  if (!is_acked_out_of_order) {
    // Cancel resends of acked frames.
    MissingFramesAndPacketsMap missing_frames_and_packets;
    PacketIdSet missing;
    while (latest_acked_frame_id_ != cast_feedback.ack_frame_id_) {
      latest_acked_frame_id_++;
      missing_frames_and_packets[latest_acked_frame_id_] = missing;
    }
    transport_sender_->ResendPackets(
        true, missing_frames_and_packets, true, base::TimeDelta());
    latest_acked_frame_id_ = cast_feedback.ack_frame_id_;
  }
}

bool AudioSender::AreTooManyFramesInFlight() const {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  int frames_in_flight = 0;
  if (!last_send_time_.is_null()) {
    frames_in_flight +=
        static_cast<int32>(last_sent_frame_id_ - latest_acked_frame_id_);
  }
  VLOG(2) << frames_in_flight
          << " frames in flight; last sent: " << last_sent_frame_id_
          << " latest acked: " << latest_acked_frame_id_;
  return frames_in_flight >= max_unacked_frames_;
}

void AudioSender::ResendForKickstart() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  VLOG(1) << "Resending last packet of frame " << last_sent_frame_id_
          << " to kick-start.";
  // Send the first packet of the last encoded frame to kick start
  // retransmission. This gives enough information to the receiver what
  // packets and frames are missing.
  MissingFramesAndPacketsMap missing_frames_and_packets;
  PacketIdSet missing;
  missing.insert(kRtcpCastLastPacket);
  missing_frames_and_packets.insert(
      std::make_pair(last_sent_frame_id_, missing));
  last_send_time_ = cast_environment_->Clock()->NowTicks();

  base::TimeDelta rtt;
  base::TimeDelta avg_rtt;
  base::TimeDelta min_rtt;
  base::TimeDelta max_rtt;
  rtcp_.Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt);

  // Sending this extra packet is to kick-start the session. There is
  // no need to optimize re-transmission for this case.
  transport_sender_->ResendPackets(
      true, missing_frames_and_packets, false, min_rtt);
}

}  // namespace cast
}  // namespace media
