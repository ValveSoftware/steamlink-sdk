// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIO_MODEM_PUBLIC_AUDIO_MODEM_TYPES_H_
#define COMPONENTS_AUDIO_MODEM_PUBLIC_AUDIO_MODEM_TYPES_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "media/base/channel_layout.h"

// Various constants and types used for the audio modem.
// TODO(ckehoe): Move the constants out into their own header.

namespace audio_modem {

// Number of repetitions of the audio token in one sequence of samples.
extern const int kDefaultRepetitions;

// Whispernet encoding parameters. These need to be the same on all platforms.
extern const float kDefaultSampleRate;
extern const int kDefaultBitsPerSample;
extern const float kDefaultCarrierFrequency;

// This really needs to be configurable since it doesn't need
// to be consistent across platforms, or even playing/recording.
extern const media::ChannelLayout kDefaultChannelLayout;

// Enum to represent the audio band (audible vs. ultrasound).
// AUDIBLE and INAUDIBLE are used as array indices.
enum AudioType {
  AUDIBLE = 0,
  INAUDIBLE = 1,
  BOTH = 2,
  AUDIO_TYPE_UNKNOWN = 3,
};

// Struct representing an audio token.
// TODO(ckehoe): Make this use the AudioType enum instead of a boolean.
struct AudioToken final {
  AudioToken(const std::string& token, bool audible)
      : token(token),
        audible(audible) {}

  std::string token;
  bool audible;
};

// Struct to hold the encoding parameters for tokens.
// Parity is on by default.
struct TokenParameters {
  TokenParameters() : TokenParameters(0, false, true) {}

  explicit TokenParameters(size_t length)
      : TokenParameters(length, false, true) {}

  TokenParameters(size_t length, bool crc, bool parity)
      : length(length), crc(crc), parity(parity) {}

  size_t length;
  bool crc;
  bool parity;
};

// Callback to pass around found tokens.
using TokensCallback = base::Callback<void(const std::vector<AudioToken>&)>;

}  // namespace audio_modem

#endif  // COMPONENTS_AUDIO_MODEM_PUBLIC_AUDIO_MODEM_TYPES_H_
