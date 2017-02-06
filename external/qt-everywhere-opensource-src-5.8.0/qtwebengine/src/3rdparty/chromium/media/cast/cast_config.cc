// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_config.h"

namespace {

const float kDefaultCongestionControlBackOff = 0.875f;

enum {
  // Minimum and Maximum VP8 quantizer in default configuration.
  kDefaultMaxQp = 63,
  kDefaultMinQp = 4,

  kDefaultMaxCpuSaverQp = 25,

  // Number of video buffers in default configuration (applies only to certain
  // external codecs).
  kDefaultNumberOfVideoBuffers = 1,
};

}  // namespace

namespace media {
namespace cast {

// TODO(miu): Revisit code factoring of these structs.  There are a number of
// common elements between them all, so it might be reasonable to only have one
// or two structs; or, at least a common base class.

// TODO(miu): Make sure all POD members are initialized by ctors.  Policy
// decision: Reasonable defaults or use invalid placeholder values to expose
// unset members?

// TODO(miu): Provide IsValidConfig() functions?

// TODO(miu): Throughout the code, there is a lot of copy-and-paste of the same
// calculations based on these config values.  So, why don't we add methods to
// these classes to centralize the logic?

VideoSenderConfig::VideoSenderConfig()
    : ssrc(0),
      receiver_ssrc(0),
      max_playout_delay(
          base::TimeDelta::FromMilliseconds(kDefaultRtpMaxDelayMs)),
      rtp_payload_type(0),
      use_external_encoder(false),
      congestion_control_back_off(kDefaultCongestionControlBackOff),
      max_bitrate(kDefaultMaxVideoKbps * 1000),
      min_bitrate(kDefaultMinVideoKbps * 1000),
      start_bitrate(kDefaultMaxVideoKbps * 1000),
      max_qp(kDefaultMaxQp),
      min_qp(kDefaultMinQp),
      max_cpu_saver_qp(kDefaultMaxCpuSaverQp),
      max_frame_rate(kDefaultMaxFrameRate),
      max_number_of_video_buffers_used(kDefaultNumberOfVideoBuffers),
      codec(CODEC_VIDEO_VP8),
      number_of_encode_threads(1) {}

VideoSenderConfig::VideoSenderConfig(const VideoSenderConfig& other) = default;

VideoSenderConfig::~VideoSenderConfig() {}

AudioSenderConfig::AudioSenderConfig()
    : ssrc(0),
      receiver_ssrc(0),
      max_playout_delay(
          base::TimeDelta::FromMilliseconds(kDefaultRtpMaxDelayMs)),
      rtp_payload_type(0),
      use_external_encoder(false),
      frequency(0),
      channels(0),
      bitrate(kDefaultAudioEncoderBitrate),
      codec(CODEC_AUDIO_OPUS) {}

AudioSenderConfig::AudioSenderConfig(const AudioSenderConfig& other) = default;

AudioSenderConfig::~AudioSenderConfig() {}

FrameReceiverConfig::FrameReceiverConfig()
    : receiver_ssrc(0),
      sender_ssrc(0),
      rtp_max_delay_ms(kDefaultRtpMaxDelayMs),
      rtp_payload_type(0),
      rtp_timebase(0),
      channels(0),
      target_frame_rate(0),
      codec(CODEC_UNKNOWN) {}

FrameReceiverConfig::FrameReceiverConfig(const FrameReceiverConfig& other) =
    default;

FrameReceiverConfig::~FrameReceiverConfig() {}

}  // namespace cast
}  // namespace media
