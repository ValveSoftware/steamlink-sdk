// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_VIDEO_DECODER_JOB_H_
#define MEDIA_BASE_ANDROID_VIDEO_DECODER_JOB_H_

#include <jni.h>
#include <stddef.h>

#include "base/macros.h"
#include "media/base/android/media_decoder_job.h"

namespace media {

class VideoCodecBridge;

// Class for managing video decoding jobs.
class VideoDecoderJob : public MediaDecoderJob {
 public:
  // Create a new VideoDecoderJob instance.
  // |request_data_cb| - Callback used to request more data for the decoder.
  // |on_demuxer_config_changed_cb| - Callback used to inform the caller that
  // demuxer config has changed.
  VideoDecoderJob(const base::Closure& request_data_cb,
                  const base::Closure& on_demuxer_config_changed_cb);
  ~VideoDecoderJob() override;

  // Passes a java surface object to the codec. Returns true if the surface
  // can be used by the decoder, or false otherwise.
  bool SetVideoSurface(gl::ScopedJavaSurface surface);

  // MediaDecoderJob implementation.
  bool HasStream() const override;
  void ReleaseDecoderResources() override;
  void SetDemuxerConfigs(const DemuxerConfigs& configs) override;

  int output_width() const { return output_width_; }
  int output_height() const { return output_height_; }

 private:
  // MediaDecoderJob implementation.
  void ReleaseOutputBuffer(int output_buffer_index,
                           size_t offset,
                           size_t size,
                           bool render_output,
                           bool is_late_frame,
                           base::TimeDelta current_presentation_timestamp,
                           MediaCodecStatus status,
                           const DecoderCallback& callback) override;
  bool ComputeTimeToRender() const override;
  bool IsCodecReconfigureNeeded(const DemuxerConfigs& configs) const override;
  bool AreDemuxerConfigsChanged(const DemuxerConfigs& configs) const override;
  MediaDecoderJobStatus CreateMediaCodecBridgeInternal() override;
  bool UpdateOutputFormat() override;

  // Returns true if a protected surface is required for video playback.
  bool IsProtectedSurfaceRequired();

  // Video configs from the demuxer.
  VideoCodec video_codec_;
  int config_width_;
  int config_height_;

  // Video output format.
  int output_width_;
  int output_height_;

  // The surface object currently owned by the player.
  gl::ScopedJavaSurface surface_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderJob);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_VIDEO_DECODER_JOB_H_
