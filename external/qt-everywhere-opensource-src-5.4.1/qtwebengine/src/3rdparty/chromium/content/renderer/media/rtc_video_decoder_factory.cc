// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_decoder_factory.h"

#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "media/filters/gpu_video_accelerator_factories.h"

namespace content {

RTCVideoDecoderFactory::RTCVideoDecoderFactory(
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories)
    : gpu_factories_(gpu_factories) {
  DVLOG(2) << "RTCVideoDecoderFactory";
}

RTCVideoDecoderFactory::~RTCVideoDecoderFactory() {
  DVLOG(2) << "~RTCVideoDecoderFactory";
}

webrtc::VideoDecoder* RTCVideoDecoderFactory::CreateVideoDecoder(
    webrtc::VideoCodecType type) {
  DVLOG(2) << "CreateVideoDecoder";
  scoped_ptr<RTCVideoDecoder> decoder =
      RTCVideoDecoder::Create(type, gpu_factories_);
  return decoder.release();
}

void RTCVideoDecoderFactory::DestroyVideoDecoder(
    webrtc::VideoDecoder* decoder) {
  DVLOG(2) << "DestroyVideoDecoder";
  gpu_factories_->GetTaskRunner()->DeleteSoon(FROM_HERE, decoder);
}

}  // namespace content
