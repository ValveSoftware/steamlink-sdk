// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_VIDEO_DECODER_JOB_H_
#define MEDIA_BASE_ANDROID_VIDEO_DECODER_JOB_H_

#include <jni.h>

#include "media/base/android/media_decoder_job.h"

namespace media {

class VideoCodecBridge;

// Class for managing video decoding jobs.
class VideoDecoderJob : public MediaDecoderJob {
 public:
  // Create a new VideoDecoderJob instance.
  // |request_data_cb| - Callback used to request more data for the decoder.
  // |request_resources_cb| - Callback used to request resources.
  // |release_resources_cb| - Callback used to release resources.
  // |on_demuxer_config_changed_cb| - Callback used to inform the caller that
  // demuxer config has changed.
  VideoDecoderJob(
      const base::Closure& request_data_cb,
      const base::Closure& request_resources_cb,
      const base::Closure& release_resources_cb,
      const base::Closure& on_demuxer_config_changed_cb);
  virtual ~VideoDecoderJob();

  // Passes a java surface object to the codec. Returns true if the surface
  // can be used by the decoder, or false otherwise.
  bool SetVideoSurface(gfx::ScopedJavaSurface surface);

  // MediaDecoderJob implementation.
  virtual bool HasStream() const OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void ReleaseDecoderResources() OVERRIDE;

  bool next_video_data_is_iframe() {
    return next_video_data_is_iframe_;
  }

  int width() const { return width_; }
  int height() const { return height_; }

 private:
  // MediaDecoderJob implementation.
  virtual void ReleaseOutputBuffer(
      int output_buffer_index,
      size_t size,
      bool render_output,
      base::TimeDelta current_presentation_timestamp,
      const ReleaseOutputCompletionCallback& callback) OVERRIDE;
  virtual bool ComputeTimeToRender() const OVERRIDE;
  virtual void UpdateDemuxerConfigs(const DemuxerConfigs& configs) OVERRIDE;
  virtual bool IsCodecReconfigureNeeded(
      const DemuxerConfigs& configs) const OVERRIDE;
  virtual bool AreDemuxerConfigsChanged(
      const DemuxerConfigs& configs) const OVERRIDE;
  virtual bool CreateMediaCodecBridgeInternal() OVERRIDE;
  virtual void CurrentDataConsumed(bool is_config_change) OVERRIDE;
  virtual void OnMediaCodecBridgeReleased() OVERRIDE;

  // Returns true if a protected surface is required for video playback.
  bool IsProtectedSurfaceRequired();

  // Video configs from the demuxer.
  VideoCodec video_codec_;
  int width_;
  int height_;

  // The surface object currently owned by the player.
  gfx::ScopedJavaSurface surface_;

  // Callbacks to inform the caller about decoder resources change.
  base::Closure request_resources_cb_;
  base::Closure release_resources_cb_;

  // Track whether the next access unit is an I-frame. The first access
  // unit after Flush() and CurrentDataConsumed(true) is guaranteed to be an
  // I-frame.
  bool next_video_data_is_iframe_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderJob);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_VIDEO_DECODER_JOB_H_
