// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/rpc/proto_enum_utils.h"

namespace media {
namespace remoting {

#define CASE_RETURN_OTHER(x) \
  case OriginType::x:        \
    return OtherType::x

base::Optional<::media::EncryptionScheme::CipherMode>
ToMediaEncryptionSchemeCipherMode(pb::EncryptionScheme::CipherMode value) {
  using OriginType = pb::EncryptionScheme;
  using OtherType = ::media::EncryptionScheme;
  switch (value) {
    CASE_RETURN_OTHER(CIPHER_MODE_UNENCRYPTED);
    CASE_RETURN_OTHER(CIPHER_MODE_AES_CTR);
    CASE_RETURN_OTHER(CIPHER_MODE_AES_CBC);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::EncryptionScheme::CipherMode>
ToProtoEncryptionSchemeCipherMode(::media::EncryptionScheme::CipherMode value) {
  using OriginType = ::media::EncryptionScheme;
  using OtherType = pb::EncryptionScheme;
  switch (value) {
    CASE_RETURN_OTHER(CIPHER_MODE_UNENCRYPTED);
    CASE_RETURN_OTHER(CIPHER_MODE_AES_CTR);
    CASE_RETURN_OTHER(CIPHER_MODE_AES_CBC);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::AudioCodec> ToMediaAudioCodec(
    pb::AudioDecoderConfig::Codec value) {
  using OriginType = pb::AudioDecoderConfig;
  using OtherType = ::media::AudioCodec;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownAudioCodec);
    CASE_RETURN_OTHER(kCodecAAC);
    CASE_RETURN_OTHER(kCodecMP3);
    CASE_RETURN_OTHER(kCodecPCM);
    CASE_RETURN_OTHER(kCodecVorbis);
    CASE_RETURN_OTHER(kCodecFLAC);
    CASE_RETURN_OTHER(kCodecAMR_NB);
    CASE_RETURN_OTHER(kCodecAMR_WB);
    CASE_RETURN_OTHER(kCodecPCM_MULAW);
    CASE_RETURN_OTHER(kCodecGSM_MS);
    CASE_RETURN_OTHER(kCodecPCM_S16BE);
    CASE_RETURN_OTHER(kCodecPCM_S24BE);
    CASE_RETURN_OTHER(kCodecOpus);
    CASE_RETURN_OTHER(kCodecEAC3);
    CASE_RETURN_OTHER(kCodecPCM_ALAW);
    CASE_RETURN_OTHER(kCodecALAC);
    CASE_RETURN_OTHER(kCodecAC3);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::AudioDecoderConfig::Codec> ToProtoAudioDecoderConfigCodec(
    ::media::AudioCodec value) {
  using OriginType = ::media::AudioCodec;
  using OtherType = pb::AudioDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownAudioCodec);
    CASE_RETURN_OTHER(kCodecAAC);
    CASE_RETURN_OTHER(kCodecMP3);
    CASE_RETURN_OTHER(kCodecPCM);
    CASE_RETURN_OTHER(kCodecVorbis);
    CASE_RETURN_OTHER(kCodecFLAC);
    CASE_RETURN_OTHER(kCodecAMR_NB);
    CASE_RETURN_OTHER(kCodecAMR_WB);
    CASE_RETURN_OTHER(kCodecPCM_MULAW);
    CASE_RETURN_OTHER(kCodecGSM_MS);
    CASE_RETURN_OTHER(kCodecPCM_S16BE);
    CASE_RETURN_OTHER(kCodecPCM_S24BE);
    CASE_RETURN_OTHER(kCodecOpus);
    CASE_RETURN_OTHER(kCodecEAC3);
    CASE_RETURN_OTHER(kCodecPCM_ALAW);
    CASE_RETURN_OTHER(kCodecALAC);
    CASE_RETURN_OTHER(kCodecAC3);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::SampleFormat> ToMediaSampleFormat(
    pb::AudioDecoderConfig::SampleFormat value) {
  using OriginType = pb::AudioDecoderConfig;
  using OtherType = ::media::SampleFormat;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownSampleFormat);
    CASE_RETURN_OTHER(kSampleFormatU8);
    CASE_RETURN_OTHER(kSampleFormatS16);
    CASE_RETURN_OTHER(kSampleFormatS32);
    CASE_RETURN_OTHER(kSampleFormatF32);
    CASE_RETURN_OTHER(kSampleFormatPlanarS16);
    CASE_RETURN_OTHER(kSampleFormatPlanarF32);
    CASE_RETURN_OTHER(kSampleFormatPlanarS32);
    CASE_RETURN_OTHER(kSampleFormatS24);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::AudioDecoderConfig::SampleFormat>
ToProtoAudioDecoderConfigSampleFormat(::media::SampleFormat value) {
  using OriginType = ::media::SampleFormat;
  using OtherType = pb::AudioDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownSampleFormat);
    CASE_RETURN_OTHER(kSampleFormatU8);
    CASE_RETURN_OTHER(kSampleFormatS16);
    CASE_RETURN_OTHER(kSampleFormatS32);
    CASE_RETURN_OTHER(kSampleFormatF32);
    CASE_RETURN_OTHER(kSampleFormatPlanarS16);
    CASE_RETURN_OTHER(kSampleFormatPlanarF32);
    CASE_RETURN_OTHER(kSampleFormatPlanarS32);
    CASE_RETURN_OTHER(kSampleFormatS24);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::ChannelLayout> ToMediaChannelLayout(
    pb::AudioDecoderConfig::ChannelLayout value) {
  using OriginType = pb::AudioDecoderConfig;
  using OtherType = ::media::ChannelLayout;
  switch (value) {
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_NONE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_UNSUPPORTED);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_MONO);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_SURROUND);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2_2);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_QUAD);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_0_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_1_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1_WIDE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO_DOWNMIX);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2POINT1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_3_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_0_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_HEXAGONAL);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_0_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1_WIDE_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_OCTAGONAL);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_DISCRETE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_1_QUAD_SIDE);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::AudioDecoderConfig::ChannelLayout>
ToProtoAudioDecoderConfigChannelLayout(::media::ChannelLayout value) {
  using OriginType = ::media::ChannelLayout;
  using OtherType = pb::AudioDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_NONE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_UNSUPPORTED);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_MONO);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_SURROUND);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2_2);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_QUAD);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_0_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_1_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1_WIDE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO_DOWNMIX);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2POINT1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_3_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_0_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_HEXAGONAL);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_0_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1_WIDE_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_OCTAGONAL);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_DISCRETE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_1_QUAD_SIDE);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::VideoCodec> ToMediaVideoCodec(
    pb::VideoDecoderConfig::Codec value) {
  using OriginType = pb::VideoDecoderConfig;
  using OtherType = ::media::VideoCodec;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownVideoCodec);
    CASE_RETURN_OTHER(kCodecH264);
    CASE_RETURN_OTHER(kCodecVC1);
    CASE_RETURN_OTHER(kCodecMPEG2);
    CASE_RETURN_OTHER(kCodecMPEG4);
    CASE_RETURN_OTHER(kCodecTheora);
    CASE_RETURN_OTHER(kCodecVP8);
    CASE_RETURN_OTHER(kCodecVP9);
    CASE_RETURN_OTHER(kCodecHEVC);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::VideoDecoderConfig::Codec> ToProtoVideoDecoderConfigCodec(
    ::media::VideoCodec value) {
  using OriginType = ::media::VideoCodec;
  using OtherType = pb::VideoDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownVideoCodec);
    CASE_RETURN_OTHER(kCodecH264);
    CASE_RETURN_OTHER(kCodecVC1);
    CASE_RETURN_OTHER(kCodecMPEG2);
    CASE_RETURN_OTHER(kCodecMPEG4);
    CASE_RETURN_OTHER(kCodecTheora);
    CASE_RETURN_OTHER(kCodecVP8);
    CASE_RETURN_OTHER(kCodecVP9);
    CASE_RETURN_OTHER(kCodecHEVC);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::VideoCodecProfile> ToMediaVideoCodecProfile(
    pb::VideoDecoderConfig::Profile value) {
  using OriginType = pb::VideoDecoderConfig;
  using OtherType = ::media::VideoCodecProfile;
  switch (value) {
    CASE_RETURN_OTHER(VIDEO_CODEC_PROFILE_UNKNOWN);
    CASE_RETURN_OTHER(H264PROFILE_BASELINE);
    CASE_RETURN_OTHER(H264PROFILE_MAIN);
    CASE_RETURN_OTHER(H264PROFILE_EXTENDED);
    CASE_RETURN_OTHER(H264PROFILE_HIGH);
    CASE_RETURN_OTHER(H264PROFILE_HIGH10PROFILE);
    CASE_RETURN_OTHER(H264PROFILE_HIGH422PROFILE);
    CASE_RETURN_OTHER(H264PROFILE_HIGH444PREDICTIVEPROFILE);
    CASE_RETURN_OTHER(H264PROFILE_SCALABLEBASELINE);
    CASE_RETURN_OTHER(H264PROFILE_SCALABLEHIGH);
    CASE_RETURN_OTHER(H264PROFILE_STEREOHIGH);
    CASE_RETURN_OTHER(H264PROFILE_MULTIVIEWHIGH);
    CASE_RETURN_OTHER(VP8PROFILE_ANY);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE0);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE1);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE2);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE3);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN10);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN_STILL_PICTURE);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::VideoDecoderConfig::Profile>
ToProtoVideoDecoderConfigProfile(::media::VideoCodecProfile value) {
  using OriginType = ::media::VideoCodecProfile;
  using OtherType = pb::VideoDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(VIDEO_CODEC_PROFILE_UNKNOWN);
    CASE_RETURN_OTHER(H264PROFILE_BASELINE);
    CASE_RETURN_OTHER(H264PROFILE_MAIN);
    CASE_RETURN_OTHER(H264PROFILE_EXTENDED);
    CASE_RETURN_OTHER(H264PROFILE_HIGH);
    CASE_RETURN_OTHER(H264PROFILE_HIGH10PROFILE);
    CASE_RETURN_OTHER(H264PROFILE_HIGH422PROFILE);
    CASE_RETURN_OTHER(H264PROFILE_HIGH444PREDICTIVEPROFILE);
    CASE_RETURN_OTHER(H264PROFILE_SCALABLEBASELINE);
    CASE_RETURN_OTHER(H264PROFILE_SCALABLEHIGH);
    CASE_RETURN_OTHER(H264PROFILE_STEREOHIGH);
    CASE_RETURN_OTHER(H264PROFILE_MULTIVIEWHIGH);
    CASE_RETURN_OTHER(VP8PROFILE_ANY);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE0);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE1);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE2);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE3);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN10);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN_STILL_PICTURE);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::VideoPixelFormat> ToMediaVideoPixelFormat(
    pb::VideoDecoderConfig::Format value) {
  using OriginType = pb::VideoDecoderConfig;
  using OtherType = ::media::VideoPixelFormat;
  switch (value) {
    CASE_RETURN_OTHER(PIXEL_FORMAT_UNKNOWN);
    CASE_RETURN_OTHER(PIXEL_FORMAT_I420);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YV12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YV16);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YV12A);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YV24);
    CASE_RETURN_OTHER(PIXEL_FORMAT_NV12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_NV21);
    CASE_RETURN_OTHER(PIXEL_FORMAT_UYVY);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUY2);
    CASE_RETURN_OTHER(PIXEL_FORMAT_ARGB);
    CASE_RETURN_OTHER(PIXEL_FORMAT_XRGB);
    CASE_RETURN_OTHER(PIXEL_FORMAT_RGB24);
    CASE_RETURN_OTHER(PIXEL_FORMAT_RGB32);
    CASE_RETURN_OTHER(PIXEL_FORMAT_MJPEG);
    CASE_RETURN_OTHER(PIXEL_FORMAT_MT21);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV420P9);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV420P10);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV422P9);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV422P10);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV444P9);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV444P10);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV420P12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV422P12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV444P12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_Y8);
    CASE_RETURN_OTHER(PIXEL_FORMAT_Y16);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::VideoDecoderConfig::Format> ToProtoVideoDecoderConfigFormat(
    ::media::VideoPixelFormat value) {
  using OriginType = ::media::VideoPixelFormat;
  using OtherType = pb::VideoDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(PIXEL_FORMAT_UNKNOWN);
    CASE_RETURN_OTHER(PIXEL_FORMAT_I420);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YV12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YV16);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YV12A);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YV24);
    CASE_RETURN_OTHER(PIXEL_FORMAT_NV12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_NV21);
    CASE_RETURN_OTHER(PIXEL_FORMAT_UYVY);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUY2);
    CASE_RETURN_OTHER(PIXEL_FORMAT_ARGB);
    CASE_RETURN_OTHER(PIXEL_FORMAT_XRGB);
    CASE_RETURN_OTHER(PIXEL_FORMAT_RGB24);
    CASE_RETURN_OTHER(PIXEL_FORMAT_RGB32);
    CASE_RETURN_OTHER(PIXEL_FORMAT_MJPEG);
    CASE_RETURN_OTHER(PIXEL_FORMAT_MT21);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV420P9);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV420P10);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV422P9);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV422P10);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV444P9);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV444P10);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV420P12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV422P12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV444P12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_Y8);
    CASE_RETURN_OTHER(PIXEL_FORMAT_Y16);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::ColorSpace> ToMediaColorSpace(
    pb::VideoDecoderConfig::ColorSpace value) {
  using OriginType = pb::VideoDecoderConfig;
  using OtherType = ::media::ColorSpace;
  switch (value) {
    CASE_RETURN_OTHER(COLOR_SPACE_UNSPECIFIED);
    CASE_RETURN_OTHER(COLOR_SPACE_JPEG);
    CASE_RETURN_OTHER(COLOR_SPACE_HD_REC709);
    CASE_RETURN_OTHER(COLOR_SPACE_SD_REC601);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::VideoDecoderConfig::ColorSpace>
ToProtoVideoDecoderConfigColorSpace(::media::ColorSpace value) {
  using OriginType = ::media::ColorSpace;
  using OtherType = pb::VideoDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(COLOR_SPACE_UNSPECIFIED);
    CASE_RETURN_OTHER(COLOR_SPACE_JPEG);
    CASE_RETURN_OTHER(COLOR_SPACE_HD_REC709);
    CASE_RETURN_OTHER(COLOR_SPACE_SD_REC601);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::BufferingState> ToMediaBufferingState(
    pb::RendererClientOnBufferingStateChange::State value) {
  using OriginType = pb::RendererClientOnBufferingStateChange;
  using OtherType = ::media::BufferingState;
  switch (value) {
    CASE_RETURN_OTHER(BUFFERING_HAVE_NOTHING);
    CASE_RETURN_OTHER(BUFFERING_HAVE_ENOUGH);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::RendererClientOnBufferingStateChange::State>
ToProtoMediaBufferingState(::media::BufferingState value) {
  using OriginType = ::media::BufferingState;
  using OtherType = pb::RendererClientOnBufferingStateChange;
  switch (value) {
    CASE_RETURN_OTHER(BUFFERING_HAVE_NOTHING);
    CASE_RETURN_OTHER(BUFFERING_HAVE_ENOUGH);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::CdmKeyInformation::KeyStatus>
ToMediaCdmKeyInformationKeyStatus(pb::CdmKeyInformation::KeyStatus value) {
  using OriginType = pb::CdmKeyInformation;
  using OtherType = ::media::CdmKeyInformation;
  switch (value) {
    CASE_RETURN_OTHER(USABLE);
    CASE_RETURN_OTHER(INTERNAL_ERROR);
    CASE_RETURN_OTHER(EXPIRED);
    CASE_RETURN_OTHER(OUTPUT_RESTRICTED);
    CASE_RETURN_OTHER(OUTPUT_DOWNSCALED);
    CASE_RETURN_OTHER(KEY_STATUS_PENDING);
    CASE_RETURN_OTHER(RELEASED);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::CdmKeyInformation::KeyStatus> ToProtoCdmKeyInformation(
    ::media::CdmKeyInformation::KeyStatus value) {
  using OriginType = ::media::CdmKeyInformation;
  using OtherType = pb::CdmKeyInformation;
  switch (value) {
    CASE_RETURN_OTHER(USABLE);
    CASE_RETURN_OTHER(INTERNAL_ERROR);
    CASE_RETURN_OTHER(EXPIRED);
    CASE_RETURN_OTHER(OUTPUT_RESTRICTED);
    CASE_RETURN_OTHER(OUTPUT_DOWNSCALED);
    CASE_RETURN_OTHER(KEY_STATUS_PENDING);
    CASE_RETURN_OTHER(RELEASED);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::CdmPromise::Exception> ToCdmPromiseException(
    pb::MediaKeysException value) {
  using OriginType = pb::MediaKeysException;
  using OtherType = ::media::CdmPromise;
  switch (value) {
    CASE_RETURN_OTHER(NOT_SUPPORTED_ERROR);
    CASE_RETURN_OTHER(INVALID_STATE_ERROR);
    CASE_RETURN_OTHER(INVALID_ACCESS_ERROR);
    CASE_RETURN_OTHER(QUOTA_EXCEEDED_ERROR);
    CASE_RETURN_OTHER(UNKNOWN_ERROR);
    CASE_RETURN_OTHER(CLIENT_ERROR);
    CASE_RETURN_OTHER(OUTPUT_ERROR);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::MediaKeysException> ToProtoMediaKeysException(
    ::media::CdmPromise::Exception value) {
  using OriginType = ::media::CdmPromise;
  using OtherType = pb::MediaKeysException;
  switch (value) {
    CASE_RETURN_OTHER(NOT_SUPPORTED_ERROR);
    CASE_RETURN_OTHER(INVALID_STATE_ERROR);
    CASE_RETURN_OTHER(INVALID_ACCESS_ERROR);
    CASE_RETURN_OTHER(QUOTA_EXCEEDED_ERROR);
    CASE_RETURN_OTHER(UNKNOWN_ERROR);
    CASE_RETURN_OTHER(CLIENT_ERROR);
    CASE_RETURN_OTHER(OUTPUT_ERROR);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::MediaKeys::MessageType> ToMediaMediaKeysMessageType(
    pb::MediaKeysMessageType value) {
  using OriginType = pb::MediaKeysMessageType;
  using OtherType = ::media::MediaKeys;
  switch (value) {
    CASE_RETURN_OTHER(LICENSE_REQUEST);
    CASE_RETURN_OTHER(LICENSE_RENEWAL);
    CASE_RETURN_OTHER(LICENSE_RELEASE);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::MediaKeysMessageType> ToProtoMediaKeysMessageType(
    ::media::MediaKeys::MessageType value) {
  using OriginType = ::media::MediaKeys;
  using OtherType = pb::MediaKeysMessageType;
  switch (value) {
    CASE_RETURN_OTHER(LICENSE_REQUEST);
    CASE_RETURN_OTHER(LICENSE_RENEWAL);
    CASE_RETURN_OTHER(LICENSE_RELEASE);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::MediaKeys::SessionType> ToMediaKeysSessionType(
    pb::MediaKeysSessionType value) {
  using OriginType = pb::MediaKeysSessionType;
  using OtherType = ::media::MediaKeys;
  switch (value) {
    CASE_RETURN_OTHER(TEMPORARY_SESSION);
    CASE_RETURN_OTHER(PERSISTENT_LICENSE_SESSION);
    CASE_RETURN_OTHER(PERSISTENT_RELEASE_MESSAGE_SESSION);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::MediaKeysSessionType> ToProtoMediaKeysSessionType(
    ::media::MediaKeys::SessionType value) {
  using OriginType = ::media::MediaKeys;
  using OtherType = pb::MediaKeysSessionType;
  switch (value) {
    CASE_RETURN_OTHER(TEMPORARY_SESSION);
    CASE_RETURN_OTHER(PERSISTENT_LICENSE_SESSION);
    CASE_RETURN_OTHER(PERSISTENT_RELEASE_MESSAGE_SESSION);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::EmeInitDataType> ToMediaEmeInitDataType(
    pb::CdmCreateSessionAndGenerateRequest::EmeInitDataType value) {
  using OriginType = pb::CdmCreateSessionAndGenerateRequest;
  using OtherType = ::media::EmeInitDataType;
  switch (value) {
    CASE_RETURN_OTHER(UNKNOWN);
    CASE_RETURN_OTHER(WEBM);
    CASE_RETURN_OTHER(CENC);
    CASE_RETURN_OTHER(KEYIDS);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::CdmCreateSessionAndGenerateRequest::EmeInitDataType>
ToProtoMediaEmeInitDataType(::media::EmeInitDataType value) {
  using OriginType = ::media::EmeInitDataType;
  using OtherType = pb::CdmCreateSessionAndGenerateRequest;
  switch (value) {
    CASE_RETURN_OTHER(UNKNOWN);
    CASE_RETURN_OTHER(WEBM);
    CASE_RETURN_OTHER(CENC);
    CASE_RETURN_OTHER(KEYIDS);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<::media::DemuxerStream::Status> ToDemuxerStreamStatus(
    pb::DemuxerStreamReadUntilCallback::Status value) {
  using OriginType = pb::DemuxerStreamReadUntilCallback;
  using OtherType = ::media::DemuxerStream;
  switch (value) {
    CASE_RETURN_OTHER(kOk);
    CASE_RETURN_OTHER(kAborted);
    CASE_RETURN_OTHER(kConfigChanged);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::DemuxerStreamReadUntilCallback::Status>
ToProtoDemuxerStreamStatus(::media::DemuxerStream::Status value) {
  using OriginType = ::media::DemuxerStream;
  using OtherType = pb::DemuxerStreamReadUntilCallback;
  switch (value) {
    CASE_RETURN_OTHER(kOk);
    CASE_RETURN_OTHER(kAborted);
    CASE_RETURN_OTHER(kConfigChanged);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

}  // namespace remoting
}  // namespace media
