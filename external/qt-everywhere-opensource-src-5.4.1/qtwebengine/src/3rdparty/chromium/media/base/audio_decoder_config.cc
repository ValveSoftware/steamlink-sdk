// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_decoder_config.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "media/audio/sample_rates.h"
#include "media/base/limits.h"
#include "media/base/sample_format.h"

namespace media {

AudioDecoderConfig::AudioDecoderConfig()
    : codec_(kUnknownAudioCodec),
      sample_format_(kUnknownSampleFormat),
      bytes_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_UNSUPPORTED),
      samples_per_second_(0),
      bytes_per_frame_(0),
      is_encrypted_(false),
      codec_delay_(0) {
}

AudioDecoderConfig::AudioDecoderConfig(AudioCodec codec,
                                       SampleFormat sample_format,
                                       ChannelLayout channel_layout,
                                       int samples_per_second,
                                       const uint8* extra_data,
                                       size_t extra_data_size,
                                       bool is_encrypted) {
  Initialize(codec, sample_format, channel_layout, samples_per_second,
             extra_data, extra_data_size, is_encrypted, true,
             base::TimeDelta(), 0);
}

void AudioDecoderConfig::Initialize(AudioCodec codec,
                                    SampleFormat sample_format,
                                    ChannelLayout channel_layout,
                                    int samples_per_second,
                                    const uint8* extra_data,
                                    size_t extra_data_size,
                                    bool is_encrypted,
                                    bool record_stats,
                                    base::TimeDelta seek_preroll,
                                    int codec_delay) {
  CHECK((extra_data_size != 0) == (extra_data != NULL));

  if (record_stats) {
    UMA_HISTOGRAM_ENUMERATION("Media.AudioCodec", codec, kAudioCodecMax + 1);
    UMA_HISTOGRAM_ENUMERATION("Media.AudioSampleFormat", sample_format,
                              kSampleFormatMax + 1);
    UMA_HISTOGRAM_ENUMERATION("Media.AudioChannelLayout", channel_layout,
                              CHANNEL_LAYOUT_MAX + 1);
    AudioSampleRate asr;
    if (ToAudioSampleRate(samples_per_second, &asr)) {
      UMA_HISTOGRAM_ENUMERATION("Media.AudioSamplesPerSecond", asr,
                                kAudioSampleRateMax + 1);
    } else {
      UMA_HISTOGRAM_COUNTS(
          "Media.AudioSamplesPerSecondUnexpected", samples_per_second);
    }
  }

  codec_ = codec;
  channel_layout_ = channel_layout;
  samples_per_second_ = samples_per_second;
  sample_format_ = sample_format;
  bytes_per_channel_ = SampleFormatToBytesPerChannel(sample_format);
  extra_data_.assign(extra_data, extra_data + extra_data_size);
  is_encrypted_ = is_encrypted;
  seek_preroll_ = seek_preroll;
  codec_delay_ = codec_delay;

  int channels = ChannelLayoutToChannelCount(channel_layout_);
  bytes_per_frame_ = channels * bytes_per_channel_;
}

AudioDecoderConfig::~AudioDecoderConfig() {}

bool AudioDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownAudioCodec &&
         channel_layout_ != CHANNEL_LAYOUT_UNSUPPORTED &&
         bytes_per_channel_ > 0 &&
         bytes_per_channel_ <= limits::kMaxBytesPerSample &&
         samples_per_second_ > 0 &&
         samples_per_second_ <= limits::kMaxSampleRate &&
         sample_format_ != kUnknownSampleFormat &&
         seek_preroll_ >= base::TimeDelta() &&
         codec_delay_ >= 0;
}

bool AudioDecoderConfig::Matches(const AudioDecoderConfig& config) const {
  return ((codec() == config.codec()) &&
          (bytes_per_channel() == config.bytes_per_channel()) &&
          (channel_layout() == config.channel_layout()) &&
          (samples_per_second() == config.samples_per_second()) &&
          (extra_data_size() == config.extra_data_size()) &&
          (!extra_data() || !memcmp(extra_data(), config.extra_data(),
                                    extra_data_size())) &&
          (is_encrypted() == config.is_encrypted()) &&
          (sample_format() == config.sample_format()) &&
          (seek_preroll() == config.seek_preroll()) &&
          (codec_delay() == config.codec_delay()));
}

std::string AudioDecoderConfig::AsHumanReadableString() const {
  std::ostringstream s;
  s << "codec: " << codec()
    << " bytes_per_channel: " << bytes_per_channel()
    << " channel_layout: " << channel_layout()
    << " samples_per_second: " << samples_per_second()
    << " sample_format: " << sample_format()
    << " bytes_per_frame: " << bytes_per_frame()
    << " seek_preroll: " << seek_preroll().InMilliseconds() << "ms"
    << " codec_delay: " << codec_delay()
    << " has extra data? " << (extra_data() ? "true" : "false")
    << " encrypted? " << (is_encrypted() ? "true" : "false");
  return s.str();
}

}  // namespace media
