// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_level_calculator.h"

#include "base/logging.h"
#include "base/stl_util.h"

namespace content {

namespace {

// Calculates the maximum absolute amplitude of the audio data.
// Note, the return value can be bigger than std::numeric_limits<int16>::max().
int MaxAmplitude(const int16* audio_data, int length) {
  int max = 0, absolute = 0;
  for (int i = 0; i < length; ++i) {
    absolute = std::abs(audio_data[i]);
    if (absolute > max)
      max = absolute;
  }
  // The range of int16 is [-32768, 32767], verify the |max| should not be
  // bigger than 32768.
  DCHECK(max <= std::abs(std::numeric_limits<int16>::min()));

  return max;
}

}  // namespace

MediaStreamAudioLevelCalculator::MediaStreamAudioLevelCalculator()
    : counter_(0),
      max_amplitude_(0),
      level_(0) {
}

MediaStreamAudioLevelCalculator::~MediaStreamAudioLevelCalculator() {
}

int MediaStreamAudioLevelCalculator::Calculate(const int16* audio_data,
                                               int number_of_channels,
                                               int number_of_frames) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // |level_| is updated every 10 callbacks. For the case where callback comes
  // every 10ms, |level_| will be updated approximately every 100ms.
  static const int kUpdateFrequency = 10;

  int max = MaxAmplitude(audio_data, number_of_channels * number_of_frames);
  max_amplitude_ = std::max(max_amplitude_, max);

  if (counter_++ == kUpdateFrequency) {
    level_ = max_amplitude_;

    // Decay the absolute maximum amplitude by 1/4.
    max_amplitude_ >>= 2;

    // Reset the counter.
    counter_ = 0;
  }

  return level_;
}

}  // namespace content
