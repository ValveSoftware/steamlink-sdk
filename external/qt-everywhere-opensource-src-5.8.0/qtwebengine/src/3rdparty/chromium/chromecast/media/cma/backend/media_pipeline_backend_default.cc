// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_default.h"

#include <algorithm>
#include <limits>

#include "chromecast/media/cma/backend/audio_decoder_default.h"
#include "chromecast/media/cma/backend/video_decoder_default.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {

MediaPipelineBackendDefault::MediaPipelineBackendDefault()
    : start_pts_(std::numeric_limits<int64_t>::min()),
      running_(false),
      rate_(1.0f) {}

MediaPipelineBackendDefault::~MediaPipelineBackendDefault() {
}

MediaPipelineBackend::AudioDecoder*
MediaPipelineBackendDefault::CreateAudioDecoder() {
  DCHECK(!audio_decoder_);
  audio_decoder_.reset(new AudioDecoderDefault());
  return audio_decoder_.get();
}

MediaPipelineBackend::VideoDecoder*
MediaPipelineBackendDefault::CreateVideoDecoder() {
  DCHECK(!video_decoder_);
  video_decoder_.reset(new VideoDecoderDefault());
  return video_decoder_.get();
}

bool MediaPipelineBackendDefault::Initialize() {
  return true;
}

bool MediaPipelineBackendDefault::Start(int64_t start_pts) {
  DCHECK(!running_);
  start_pts_ = start_pts;
  start_clock_ = base::TimeTicks::Now();
  running_ = true;
  return true;
}

bool MediaPipelineBackendDefault::Stop() {
  start_pts_ = GetCurrentPts();
  running_ = false;
  return true;
}

bool MediaPipelineBackendDefault::Pause() {
  DCHECK(running_);
  start_pts_ = GetCurrentPts();
  running_ = false;
  return true;
}

bool MediaPipelineBackendDefault::Resume() {
  DCHECK(!running_);
  running_ = true;
  start_clock_ = base::TimeTicks::Now();
  return true;
}

int64_t MediaPipelineBackendDefault::GetCurrentPts() {
  if (!running_)
    return start_pts_;

  if (audio_decoder_ &&
      audio_decoder_->last_push_pts() != std::numeric_limits<int64_t>::min()) {
    start_pts_ = std::min(start_pts_, audio_decoder_->last_push_pts());
  }
  if (video_decoder_ &&
      video_decoder_->last_push_pts() != std::numeric_limits<int64_t>::min()) {
    start_pts_ = std::min(start_pts_, video_decoder_->last_push_pts());
  }

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta interpolated_media_time =
      base::TimeDelta::FromMicroseconds(start_pts_) +
      (now - start_clock_) * rate_;

  return interpolated_media_time.InMicroseconds();
}

bool MediaPipelineBackendDefault::SetPlaybackRate(float rate) {
  DCHECK_GT(rate, 0.0f);
  start_pts_ = GetCurrentPts();
  start_clock_ = base::TimeTicks::Now();
  rate_ = rate;
  return true;
}

}  // namespace media
}  // namespace chromecast
