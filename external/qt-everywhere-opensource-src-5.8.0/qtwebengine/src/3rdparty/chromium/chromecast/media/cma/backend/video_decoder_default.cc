// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video_decoder_default.h"

#include <limits>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {

VideoDecoderDefault::VideoDecoderDefault()
    : delegate_(nullptr),
      last_push_pts_(std::numeric_limits<int64_t>::min()),
      weak_factory_(this) {}

VideoDecoderDefault::~VideoDecoderDefault() {}

void VideoDecoderDefault::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

MediaPipelineBackend::BufferStatus VideoDecoderDefault::PushBuffer(
    CastDecoderBuffer* buffer) {
  DCHECK(delegate_);
  DCHECK(buffer);
  if (buffer->end_of_stream()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&VideoDecoderDefault::OnEndOfStream,
                              weak_factory_.GetWeakPtr()));
  } else {
    last_push_pts_ = buffer->timestamp();
  }
  return MediaPipelineBackend::kBufferSuccess;
}

void VideoDecoderDefault::GetStatistics(Statistics* statistics) {
}

bool VideoDecoderDefault::SetConfig(const VideoConfig& config) {
  return true;
}

void VideoDecoderDefault::OnEndOfStream() {
  delegate_->OnEndOfStream();
}

}  // namespace media
}  // namespace chromecast
