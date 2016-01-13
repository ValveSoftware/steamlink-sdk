// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/cast_receiver_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/receiver/audio_decoder.h"
#include "media/cast/receiver/video_decoder.h"

namespace media {
namespace cast {

scoped_ptr<CastReceiver> CastReceiver::Create(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameReceiverConfig& audio_config,
    const FrameReceiverConfig& video_config,
    transport::PacketSender* const packet_sender) {
  return scoped_ptr<CastReceiver>(new CastReceiverImpl(
      cast_environment, audio_config, video_config, packet_sender));
}

CastReceiverImpl::CastReceiverImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameReceiverConfig& audio_config,
    const FrameReceiverConfig& video_config,
    transport::PacketSender* const packet_sender)
    : cast_environment_(cast_environment),
      pacer_(cast_environment->Clock(),
             cast_environment->Logging(),
             packet_sender,
             cast_environment->GetTaskRunner(CastEnvironment::MAIN)),
      audio_receiver_(cast_environment, audio_config, AUDIO_EVENT, &pacer_),
      video_receiver_(cast_environment, video_config, VIDEO_EVENT, &pacer_),
      ssrc_of_audio_sender_(audio_config.incoming_ssrc),
      ssrc_of_video_sender_(video_config.incoming_ssrc),
      num_audio_channels_(audio_config.channels),
      audio_sampling_rate_(audio_config.frequency),
      audio_codec_(audio_config.codec.audio),
      video_codec_(video_config.codec.video) {}

CastReceiverImpl::~CastReceiverImpl() {}

void CastReceiverImpl::DispatchReceivedPacket(scoped_ptr<Packet> packet) {
  const uint8_t* const data = &packet->front();
  const size_t length = packet->size();

  uint32 ssrc_of_sender;
  if (Rtcp::IsRtcpPacket(data, length)) {
    ssrc_of_sender = Rtcp::GetSsrcOfSender(data, length);
  } else if (!FrameReceiver::ParseSenderSsrc(data, length, &ssrc_of_sender)) {
    VLOG(1) << "Invalid RTP packet.";
    return;
  }

  base::WeakPtr<FrameReceiver> target;
  if (ssrc_of_sender == ssrc_of_video_sender_) {
    target = video_receiver_.AsWeakPtr();
  } else if (ssrc_of_sender == ssrc_of_audio_sender_) {
    target = audio_receiver_.AsWeakPtr();
  } else {
    VLOG(1) << "Dropping packet with a non matching sender SSRC: "
            << ssrc_of_sender;
    return;
  }
  cast_environment_->PostTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&FrameReceiver::ProcessPacket),
                 target,
                 base::Passed(&packet)));
}

transport::PacketReceiverCallback CastReceiverImpl::packet_receiver() {
  return base::Bind(&CastReceiverImpl::DispatchReceivedPacket,
                    // TODO(miu): This code structure is dangerous, since the
                    // callback could be stored and then invoked after
                    // destruction of |this|.
                    base::Unretained(this));
}

void CastReceiverImpl::RequestDecodedAudioFrame(
    const AudioFrameDecodedCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!callback.is_null());
  audio_receiver_.RequestEncodedFrame(base::Bind(
      &CastReceiverImpl::DecodeEncodedAudioFrame,
      // Note: Use of Unretained is safe since this Closure is guaranteed to be
      // invoked or discarded by |audio_receiver_| before destruction of |this|.
      base::Unretained(this),
      callback));
}

void CastReceiverImpl::RequestEncodedAudioFrame(
    const ReceiveEncodedFrameCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  audio_receiver_.RequestEncodedFrame(callback);
}

void CastReceiverImpl::RequestDecodedVideoFrame(
    const VideoFrameDecodedCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!callback.is_null());
  video_receiver_.RequestEncodedFrame(base::Bind(
      &CastReceiverImpl::DecodeEncodedVideoFrame,
      // Note: Use of Unretained is safe since this Closure is guaranteed to be
      // invoked or discarded by |video_receiver_| before destruction of |this|.
      base::Unretained(this),
      callback));
}

void CastReceiverImpl::RequestEncodedVideoFrame(
    const ReceiveEncodedFrameCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  video_receiver_.RequestEncodedFrame(callback);
}

void CastReceiverImpl::DecodeEncodedAudioFrame(
    const AudioFrameDecodedCallback& callback,
    scoped_ptr<transport::EncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!encoded_frame) {
    callback.Run(make_scoped_ptr<AudioBus>(NULL), base::TimeTicks(), false);
    return;
  }

  if (!audio_decoder_) {
    audio_decoder_.reset(new AudioDecoder(cast_environment_,
                                          num_audio_channels_,
                                          audio_sampling_rate_,
                                          audio_codec_));
  }
  const uint32 frame_id = encoded_frame->frame_id;
  const uint32 rtp_timestamp = encoded_frame->rtp_timestamp;
  const base::TimeTicks playout_time = encoded_frame->reference_time;
  audio_decoder_->DecodeFrame(
      encoded_frame.Pass(),
      base::Bind(&CastReceiverImpl::EmitDecodedAudioFrame,
                 cast_environment_,
                 callback,
                 frame_id,
                 rtp_timestamp,
                 playout_time));
}

void CastReceiverImpl::DecodeEncodedVideoFrame(
    const VideoFrameDecodedCallback& callback,
    scoped_ptr<transport::EncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!encoded_frame) {
    callback.Run(
        make_scoped_refptr<VideoFrame>(NULL), base::TimeTicks(), false);
    return;
  }

  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT2(
      "cast_perf_test", "PullEncodedVideoFrame",
      TRACE_EVENT_SCOPE_THREAD,
      "rtp_timestamp", encoded_frame->rtp_timestamp,
      "render_time", encoded_frame->reference_time.ToInternalValue());

  if (!video_decoder_)
    video_decoder_.reset(new VideoDecoder(cast_environment_, video_codec_));
  const uint32 frame_id = encoded_frame->frame_id;
  const uint32 rtp_timestamp = encoded_frame->rtp_timestamp;
  const base::TimeTicks playout_time = encoded_frame->reference_time;
  video_decoder_->DecodeFrame(
      encoded_frame.Pass(),
      base::Bind(&CastReceiverImpl::EmitDecodedVideoFrame,
                 cast_environment_,
                 callback,
                 frame_id,
                 rtp_timestamp,
                 playout_time));
}

// static
void CastReceiverImpl::EmitDecodedAudioFrame(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const AudioFrameDecodedCallback& callback,
    uint32 frame_id,
    uint32 rtp_timestamp,
    const base::TimeTicks& playout_time,
    scoped_ptr<AudioBus> audio_bus,
    bool is_continuous) {
  DCHECK(cast_environment->CurrentlyOn(CastEnvironment::MAIN));
  if (audio_bus.get()) {
    const base::TimeTicks now = cast_environment->Clock()->NowTicks();
    cast_environment->Logging()->InsertFrameEvent(
        now, FRAME_DECODED, AUDIO_EVENT, rtp_timestamp, frame_id);
    cast_environment->Logging()->InsertFrameEventWithDelay(
        now, FRAME_PLAYOUT, AUDIO_EVENT, rtp_timestamp, frame_id,
        playout_time - now);
  }
  callback.Run(audio_bus.Pass(), playout_time, is_continuous);
}

// static
void CastReceiverImpl::EmitDecodedVideoFrame(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const VideoFrameDecodedCallback& callback,
    uint32 frame_id,
    uint32 rtp_timestamp,
    const base::TimeTicks& playout_time,
    const scoped_refptr<VideoFrame>& video_frame,
    bool is_continuous) {
  DCHECK(cast_environment->CurrentlyOn(CastEnvironment::MAIN));
  if (video_frame) {
    const base::TimeTicks now = cast_environment->Clock()->NowTicks();
    cast_environment->Logging()->InsertFrameEvent(
        now, FRAME_DECODED, VIDEO_EVENT, rtp_timestamp, frame_id);
    cast_environment->Logging()->InsertFrameEventWithDelay(
        now, FRAME_PLAYOUT, VIDEO_EVENT, rtp_timestamp, frame_id,
        playout_time - now);

    // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
    TRACE_EVENT_INSTANT1(
        "cast_perf_test", "FrameDecoded",
        TRACE_EVENT_SCOPE_THREAD,
        "rtp_timestamp", rtp_timestamp);
  }
  callback.Run(video_frame, playout_time, is_continuous);
}

}  // namespace cast
}  // namespace media
