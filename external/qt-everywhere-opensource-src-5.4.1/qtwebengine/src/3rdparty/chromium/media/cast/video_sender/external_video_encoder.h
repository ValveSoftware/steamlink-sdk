// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_VIDEO_SENDER_EXTERNAL_VIDEO_ENCODER_H_
#define MEDIA_CAST_VIDEO_SENDER_EXTERNAL_VIDEO_ENCODER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/video_sender/video_encoder.h"
#include "media/video/video_encode_accelerator.h"

namespace media {
class VideoFrame;
}

namespace media {
namespace cast {

class LocalVideoEncodeAcceleratorClient;

// This object is called external from the main cast thread and internally from
// the video encoder thread.
class ExternalVideoEncoder : public VideoEncoder {
 public:
  ExternalVideoEncoder(
      scoped_refptr<CastEnvironment> cast_environment,
      const VideoSenderConfig& video_config,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
      const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb);

  virtual ~ExternalVideoEncoder();

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

  // Called when a VEA is created.
  void OnCreateVideoEncodeAccelerator(
      const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
      scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner,
      scoped_ptr<media::VideoEncodeAccelerator> vea);

 protected:
  void EncoderInitialized();
  void EncoderError();

 private:
  friend class LocalVideoEncodeAcceleratorClient;

  VideoSenderConfig video_config_;
  scoped_refptr<CastEnvironment> cast_environment_;

  bool encoder_active_;
  bool key_frame_requested_;

  scoped_refptr<LocalVideoEncodeAcceleratorClient> video_accelerator_client_;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner_;

  // Weak pointer factory for posting back LocalVideoEncodeAcceleratorClient
  // notifications to ExternalVideoEncoder.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<ExternalVideoEncoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalVideoEncoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_SENDER_EXTERNAL_VIDEO_ENCODER_H_
