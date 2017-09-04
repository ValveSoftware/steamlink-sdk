// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/gpu/rtc_video_encoder_factory.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/feature_h264_with_openh264_ffmpeg.h"
#include "content/renderer/media/gpu/rtc_video_encoder.h"
#include "media/gpu/ipc/client/gpu_video_encode_accelerator_host.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator.h"

namespace content {

namespace {
bool IsCodecDisabledByCommandLine(const base::CommandLine* cmd_line,
                                  const std::string codec_name) {
  if (!cmd_line->HasSwitch(switches::kDisableWebRtcHWEncoding))
    return false;

  const std::string codec_filter =
      cmd_line->GetSwitchValueASCII(switches::kDisableWebRtcHWEncoding);
  return codec_filter.empty() || codec_filter == codec_name;
}

// Translate from media::VideoEncodeAccelerator::SupportedProfile to
// one or more instances of cricket::WebRtcVideoEncoderFactory::VideoCodec
void VEAToWebRTCCodecs(
    std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>* codecs,
    const media::VideoEncodeAccelerator::SupportedProfile& profile) {
  const int width = profile.max_resolution.width();
  const int height = profile.max_resolution.height();
  const int fps = profile.max_framerate_numerator;
  DCHECK_EQ(profile.max_framerate_denominator, 1U);

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (profile.profile >= media::VP8PROFILE_MIN &&
      profile.profile <= media::VP8PROFILE_MAX) {
    if (!IsCodecDisabledByCommandLine(cmd_line,
                                      switches::kDisableWebRtcHWEncodingVPx)) {
      codecs->push_back(cricket::WebRtcVideoEncoderFactory::VideoCodec(
          webrtc::kVideoCodecVP8, "VP8", width, height, fps));
    }
  } else if (profile.profile >= media::H264PROFILE_MIN &&
             profile.profile <= media::H264PROFILE_MAX) {
    // Enable H264 HW encode for WebRTC when SW fallback is available, which is
    // checked by kWebRtcH264WithOpenH264FFmpeg flag. This check should be
    // removed when SW implementation is fully enabled.
    bool webrtc_h264_sw_enabled = false;
#if BUILDFLAG(RTC_USE_H264) && !defined(MEDIA_DISABLE_FFMPEG)
    webrtc_h264_sw_enabled =
        base::FeatureList::IsEnabled(kWebRtcH264WithOpenH264FFmpeg);
#endif  // BUILDFLAG(RTC_USE_H264) && !defined(MEDIA_DISABLE_FFMPEG)
    if (webrtc_h264_sw_enabled ||
        !IsCodecDisabledByCommandLine(cmd_line,
                                      switches::kDisableWebRtcHWEncodingH264)) {
      codecs->push_back(cricket::WebRtcVideoEncoderFactory::VideoCodec(
          webrtc::kVideoCodecH264, "H264", width, height, fps));
    }
  }
}

}  // anonymous namespace

RTCVideoEncoderFactory::RTCVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories) {
  const media::VideoEncodeAccelerator::SupportedProfiles& profiles =
      gpu_factories_->GetVideoEncodeAcceleratorSupportedProfiles();
  for (const auto& profile : profiles)
    VEAToWebRTCCodecs(&codecs_, profile);
}

RTCVideoEncoderFactory::~RTCVideoEncoderFactory() {}

webrtc::VideoEncoder* RTCVideoEncoderFactory::CreateVideoEncoder(
    webrtc::VideoCodecType type) {
  for (const auto& codec : codecs_) {
    if (codec.type == type)
      return new RTCVideoEncoder(type, gpu_factories_);
  }
  return nullptr;
}

const std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>&
RTCVideoEncoderFactory::codecs() const {
  return codecs_;
}

void RTCVideoEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  delete encoder;
}

}  // namespace content
