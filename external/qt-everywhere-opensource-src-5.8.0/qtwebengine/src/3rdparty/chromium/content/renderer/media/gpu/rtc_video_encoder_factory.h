// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_GPU_RTC_VIDEO_ENCODER_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_GPU_RTC_VIDEO_ENCODER_FACTORY_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/webrtc/media/engine/webrtcvideoencoderfactory.h"

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace content {

// This class creates RTCVideoEncoder instances (each wrapping a
// media::VideoEncodeAccelerator) on behalf of the WebRTC stack.
class CONTENT_EXPORT RTCVideoEncoderFactory
    : NON_EXPORTED_BASE(public cricket::WebRtcVideoEncoderFactory) {
 public:
  explicit RTCVideoEncoderFactory(
      media::GpuVideoAcceleratorFactories* gpu_factories);
  ~RTCVideoEncoderFactory() override;

  // cricket::WebRtcVideoEncoderFactory implementation.
  webrtc::VideoEncoder* CreateVideoEncoder(
      webrtc::VideoCodecType type) override;
  const std::vector<VideoCodec>& codecs() const override;
  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override;

 private:
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  // List of supported cricket::WebRtcVideoEncoderFactory::VideoCodec.
  std::vector<VideoCodec> codecs_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoEncoderFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_GPU_RTC_VIDEO_ENCODER_FACTORY_H_
