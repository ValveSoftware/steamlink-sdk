// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_encoder_factory.h"

#include "content/common/gpu/client/gpu_video_encode_accelerator_host.h"
#include "content/renderer/media/rtc_video_encoder.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator.h"

namespace content {

namespace {

// Translate from media::VideoEncodeAccelerator::SupportedProfile to
// cricket::WebRtcVideoEncoderFactory::VideoCodec
cricket::WebRtcVideoEncoderFactory::VideoCodec VEAToWebRTCCodec(
    const media::VideoEncodeAccelerator::SupportedProfile& profile) {
  webrtc::VideoCodecType type = webrtc::kVideoCodecUnknown;
  std::string name;
  int width = 0, height = 0, fps = 0;

  if (profile.profile >= media::VP8PROFILE_MIN &&
      profile.profile <= media::VP8PROFILE_MAX) {
    type = webrtc::kVideoCodecVP8;
    name = "VP8";
  } else if (profile.profile >= media::H264PROFILE_MIN &&
             profile.profile <= media::H264PROFILE_MAX) {
    type = webrtc::kVideoCodecGeneric;
    name = "CAST1";
  }

  if (type != webrtc::kVideoCodecUnknown) {
    width = profile.max_resolution.width();
    height = profile.max_resolution.height();
    fps = profile.max_framerate.numerator;
    DCHECK_EQ(profile.max_framerate.denominator, 1U);
  }

  return cricket::WebRtcVideoEncoderFactory::VideoCodec(
      type, name, width, height, fps);
}

// Translate from cricket::WebRtcVideoEncoderFactory::VideoCodec to
// media::VideoCodecProfile.  Pick a default profile for each codec type.
media::VideoCodecProfile WebRTCCodecToVideoCodecProfile(
    webrtc::VideoCodecType type) {
  switch (type) {
    case webrtc::kVideoCodecVP8:
      return media::VP8PROFILE_MAIN;
    case webrtc::kVideoCodecGeneric:
      return media::H264PROFILE_MAIN;
    default:
      return media::VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

}  // anonymous namespace

RTCVideoEncoderFactory::RTCVideoEncoderFactory(
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories)
    : gpu_factories_(gpu_factories) {
  // Query media::VideoEncodeAccelerator (statically) for our supported codecs.
  std::vector<media::VideoEncodeAccelerator::SupportedProfile> profiles =
      GpuVideoEncodeAcceleratorHost::GetSupportedProfiles();
  for (size_t i = 0; i < profiles.size(); ++i) {
    VideoCodec codec = VEAToWebRTCCodec(profiles[i]);
    if (codec.type != webrtc::kVideoCodecUnknown)
      codecs_.push_back(codec);
  }
}

RTCVideoEncoderFactory::~RTCVideoEncoderFactory() {}

webrtc::VideoEncoder* RTCVideoEncoderFactory::CreateVideoEncoder(
    webrtc::VideoCodecType type) {
  bool found = false;
  for (size_t i = 0; i < codecs_.size(); ++i) {
    if (codecs_[i].type == type) {
      found = true;
      break;
    }
  }
  if (!found)
    return NULL;
  return new RTCVideoEncoder(
      type, WebRTCCodecToVideoCodecProfile(type), gpu_factories_);
}

void RTCVideoEncoderFactory::AddObserver(Observer* observer) {
  // No-op: our codec list is populated on installation.
}

void RTCVideoEncoderFactory::RemoveObserver(Observer* observer) {}

const std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>&
RTCVideoEncoderFactory::codecs() const {
  return codecs_;
}

void RTCVideoEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  delete encoder;
}

}  // namespace content
