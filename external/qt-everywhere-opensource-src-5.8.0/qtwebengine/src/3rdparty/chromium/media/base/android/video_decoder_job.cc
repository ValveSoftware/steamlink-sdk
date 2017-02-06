// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/video_decoder_job.h"

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/sdk_media_codec_bridge.h"

namespace media {

class VideoDecoderThread : public base::Thread {
 public:
  VideoDecoderThread() : base::Thread("MediaSource_VideoDecoderThread") {
    Start();
  }
};

// TODO(qinmin): Check if it is tolerable to use worker pool to handle all the
// decoding tasks so that we don't need a global thread here.
// http://crbug.com/245750
base::LazyInstance<VideoDecoderThread>::Leaky
    g_video_decoder_thread = LAZY_INSTANCE_INITIALIZER;

VideoDecoderJob::VideoDecoderJob(
    const base::Closure& request_data_cb,
    const base::Closure& on_demuxer_config_changed_cb)
    : MediaDecoderJob(g_video_decoder_thread.Pointer()->task_runner(),
                      request_data_cb,
                      on_demuxer_config_changed_cb),
      video_codec_(kUnknownVideoCodec),
      config_width_(0),
      config_height_(0),
      output_width_(0),
      output_height_(0) {
}

VideoDecoderJob::~VideoDecoderJob() {}

bool VideoDecoderJob::SetVideoSurface(gl::ScopedJavaSurface surface) {
  // For an empty surface, always pass it to the |media_codec_bridge_| job so
  // that it can detach from the current one. Otherwise, don't pass an
  // unprotected surface if the video content requires a protected one.
  if (!surface.IsEmpty() && IsProtectedSurfaceRequired() &&
      !surface.is_protected()) {
    return false;
  }

  surface_ = std::move(surface);
  need_to_reconfig_decoder_job_ = true;
  return true;
}

bool VideoDecoderJob::HasStream() const {
  return video_codec_ != kUnknownVideoCodec;
}

void VideoDecoderJob::ReleaseDecoderResources() {
  MediaDecoderJob::ReleaseDecoderResources();
  surface_ = gl::ScopedJavaSurface();
}

void VideoDecoderJob::SetDemuxerConfigs(const DemuxerConfigs& configs) {
  video_codec_ = configs.video_codec;
  config_width_ = configs.video_size.width();
  config_height_ = configs.video_size.height();
  set_is_content_encrypted(configs.is_video_encrypted);
  if (!media_codec_bridge_) {
    output_width_ = config_width_;
    output_height_ = config_height_;
  }
}

void VideoDecoderJob::ReleaseOutputBuffer(
    int output_buffer_index,
    size_t offset,
    size_t size,
    bool render_output,
    bool is_late_frame,
    base::TimeDelta current_presentation_timestamp,
    MediaCodecStatus status,
    const DecoderCallback& callback) {
  media_codec_bridge_->ReleaseOutputBuffer(output_buffer_index, render_output);
  callback.Run(status, is_late_frame, current_presentation_timestamp,
               current_presentation_timestamp);
}

bool VideoDecoderJob::ComputeTimeToRender() const {
  return true;
}

bool VideoDecoderJob::IsCodecReconfigureNeeded(
    const DemuxerConfigs& configs) const {
  if (!media_codec_bridge_)
    return true;

  if (!AreDemuxerConfigsChanged(configs))
    return false;

  bool only_size_changed = false;
  if (video_codec_ == configs.video_codec &&
      is_content_encrypted() == configs.is_video_encrypted) {
    only_size_changed = true;
  }

  return !only_size_changed ||
      !static_cast<VideoCodecBridge*>(media_codec_bridge_.get())->
          IsAdaptivePlaybackSupported(configs.video_size.width(),
                                      configs.video_size.height());
}

bool VideoDecoderJob::AreDemuxerConfigsChanged(
    const DemuxerConfigs& configs) const {
  return video_codec_ != configs.video_codec ||
      is_content_encrypted() != configs.is_video_encrypted ||
      config_width_ != configs.video_size.width() ||
      config_height_ != configs.video_size.height();
}

MediaDecoderJob::MediaDecoderJobStatus
    VideoDecoderJob::CreateMediaCodecBridgeInternal() {
  if (surface_.IsEmpty()) {
    ReleaseMediaCodecBridge();
    return STATUS_FAILURE;
  }

  // If we cannot find a key frame in cache, browser seek is needed.
  bool next_video_data_is_iframe = SetCurrentFrameToPreviouslyCachedKeyFrame();
  if (!next_video_data_is_iframe)
    return STATUS_KEY_FRAME_REQUIRED;

  bool is_secure = is_content_encrypted() && drm_bridge() &&
      drm_bridge()->IsProtectedSurfaceRequired();

  media_codec_bridge_.reset(VideoCodecBridge::CreateDecoder(
      video_codec_, is_secure, gfx::Size(config_width_, config_height_),
      surface_.j_surface().obj(), GetMediaCrypto()));

  if (!media_codec_bridge_)
    return STATUS_FAILURE;

  return STATUS_SUCCESS;
}

bool VideoDecoderJob::UpdateOutputFormat() {
  if (!media_codec_bridge_)
    return false;
  int prev_output_width = output_width_;
  int prev_output_height = output_height_;
  // See b/18224769. The values reported from MediaCodecBridge::GetOutputSize
  // correspond to the actual video frame size, but this is not necessarily the
  // size that should be output.
  output_width_ = config_width_;
  output_height_ = config_height_;
  return (output_width_ != prev_output_width) ||
      (output_height_ != prev_output_height);
}

bool VideoDecoderJob::IsProtectedSurfaceRequired() {
  return is_content_encrypted() && drm_bridge() &&
      drm_bridge()->IsProtectedSurfaceRequired();
}

}  // namespace media
