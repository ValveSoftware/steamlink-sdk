// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_VIDEO_SENDER_VIDEO_ENCODER_IMPL_H_
#define MEDIA_CAST_VIDEO_SENDER_VIDEO_ENCODER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/video_sender/software_video_encoder.h"
#include "media/cast/video_sender/video_encoder.h"

namespace media {
class VideoFrame;

namespace cast {

// This object is called external from the main cast thread and internally from
// the video encoder thread.
class VideoEncoderImpl : public VideoEncoder {
 public:
  struct CodecDynamicConfig {
    bool key_frame_requested;
    uint32 latest_frame_id_to_reference;
    int bit_rate;
  };

  typedef base::Callback<void(scoped_ptr<transport::EncodedFrame>)>
      FrameEncodedCallback;

  VideoEncoderImpl(scoped_refptr<CastEnvironment> cast_environment,
                   const VideoSenderConfig& video_config,
                   int max_unacked_frames);

  virtual ~VideoEncoderImpl();

  // Called from the main cast thread. This function post the encode task to the
  // video encoder thread;
  // The video_frame must be valid until the closure callback is called.
  // The closure callback is called from the video encoder thread as soon as
  // the encoder is done with the frame; it does not mean that the encoded frame
  // has been sent out.
  // Once the encoded frame is ready the frame_encoded_callback is called.
  virtual bool EncodeVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& capture_time,
      const FrameEncodedCallback& frame_encoded_callback) OVERRIDE;

  // The following functions are called from the main cast thread.
  virtual void SetBitRate(int new_bit_rate) OVERRIDE;
  virtual void GenerateKeyFrame() OVERRIDE;
  virtual void LatestFrameIdToReference(uint32 frame_id) OVERRIDE;

 private:
  const VideoSenderConfig video_config_;
  scoped_refptr<CastEnvironment> cast_environment_;
  CodecDynamicConfig dynamic_config_;

  // This member belongs to the video encoder thread. It must not be
  // dereferenced on the main thread. We manage the lifetime of this member
  // manually because it needs to be initialize, used and destroyed on the
  // video encoder thread and video encoder thread can out-live the main thread.
  scoped_ptr<SoftwareVideoEncoder> encoder_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_SENDER_VIDEO_ENCODER_IMPL_H_
