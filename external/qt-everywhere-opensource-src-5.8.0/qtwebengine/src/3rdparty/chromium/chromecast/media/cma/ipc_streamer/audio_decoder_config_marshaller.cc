// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc_streamer/audio_decoder_config_marshaller.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/logging.h"
#include "chromecast/media/cma/ipc/media_message.h"
#include "chromecast/media/cma/ipc_streamer/encryption_scheme_marshaller.h"
#include "media/base/audio_decoder_config.h"

namespace chromecast {
namespace media {

namespace {
const size_t kMaxExtraDataSize = 16 * 1024;
}

// static
void AudioDecoderConfigMarshaller::Write(
    const ::media::AudioDecoderConfig& config, MediaMessage* msg) {
  CHECK(msg->WritePod(config.codec()));
  CHECK(msg->WritePod(config.channel_layout()));
  CHECK(msg->WritePod(config.samples_per_second()));
  CHECK(msg->WritePod(config.sample_format()));
  EncryptionSchemeMarshaller::Write(config.encryption_scheme(), msg);
  CHECK(msg->WritePod(config.extra_data().size()));
  if (!config.extra_data().empty())
    CHECK(msg->WriteBuffer(&config.extra_data()[0],
                           config.extra_data().size()));
}

// static
::media::AudioDecoderConfig AudioDecoderConfigMarshaller::Read(
    MediaMessage* msg) {
  ::media::AudioCodec codec;
  ::media::SampleFormat sample_format;
  ::media::ChannelLayout channel_layout;
  int samples_per_second;
  size_t extra_data_size;
  std::vector<uint8_t> extra_data;
  ::media::EncryptionScheme encryption_scheme;

  CHECK(msg->ReadPod(&codec));
  CHECK(msg->ReadPod(&channel_layout));
  CHECK(msg->ReadPod(&samples_per_second));
  CHECK(msg->ReadPod(&sample_format));
  encryption_scheme = EncryptionSchemeMarshaller::Read(msg);
  CHECK(msg->ReadPod(&extra_data_size));

  CHECK_GE(codec, ::media::kUnknownAudioCodec);
  CHECK_LE(codec, ::media::kAudioCodecMax);
  CHECK_GE(channel_layout, ::media::CHANNEL_LAYOUT_NONE);
  CHECK_LE(channel_layout, ::media::CHANNEL_LAYOUT_MAX);
  CHECK_GE(sample_format, ::media::kUnknownSampleFormat);
  CHECK_LE(sample_format, ::media::kSampleFormatMax);
  CHECK_LT(extra_data_size, kMaxExtraDataSize);
  if (extra_data_size > 0) {
    extra_data.resize(extra_data_size);
    CHECK(msg->ReadBuffer(&extra_data[0], extra_data.size()));
  }

  return ::media::AudioDecoderConfig(
      codec, sample_format,
      channel_layout, samples_per_second,
      extra_data, encryption_scheme);
}

}  // namespace media
}  // namespace chromecast
