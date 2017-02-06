// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/gpu/rtc_video_decoder_factory.h"

#include <memory>

#include "content/renderer/media/gpu/rtc_video_decoder.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

namespace content {

RTCVideoDecoderFactory::RTCVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories) {
  DVLOG(2) << __FUNCTION__;
}

RTCVideoDecoderFactory::~RTCVideoDecoderFactory() {
  DVLOG(2) << __FUNCTION__;
}

webrtc::VideoDecoder* RTCVideoDecoderFactory::CreateVideoDecoder(
    webrtc::VideoCodecType type) {
  DVLOG(2) << __FUNCTION__;
  return RTCVideoDecoder::Create(type, gpu_factories_).release();
}

void RTCVideoDecoderFactory::DestroyVideoDecoder(
    webrtc::VideoDecoder* decoder) {
  DVLOG(2) << __FUNCTION__;
  RTCVideoDecoder::Destroy(decoder, gpu_factories_);
}

}  // namespace content
