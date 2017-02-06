// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audio_modem/public/audio_modem_types.h"

namespace audio_modem {

const int kDefaultRepetitions = 5;
const float kDefaultSampleRate = 48000.0f;
const int kDefaultBitsPerSample = 16;
const float kDefaultCarrierFrequency = 18500.0f;
const media::ChannelLayout kDefaultChannelLayout = media::CHANNEL_LAYOUT_STEREO;

}  // namespace audio_modem
