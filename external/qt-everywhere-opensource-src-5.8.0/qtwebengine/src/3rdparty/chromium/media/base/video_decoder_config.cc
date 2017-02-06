// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_decoder_config.h"

#include <vector>

#include "base/logging.h"
#include "media/base/video_frame.h"

namespace media {

VideoCodec VideoCodecProfileToVideoCodec(VideoCodecProfile profile) {
  switch (profile) {
    case VIDEO_CODEC_PROFILE_UNKNOWN:
      return kUnknownVideoCodec;
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_EXTENDED:
    case H264PROFILE_HIGH:
    case H264PROFILE_HIGH10PROFILE:
    case H264PROFILE_HIGH422PROFILE:
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
    case H264PROFILE_SCALABLEBASELINE:
    case H264PROFILE_SCALABLEHIGH:
    case H264PROFILE_STEREOHIGH:
    case H264PROFILE_MULTIVIEWHIGH:
      return kCodecH264;
    case HEVCPROFILE_MAIN:
    case HEVCPROFILE_MAIN10:
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return kCodecHEVC;
    case VP8PROFILE_ANY:
      return kCodecVP8;
    case VP9PROFILE_PROFILE0:
    case VP9PROFILE_PROFILE1:
    case VP9PROFILE_PROFILE2:
    case VP9PROFILE_PROFILE3:
      return kCodecVP9;
  }
  NOTREACHED();
  return kUnknownVideoCodec;
}

VideoDecoderConfig::VideoDecoderConfig()
    : codec_(kUnknownVideoCodec),
      profile_(VIDEO_CODEC_PROFILE_UNKNOWN),
      format_(PIXEL_FORMAT_UNKNOWN) {}

VideoDecoderConfig::VideoDecoderConfig(
    VideoCodec codec,
    VideoCodecProfile profile,
    VideoPixelFormat format,
    ColorSpace color_space,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    const std::vector<uint8_t>& extra_data,
    const EncryptionScheme& encryption_scheme) {
  Initialize(codec, profile, format, color_space, coded_size, visible_rect,
             natural_size, extra_data, encryption_scheme);
}

VideoDecoderConfig::VideoDecoderConfig(const VideoDecoderConfig& other) =
    default;

VideoDecoderConfig::~VideoDecoderConfig() {}

void VideoDecoderConfig::Initialize(VideoCodec codec,
                                    VideoCodecProfile profile,
                                    VideoPixelFormat format,
                                    ColorSpace color_space,
                                    const gfx::Size& coded_size,
                                    const gfx::Rect& visible_rect,
                                    const gfx::Size& natural_size,
                                    const std::vector<uint8_t>& extra_data,
                                    const EncryptionScheme& encryption_scheme) {
  codec_ = codec;
  profile_ = profile;
  format_ = format;
  color_space_ = color_space;
  coded_size_ = coded_size;
  visible_rect_ = visible_rect;
  natural_size_ = natural_size;
  extra_data_ = extra_data;
  encryption_scheme_ = encryption_scheme;
}

bool VideoDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownVideoCodec &&
      natural_size_.width() > 0 &&
      natural_size_.height() > 0 &&
      VideoFrame::IsValidConfig(format_, VideoFrame::STORAGE_UNOWNED_MEMORY,
                                coded_size_, visible_rect_, natural_size_);
}

bool VideoDecoderConfig::Matches(const VideoDecoderConfig& config) const {
  return ((codec() == config.codec()) && (format() == config.format()) &&
          (profile() == config.profile()) &&
          (coded_size() == config.coded_size()) &&
          (visible_rect() == config.visible_rect()) &&
          (natural_size() == config.natural_size()) &&
          (extra_data() == config.extra_data()) &&
          (encryption_scheme().Matches(config.encryption_scheme())));
}

std::string VideoDecoderConfig::AsHumanReadableString() const {
  std::ostringstream s;
  s << "codec: " << GetCodecName(codec()) << " format: " << format()
    << " profile: " << GetProfileName(profile()) << " coded size: ["
    << coded_size().width() << "," << coded_size().height() << "]"
    << " visible rect: [" << visible_rect().x() << "," << visible_rect().y()
    << "," << visible_rect().width() << "," << visible_rect().height() << "]"
    << " natural size: [" << natural_size().width() << ","
    << natural_size().height() << "]"
    << " has extra data? " << (extra_data().empty() ? "false" : "true")
    << " encrypted? " << (is_encrypted() ? "true" : "false");
  return s.str();
}

}  // namespace media
