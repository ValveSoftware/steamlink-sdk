// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_VIDEO_SENDER_VIDEO_ENCODER_H_
#define MEDIA_CAST_VIDEO_SENDER_VIDEO_ENCODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"

namespace media {
namespace cast {

// All these functions are called from the main cast thread.
class VideoEncoder {
 public:
  typedef base::Callback<void(scoped_ptr<transport::EncodedFrame>)>
      FrameEncodedCallback;

  virtual ~VideoEncoder() {}

  // The video_frame must be valid until the closure callback is called.
  // The closure callback is called from the video encoder thread as soon as
  // the encoder is done with the frame; it does not mean that the encoded frame
  // has been sent out.
  // Once the encoded frame is ready the frame_encoded_callback is called.
  virtual bool EncodeVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& capture_time,
      const FrameEncodedCallback& frame_encoded_callback) = 0;

  // Inform the encoder about the new target bit rate.
  virtual void SetBitRate(int new_bit_rate) = 0;

  // Inform the encoder to encode the next frame as a key frame.
  virtual void GenerateKeyFrame() = 0;

  // Inform the encoder to only reference frames older or equal to frame_id;
  virtual void LatestFrameIdToReference(uint32 frame_id) = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_SENDER_VIDEO_ENCODER_H_
