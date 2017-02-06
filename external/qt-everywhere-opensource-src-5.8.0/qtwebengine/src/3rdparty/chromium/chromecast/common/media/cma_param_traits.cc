// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/media/cma_param_traits.h"

#include <stdint.h>

#include <vector>

#include "chromecast/common/media/cma_param_traits_macros.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

// Note(slan): These are defined in the following headers:
//   media/gpu/ipc/common/media_param_traits_macros.h
//   content/common/media/audio_messages.h
// but including these headers is not correct, so forward declare them here.
// Their existing implementations in content/ and media/ should be linked in.
// This hack will not be necessary with Mojo.
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::ChannelLayout,
                              media::ChannelLayout::CHANNEL_LAYOUT_NONE,
                              media::ChannelLayout::CHANNEL_LAYOUT_MAX)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::VideoCodecProfile,
                              media::VIDEO_CODEC_PROFILE_MIN,
                              media::VIDEO_CODEC_PROFILE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(media::VideoPixelFormat, media::PIXEL_FORMAT_MAX)

namespace IPC {

template <>
struct ParamTraits<media::EncryptionScheme::Pattern> {
  typedef media::EncryptionScheme::Pattern param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m, base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};


void ParamTraits<media::AudioDecoderConfig>::Write(
    base::Pickle* m,
    const media::AudioDecoderConfig& p) {
  WriteParam(m, p.codec());
  WriteParam(m, p.sample_format());
  WriteParam(m, p.channel_layout());
  WriteParam(m, p.samples_per_second());
  WriteParam(m, p.encryption_scheme());
  WriteParam(m, p.extra_data());
}

bool ParamTraits<media::AudioDecoderConfig>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    media::AudioDecoderConfig* r) {
  media::AudioCodec codec;
  media::SampleFormat sample_format;
  media::ChannelLayout channel_layout;
  int samples_per_second;
  media::EncryptionScheme encryption_scheme;
  std::vector<uint8_t> extra_data;
  if (!ReadParam(m, iter, &codec) || !ReadParam(m, iter, &sample_format) ||
      !ReadParam(m, iter, &channel_layout) ||
      !ReadParam(m, iter, &samples_per_second) ||
      !ReadParam(m, iter, &encryption_scheme) ||
      !ReadParam(m, iter, &extra_data))
    return false;
  *r = media::AudioDecoderConfig(codec, sample_format, channel_layout,
                                 samples_per_second, extra_data,
                                 encryption_scheme);
  return true;
}

void ParamTraits<media::AudioDecoderConfig>::Log(
    const media::AudioDecoderConfig& p, std::string* l) {
  l->append(base::StringPrintf("<AudioDecoderConfig>"));
}

void ParamTraits<media::VideoDecoderConfig>::Write(
    base::Pickle* m,
    const media::VideoDecoderConfig& p) {
  WriteParam(m, p.codec());
  WriteParam(m, p.profile());
  WriteParam(m, p.format());
  WriteParam(m, p.color_space());
  WriteParam(m, p.coded_size());
  WriteParam(m, p.visible_rect());
  WriteParam(m, p.natural_size());
  WriteParam(m, p.encryption_scheme());
  WriteParam(m, p.extra_data());
}

bool ParamTraits<media::VideoDecoderConfig>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    media::VideoDecoderConfig* r) {
  media::VideoCodec codec;
  media::VideoCodecProfile profile;
  media::VideoPixelFormat format;
  media::ColorSpace color_space;
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  gfx::Size natural_size;
  media::EncryptionScheme encryption_scheme;
  std::vector<uint8_t> extra_data;
  if (!ReadParam(m, iter, &codec) || !ReadParam(m, iter, &profile) ||
      !ReadParam(m, iter, &format) || !ReadParam(m, iter, &color_space) ||
      !ReadParam(m, iter, &coded_size) || !ReadParam(m, iter, &visible_rect) ||
      !ReadParam(m, iter, &natural_size) ||
      !ReadParam(m, iter, &encryption_scheme) ||
      !ReadParam(m, iter, &extra_data))
    return false;
  *r = media::VideoDecoderConfig(codec, profile, format, color_space,
                                 coded_size, visible_rect, natural_size,
                                 extra_data, encryption_scheme);
  return true;
}

void ParamTraits<media::VideoDecoderConfig>::Log(
    const media::VideoDecoderConfig& p, std::string* l) {
  l->append(base::StringPrintf("<VideoDecoderConfig>"));
}

void ParamTraits<media::EncryptionScheme>::Write(
    base::Pickle* m, const param_type& p) {
  WriteParam(m, p.mode());
  WriteParam(m, p.pattern());
}

bool ParamTraits<media::EncryptionScheme>::Read(
    const base::Pickle* m, base::PickleIterator* iter, param_type* r) {
  media::EncryptionScheme::CipherMode mode;
  media::EncryptionScheme::Pattern pattern;
  if (!ReadParam(m, iter, &mode) || !ReadParam(m, iter, &pattern))
    return false;
  *r = media::EncryptionScheme(mode, pattern);
  return true;
}

void ParamTraits<media::EncryptionScheme>::Log(
    const param_type& p, std::string* l) {
  l->append(base::StringPrintf("<EncryptionScheme>"));
}

void ParamTraits<media::EncryptionScheme::Pattern>::Write(
    base::Pickle* m, const param_type& p) {
  WriteParam(m, p.encrypt_blocks());
  WriteParam(m, p.skip_blocks());
}

bool ParamTraits<media::EncryptionScheme::Pattern>::Read(
    const base::Pickle* m, base::PickleIterator* iter, param_type* r) {
  uint32_t encrypt_blocks, skip_blocks;
  if (!ReadParam(m, iter, &encrypt_blocks) || !ReadParam(m, iter, &skip_blocks))
    return false;
  *r = media::EncryptionScheme::Pattern(encrypt_blocks, skip_blocks);
  return true;
}

void ParamTraits<media::EncryptionScheme::Pattern>::Log(
    const param_type& p, std::string* l) {
  l->append(base::StringPrintf("<Pattern>"));
}

}  // namespace IPC
