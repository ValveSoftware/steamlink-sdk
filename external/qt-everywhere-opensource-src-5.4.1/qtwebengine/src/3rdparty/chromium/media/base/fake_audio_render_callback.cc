// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include <cmath>

#include "media/base/fake_audio_render_callback.h"

namespace media {

FakeAudioRenderCallback::FakeAudioRenderCallback(double step)
    : half_fill_(false),
      step_(step),
      last_audio_delay_milliseconds_(-1),
      volume_(1) {
  reset();
}

FakeAudioRenderCallback::~FakeAudioRenderCallback() {}

int FakeAudioRenderCallback::Render(AudioBus* audio_bus,
                                    int audio_delay_milliseconds) {
  last_audio_delay_milliseconds_ = audio_delay_milliseconds;
  int number_of_frames = audio_bus->frames();
  if (half_fill_)
    number_of_frames /= 2;

  // Fill first channel with a sine wave.
  for (int i = 0; i < number_of_frames; ++i)
    audio_bus->channel(0)[i] = sin(2 * M_PI * (x_ + step_ * i));
  x_ += number_of_frames * step_;

  // Copy first channel into the rest of the channels.
  for (int i = 1; i < audio_bus->channels(); ++i)
    memcpy(audio_bus->channel(i), audio_bus->channel(0),
           number_of_frames * sizeof(*audio_bus->channel(i)));

  return number_of_frames;
}

double FakeAudioRenderCallback::ProvideInput(AudioBus* audio_bus,
                                             base::TimeDelta buffer_delay) {
  Render(audio_bus, buffer_delay.InMillisecondsF() + 0.5);
  return volume_;
}

}  // namespace media
