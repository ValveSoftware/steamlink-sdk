// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/frame_receiver.h"

#include <algorithm>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/cast_environment.h"

namespace {
const int kMinSchedulingDelayMs = 1;
}  // namespace

namespace media {
namespace cast {

FrameReceiver::FrameReceiver(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const FrameReceiverConfig& config,
    EventMediaType event_media_type,
    transport::PacedPacketSender* const packet_sender)
    : cast_environment_(cast_environment),
      packet_parser_(config.incoming_ssrc, config.rtp_payload_type),
      stats_(cast_environment->Clock()),
      event_media_type_(event_media_type),
      event_subscriber_(kReceiverRtcpEventHistorySize, event_media_type),
      rtp_timebase_(config.frequency),
      target_playout_delay_(
          base::TimeDelta::FromMilliseconds(config.rtp_max_delay_ms)),
      expected_frame_duration_(
          base::TimeDelta::FromSeconds(1) / config.max_frame_rate),
      reports_are_scheduled_(false),
      framer_(cast_environment->Clock(),
              this,
              config.incoming_ssrc,
              true,
              config.rtp_max_delay_ms * config.max_frame_rate / 1000),
      rtcp_(cast_environment_,
            NULL,
            NULL,
            packet_sender,
            &stats_,
            config.rtcp_mode,
            base::TimeDelta::FromMilliseconds(config.rtcp_interval),
            config.feedback_ssrc,
            config.incoming_ssrc,
            config.rtcp_c_name,
            event_media_type),
      is_waiting_for_consecutive_frame_(false),
      lip_sync_drift_(ClockDriftSmoother::GetDefaultTimeConstant()),
      weak_factory_(this) {
  DCHECK_GT(config.rtp_max_delay_ms, 0);
  DCHECK_GT(config.max_frame_rate, 0);
  decryptor_.Initialize(config.aes_key, config.aes_iv_mask);
  rtcp_.SetTargetDelay(target_playout_delay_);
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);
  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));
}

FrameReceiver::~FrameReceiver() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber_);
}

void FrameReceiver::RequestEncodedFrame(
    const ReceiveEncodedFrameCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  frame_request_queue_.push_back(callback);
  EmitAvailableEncodedFrames();
}

bool FrameReceiver::ProcessPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  if (Rtcp::IsRtcpPacket(&packet->front(), packet->size())) {
    rtcp_.IncomingRtcpPacket(&packet->front(), packet->size());
  } else {
    RtpCastHeader rtp_header;
    const uint8* payload_data;
    size_t payload_size;
    if (!packet_parser_.ParsePacket(&packet->front(),
                                    packet->size(),
                                    &rtp_header,
                                    &payload_data,
                                    &payload_size)) {
      return false;
    }

    ProcessParsedPacket(rtp_header, payload_data, payload_size);
    stats_.UpdateStatistics(rtp_header);
  }

  if (!reports_are_scheduled_) {
    ScheduleNextRtcpReport();
    ScheduleNextCastMessage();
    reports_are_scheduled_ = true;
  }

  return true;
}

// static
bool FrameReceiver::ParseSenderSsrc(const uint8* packet,
                                    size_t length,
                                    uint32* ssrc) {
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(packet), length);
  return big_endian_reader.Skip(8) && big_endian_reader.ReadU32(ssrc);
}

void FrameReceiver::ProcessParsedPacket(const RtpCastHeader& rtp_header,
                                        const uint8* payload_data,
                                        size_t payload_size) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();

  frame_id_to_rtp_timestamp_[rtp_header.frame_id & 0xff] =
      rtp_header.rtp_timestamp;
  cast_environment_->Logging()->InsertPacketEvent(
      now, PACKET_RECEIVED, event_media_type_, rtp_header.rtp_timestamp,
      rtp_header.frame_id, rtp_header.packet_id, rtp_header.max_packet_id,
      payload_size);

  bool duplicate = false;
  const bool complete =
      framer_.InsertPacket(payload_data, payload_size, rtp_header, &duplicate);

  // Duplicate packets are ignored.
  if (duplicate)
    return;

  // Update lip-sync values upon receiving the first packet of each frame, or if
  // they have never been set yet.
  if (rtp_header.packet_id == 0 || lip_sync_reference_time_.is_null()) {
    RtpTimestamp fresh_sync_rtp;
    base::TimeTicks fresh_sync_reference;
    if (!rtcp_.GetLatestLipSyncTimes(&fresh_sync_rtp, &fresh_sync_reference)) {
      // HACK: The sender should have provided Sender Reports before the first
      // frame was sent.  However, the spec does not currently require this.
      // Therefore, when the data is missing, the local clock is used to
      // generate reference timestamps.
      VLOG(2) << "Lip sync info missing.  Falling-back to local clock.";
      fresh_sync_rtp = rtp_header.rtp_timestamp;
      fresh_sync_reference = now;
    }
    // |lip_sync_reference_time_| is always incremented according to the time
    // delta computed from the difference in RTP timestamps.  Then,
    // |lip_sync_drift_| accounts for clock drift and also smoothes-out any
    // sudden/discontinuous shifts in the series of reference time values.
    if (lip_sync_reference_time_.is_null()) {
      lip_sync_reference_time_ = fresh_sync_reference;
    } else {
      lip_sync_reference_time_ += RtpDeltaToTimeDelta(
          static_cast<int32>(fresh_sync_rtp - lip_sync_rtp_timestamp_),
          rtp_timebase_);
    }
    lip_sync_rtp_timestamp_ = fresh_sync_rtp;
    lip_sync_drift_.Update(
        now, fresh_sync_reference - lip_sync_reference_time_);
  }

  // Another frame is complete from a non-duplicate packet.  Attempt to emit
  // more frames to satisfy enqueued requests.
  if (complete)
    EmitAvailableEncodedFrames();
}

void FrameReceiver::CastFeedback(const RtcpCastMessage& cast_message) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  RtpTimestamp rtp_timestamp =
      frame_id_to_rtp_timestamp_[cast_message.ack_frame_id_ & 0xff];
  cast_environment_->Logging()->InsertFrameEvent(
      now, FRAME_ACK_SENT, event_media_type_,
      rtp_timestamp, cast_message.ack_frame_id_);

  ReceiverRtcpEventSubscriber::RtcpEventMultiMap rtcp_events;
  event_subscriber_.GetRtcpEventsAndReset(&rtcp_events);
  rtcp_.SendRtcpFromRtpReceiver(&cast_message, &rtcp_events);
}

void FrameReceiver::EmitAvailableEncodedFrames() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  while (!frame_request_queue_.empty()) {
    // Attempt to peek at the next completed frame from the |framer_|.
    // TODO(miu): We should only be peeking at the metadata, and not copying the
    // payload yet!  Or, at least, peek using a StringPiece instead of a copy.
    scoped_ptr<transport::EncodedFrame> encoded_frame(
        new transport::EncodedFrame());
    bool is_consecutively_next_frame = false;
    bool have_multiple_complete_frames = false;
    if (!framer_.GetEncodedFrame(encoded_frame.get(),
                                 &is_consecutively_next_frame,
                                 &have_multiple_complete_frames)) {
      VLOG(1) << "Wait for more packets to produce a completed frame.";
      return;  // ProcessParsedPacket() will invoke this method in the future.
    }

    const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
    const base::TimeTicks playout_time =
        GetPlayoutTime(encoded_frame->rtp_timestamp);

    // If we have multiple decodable frames, and the current frame is
    // too old, then skip it and decode the next frame instead.
    if (have_multiple_complete_frames && now > playout_time) {
      framer_.ReleaseFrame(encoded_frame->frame_id);
      continue;
    }

    // If |framer_| has a frame ready that is out of sequence, examine the
    // playout time to determine whether it's acceptable to continue, thereby
    // skipping one or more frames.  Skip if the missing frame wouldn't complete
    // playing before the start of playback of the available frame.
    if (!is_consecutively_next_frame) {
      // TODO(miu): Also account for expected decode time here?
      const base::TimeTicks earliest_possible_end_time_of_missing_frame =
          now + expected_frame_duration_;
      if (earliest_possible_end_time_of_missing_frame < playout_time) {
        VLOG(1) << "Wait for next consecutive frame instead of skipping.";
        if (!is_waiting_for_consecutive_frame_) {
          is_waiting_for_consecutive_frame_ = true;
          cast_environment_->PostDelayedTask(
              CastEnvironment::MAIN,
              FROM_HERE,
              base::Bind(&FrameReceiver::EmitAvailableEncodedFramesAfterWaiting,
                         weak_factory_.GetWeakPtr()),
              playout_time - now);
        }
        return;
      }
    }

    // Decrypt the payload data in the frame, if crypto is being used.
    if (decryptor_.initialized()) {
      std::string decrypted_data;
      if (!decryptor_.Decrypt(encoded_frame->frame_id,
                              encoded_frame->data,
                              &decrypted_data)) {
        // Decryption failed.  Give up on this frame.
        framer_.ReleaseFrame(encoded_frame->frame_id);
        continue;
      }
      encoded_frame->data.swap(decrypted_data);
    }

    // At this point, we have a decrypted EncodedFrame ready to be emitted.
    encoded_frame->reference_time = playout_time;
    framer_.ReleaseFrame(encoded_frame->frame_id);
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(frame_request_queue_.front(),
                                           base::Passed(&encoded_frame)));
    frame_request_queue_.pop_front();
  }
}

void FrameReceiver::EmitAvailableEncodedFramesAfterWaiting() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(is_waiting_for_consecutive_frame_);
  is_waiting_for_consecutive_frame_ = false;
  EmitAvailableEncodedFrames();
}

base::TimeTicks FrameReceiver::GetPlayoutTime(uint32 rtp_timestamp) const {
  return lip_sync_reference_time_ +
      lip_sync_drift_.Current() +
      RtpDeltaToTimeDelta(
          static_cast<int32>(rtp_timestamp - lip_sync_rtp_timestamp_),
          rtp_timebase_) +
      target_playout_delay_;
}

void FrameReceiver::ScheduleNextCastMessage() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeTicks send_time;
  framer_.TimeToSendNextCastMessage(&send_time);
  base::TimeDelta time_to_send =
      send_time - cast_environment_->Clock()->NowTicks();
  time_to_send = std::max(
      time_to_send, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));
  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&FrameReceiver::SendNextCastMessage,
                 weak_factory_.GetWeakPtr()),
      time_to_send);
}

void FrameReceiver::SendNextCastMessage() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  framer_.SendCastMessage();  // Will only send a message if it is time.
  ScheduleNextCastMessage();
}

void FrameReceiver::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next = rtcp_.TimeToSendNextRtcpReport() -
                                 cast_environment_->Clock()->NowTicks();

  time_to_next = std::max(
      time_to_next, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&FrameReceiver::SendNextRtcpReport,
                 weak_factory_.GetWeakPtr()),
      time_to_next);
}

void FrameReceiver::SendNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  rtcp_.SendRtcpFromRtpReceiver(NULL, NULL);
  ScheduleNextRtcpReport();
}

}  // namespace cast
}  // namespace media
