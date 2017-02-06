// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_decoder_default.h"

#include <limits>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {

AudioDecoderDefault::AudioDecoderDefault()
    : delegate_(nullptr),
      last_push_pts_(std::numeric_limits<int64_t>::min()),
      weak_factory_(this) {}

AudioDecoderDefault::~AudioDecoderDefault() {}

void AudioDecoderDefault::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

MediaPipelineBackend::BufferStatus AudioDecoderDefault::PushBuffer(
    CastDecoderBuffer* buffer) {
  DCHECK(delegate_);
  DCHECK(buffer);

  if (buffer->end_of_stream()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&AudioDecoderDefault::OnEndOfStream,
                              weak_factory_.GetWeakPtr()));
  } else {
    last_push_pts_ = buffer->timestamp();
  }
  return MediaPipelineBackend::kBufferSuccess;
}

void AudioDecoderDefault::GetStatistics(Statistics* statistics) {
}

bool AudioDecoderDefault::SetConfig(const AudioConfig& config) {
  return true;
}

bool AudioDecoderDefault::SetVolume(float multiplier) {
  return true;
}

AudioDecoderDefault::RenderingDelay AudioDecoderDefault::GetRenderingDelay() {
  return RenderingDelay();
}

void AudioDecoderDefault::OnEndOfStream() {
  delegate_->OnEndOfStream();
}

}  // namespace media
}  // namespace chromecast
