// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/video_decoder_job.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_drm_bridge.h"

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
    const base::Closure& request_resources_cb,
    const base::Closure& release_resources_cb,
    const base::Closure& on_demuxer_config_changed_cb)
    : MediaDecoderJob(g_video_decoder_thread.Pointer()->message_loop_proxy(),
                      request_data_cb,
                      on_demuxer_config_changed_cb),
      video_codec_(kUnknownVideoCodec),
      width_(0),
      height_(0),
      request_resources_cb_(request_resources_cb),
      release_resources_cb_(release_resources_cb),
      next_video_data_is_iframe_(true) {
}

VideoDecoderJob::~VideoDecoderJob() {}

bool VideoDecoderJob::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  // For an empty surface, always pass it to the |media_codec_bridge_| job so
  // that it can detach from the current one. Otherwise, don't pass an
  // unprotected surface if the video content requires a protected one.
  if (!surface.IsEmpty() && IsProtectedSurfaceRequired() &&
      !surface.is_protected()) {
    return false;
  }

  surface_ =  surface.Pass();
  need_to_reconfig_decoder_job_ = true;
  return true;
}

bool VideoDecoderJob::HasStream() const {
  return video_codec_ != kUnknownVideoCodec;
}

void VideoDecoderJob::Flush() {
  MediaDecoderJob::Flush();
  next_video_data_is_iframe_ = true;
}

void VideoDecoderJob::ReleaseDecoderResources() {
  MediaDecoderJob::ReleaseDecoderResources();
  surface_ = gfx::ScopedJavaSurface();
}

void VideoDecoderJob::ReleaseOutputBuffer(
    int output_buffer_index,
    size_t size,
    bool render_output,
    base::TimeDelta current_presentation_timestamp,
    const ReleaseOutputCompletionCallback& callback) {
  media_codec_bridge_->ReleaseOutputBuffer(output_buffer_index, render_output);
  callback.Run(current_presentation_timestamp, current_presentation_timestamp);
}

bool VideoDecoderJob::ComputeTimeToRender() const {
  return true;
}

void VideoDecoderJob::UpdateDemuxerConfigs(const DemuxerConfigs& configs) {
  video_codec_ = configs.video_codec;
  width_ = configs.video_size.width();
  height_ = configs.video_size.height();
  set_is_content_encrypted(configs.is_video_encrypted);
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
      width_ != configs.video_size.width() ||
      height_ != configs.video_size.height();
}

bool VideoDecoderJob::CreateMediaCodecBridgeInternal() {
  if (surface_.IsEmpty()) {
    ReleaseMediaCodecBridge();
    return false;
  }

  // If the next data is not iframe, return false so that the player need to
  // perform a browser seek.
  if (!next_video_data_is_iframe_)
    return false;

  bool is_secure = is_content_encrypted() && drm_bridge() &&
      drm_bridge()->IsProtectedSurfaceRequired();

  media_codec_bridge_.reset(VideoCodecBridge::CreateDecoder(
      video_codec_, is_secure, gfx::Size(width_, height_),
      surface_.j_surface().obj(), GetMediaCrypto().obj()));

  if (!media_codec_bridge_)
    return false;

  request_resources_cb_.Run();
  return true;
}

void VideoDecoderJob::CurrentDataConsumed(bool is_config_change) {
  next_video_data_is_iframe_ = is_config_change;
}

void VideoDecoderJob::OnMediaCodecBridgeReleased() {
  release_resources_cb_.Run();
}

bool VideoDecoderJob::IsProtectedSurfaceRequired() {
  return is_content_encrypted() && drm_bridge() &&
      drm_bridge()->IsProtectedSurfaceRequired();
}

}  // namespace media
