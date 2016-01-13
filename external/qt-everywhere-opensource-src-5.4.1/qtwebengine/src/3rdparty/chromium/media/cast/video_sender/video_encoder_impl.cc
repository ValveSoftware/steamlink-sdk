// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_sender/video_encoder_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_defines.h"
#include "media/cast/video_sender/codecs/vp8/vp8_encoder.h"
#include "media/cast/video_sender/fake_software_video_encoder.h"

namespace media {
namespace cast {

namespace {

typedef base::Callback<void(Vp8Encoder*)> PassEncoderCallback;

void InitializeEncoderOnEncoderThread(
    const scoped_refptr<CastEnvironment>& environment,
    SoftwareVideoEncoder* encoder) {
  DCHECK(environment->CurrentlyOn(CastEnvironment::VIDEO));
  encoder->Initialize();
}

void EncodeVideoFrameOnEncoderThread(
    scoped_refptr<CastEnvironment> environment,
    SoftwareVideoEncoder* encoder,
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& capture_time,
    const VideoEncoderImpl::CodecDynamicConfig& dynamic_config,
    const VideoEncoderImpl::FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(environment->CurrentlyOn(CastEnvironment::VIDEO));
  if (dynamic_config.key_frame_requested) {
    encoder->GenerateKeyFrame();
  }
  encoder->LatestFrameIdToReference(
      dynamic_config.latest_frame_id_to_reference);
  encoder->UpdateRates(dynamic_config.bit_rate);

  scoped_ptr<transport::EncodedFrame> encoded_frame(
      new transport::EncodedFrame());
  if (!encoder->Encode(video_frame, encoded_frame.get())) {
    VLOG(1) << "Encoding failed";
    return;
  }
  if (encoded_frame->data.empty()) {
    VLOG(1) << "Encoding resulted in an empty frame";
    return;
  }
  encoded_frame->rtp_timestamp = transport::GetVideoRtpTimestamp(capture_time);
  encoded_frame->reference_time = capture_time;

  environment->PostTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(
          frame_encoded_callback, base::Passed(&encoded_frame)));
}
}  // namespace

VideoEncoderImpl::VideoEncoderImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    int max_unacked_frames)
    : video_config_(video_config),
      cast_environment_(cast_environment) {
  if (video_config.codec == transport::kVp8) {
    encoder_.reset(new Vp8Encoder(video_config, max_unacked_frames));
    cast_environment_->PostTask(CastEnvironment::VIDEO,
                                FROM_HERE,
                                base::Bind(&InitializeEncoderOnEncoderThread,
                                           cast_environment,
                                           encoder_.get()));
#ifndef OFFICIAL_BUILD
  } else if (video_config.codec == transport::kFakeSoftwareVideo) {
    encoder_.reset(new FakeSoftwareVideoEncoder(video_config));
#endif
  } else {
    DCHECK(false) << "Invalid config";  // Codec not supported.
  }

  dynamic_config_.key_frame_requested = false;
  dynamic_config_.latest_frame_id_to_reference = kStartFrameId;
  dynamic_config_.bit_rate = video_config.start_bitrate;
}

VideoEncoderImpl::~VideoEncoderImpl() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (encoder_) {
    cast_environment_->PostTask(
        CastEnvironment::VIDEO,
        FROM_HERE,
        base::Bind(&base::DeletePointer<SoftwareVideoEncoder>,
                   encoder_.release()));
  }
}

bool VideoEncoderImpl::EncodeVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& capture_time,
    const FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_environment_->PostTask(CastEnvironment::VIDEO,
                              FROM_HERE,
                              base::Bind(&EncodeVideoFrameOnEncoderThread,
                                         cast_environment_,
                                         encoder_.get(),
                                         video_frame,
                                         capture_time,
                                         dynamic_config_,
                                         frame_encoded_callback));

  dynamic_config_.key_frame_requested = false;
  return true;
}

// Inform the encoder about the new target bit rate.
void VideoEncoderImpl::SetBitRate(int new_bit_rate) {
  dynamic_config_.bit_rate = new_bit_rate;
}

// Inform the encoder to encode the next frame as a key frame.
void VideoEncoderImpl::GenerateKeyFrame() {
  dynamic_config_.key_frame_requested = true;
}

// Inform the encoder to only reference frames older or equal to frame_id;
void VideoEncoderImpl::LatestFrameIdToReference(uint32 frame_id) {
  dynamic_config_.latest_frame_id_to_reference = frame_id;
}

}  //  namespace cast
}  //  namespace media
