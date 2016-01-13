// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_ENCODER_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_ENCODER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "media/base/video_decoder_config.h"
#include "third_party/webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "ui/gfx/size.h"

namespace base {

class MessageLoopProxy;

}  // namespace base

namespace media {

class GpuVideoAcceleratorFactories;

}  // namespace media

namespace content {

// RTCVideoEncoder uses a media::VideoEncodeAccelerator to implement a
// webrtc::VideoEncoder class for WebRTC.  Internally, VEA methods are
// trampolined to a private RTCVideoEncoder::Impl instance.  The Impl class runs
// on the worker thread queried from the |gpu_factories_|, which is presently
// the media thread.  RTCVideoEncoder itself is run and destroyed on the thread
// it is constructed on, which is presently the libjingle worker thread.
// Callbacks from the Impl due to its VEA::Client notifications are also posted
// back to RTCVideoEncoder on this thread.
class CONTENT_EXPORT RTCVideoEncoder
    : NON_EXPORTED_BASE(public webrtc::VideoEncoder) {
 public:
  RTCVideoEncoder(
      webrtc::VideoCodecType type,
      media::VideoCodecProfile profile,
      const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories);
  virtual ~RTCVideoEncoder();

  // webrtc::VideoEncoder implementation.  Tasks are posted to |impl_| using the
  // appropriate VEA methods.
  virtual int32_t InitEncode(const webrtc::VideoCodec* codec_settings,
                             int32_t number_of_cores,
                             uint32_t max_payload_size) OVERRIDE;
  virtual int32_t Encode(
      const webrtc::I420VideoFrame& input_image,
      const webrtc::CodecSpecificInfo* codec_specific_info,
      const std::vector<webrtc::VideoFrameType>* frame_types) OVERRIDE;
  virtual int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) OVERRIDE;
  virtual int32_t Release() OVERRIDE;
  virtual int32_t SetChannelParameters(uint32_t packet_loss, int rtt) OVERRIDE;
  virtual int32_t SetRates(uint32_t new_bit_rate, uint32_t frame_rate) OVERRIDE;

 private:
  class Impl;
  friend class RTCVideoEncoder::Impl;

  // Return an encoded output buffer to WebRTC.
  void ReturnEncodedImage(scoped_ptr<webrtc::EncodedImage> image,
                          int32 bitstream_buffer_id,
                          uint16 picture_id);

  void NotifyError(int32_t error);

  void RecordInitEncodeUMA(int32_t init_retval);

  base::ThreadChecker thread_checker_;

  // The video codec type, as reported to WebRTC.
  const webrtc::VideoCodecType video_codec_type_;

  // The video codec profile, to configure the encoder to encode to.
  const media::VideoCodecProfile video_codec_profile_;

  // Factory for creating VEAs, shared memory buffers, etc.
  scoped_refptr<media::GpuVideoAcceleratorFactories> gpu_factories_;

  // webrtc::VideoEncoder encode complete callback.
  webrtc::EncodedImageCallback* encoded_image_callback_;

  // The RTCVideoEncoder::Impl that does all the work.
  scoped_refptr<Impl> impl_;

  // We cannot immediately return error conditions to the WebRTC user of this
  // class, as there is no error callback in the webrtc::VideoEncoder interface.
  // Instead, we cache an error status here and return it the next time an
  // interface entry point is called.
  int32_t impl_status_;

  // Weak pointer factory for posting back VEA::Client notifications to
  // RTCVideoEncoder.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<RTCVideoEncoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoEncoder);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_ENCODER_H_
