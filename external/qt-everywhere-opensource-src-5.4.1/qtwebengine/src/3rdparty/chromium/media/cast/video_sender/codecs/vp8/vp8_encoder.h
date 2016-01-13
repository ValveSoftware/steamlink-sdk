// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_VIDEO_SENDER_CODECS_VP8_VP8_ENCODER_H_
#define MEDIA_CAST_VIDEO_SENDER_CODECS_VP8_VP8_ENCODER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/video_sender/software_video_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"

namespace media {
class VideoFrame;
}

// VPX forward declaration.
typedef struct vpx_codec_ctx vpx_enc_ctx_t;

namespace media {
namespace cast {

const int kNumberOfVp8VideoBuffers = 3;

class Vp8Encoder : public SoftwareVideoEncoder {
 public:
  Vp8Encoder(const VideoSenderConfig& video_config, int max_unacked_frames);

  virtual ~Vp8Encoder();

  // Initialize the encoder before Encode() can be called. This method
  // must be called on the thread that Encode() is called.
  virtual void Initialize() OVERRIDE;

  // Encode a raw image (as a part of a video stream).
  virtual bool Encode(const scoped_refptr<media::VideoFrame>& video_frame,
                      transport::EncodedFrame* encoded_image) OVERRIDE;

  // Update the encoder with a new target bit rate.
  virtual void UpdateRates(uint32 new_bitrate) OVERRIDE;

  // Set the next frame to be a key frame.
  virtual void GenerateKeyFrame() OVERRIDE;

  virtual void LatestFrameIdToReference(uint32 frame_id) OVERRIDE;

 private:
  enum Vp8Buffers {
    kAltRefBuffer = 0,
    kGoldenBuffer = 1,
    kLastBuffer = 2,
    kNoBuffer = 3  // Note: must be last.
  };

  void InitEncode(int number_of_cores);

  // Calculate the max target in % for a keyframe.
  uint32 MaxIntraTarget(uint32 optimal_buffer_size) const;

  // Calculate which next Vp8 buffers to update with the next frame.
  Vp8Buffers GetNextBufferToUpdate();

  // Calculate which previous frame to reference.
  uint32 GetLatestFrameIdToReference();

  // Get encoder flags for our referenced encoder buffers.
  void GetCodecReferenceFlags(vpx_codec_flags_t* flags);

  // Get encoder flags for our encoder buffers to update with next frame.
  void GetCodecUpdateFlags(Vp8Buffers buffer_to_update,
                           vpx_codec_flags_t* flags);

  const VideoSenderConfig cast_config_;
  const bool use_multiple_video_buffers_;
  const int max_number_of_repeated_buffers_in_a_row_;

  // VP8 internal objects.
  scoped_ptr<vpx_codec_enc_cfg_t> config_;
  scoped_ptr<vpx_enc_ctx_t> encoder_;
  vpx_image_t* raw_image_;

  bool key_frame_requested_;
  bool first_frame_received_;
  base::TimeDelta first_frame_timestamp_;
  uint32 last_encoded_frame_id_;
  uint32 used_buffers_frame_id_[kNumberOfVp8VideoBuffers];
  bool acked_frame_buffers_[kNumberOfVp8VideoBuffers];
  Vp8Buffers last_used_vp8_buffer_;
  int number_of_repeated_buffers_;

  // This is bound to the thread where Initialize() is called.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(Vp8Encoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_SENDER_CODECS_VP8_VP8_ENCODER_H_
