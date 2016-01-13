// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES
#include <cmath>

#include "media/audio/simple_sources.h"

#include <algorithm>

#include "base/logging.h"

namespace media {

//////////////////////////////////////////////////////////////////////////////
// SineWaveAudioSource implementation.

SineWaveAudioSource::SineWaveAudioSource(int channels,
                                         double freq, double sample_freq)
    : channels_(channels),
      f_(freq / sample_freq),
      time_state_(0),
      cap_(0),
      callbacks_(0),
      errors_(0) {
}

// The implementation could be more efficient if a lookup table is constructed
// but it is efficient enough for our simple needs.
int SineWaveAudioSource::OnMoreData(AudioBus* audio_bus,
                                    AudioBuffersState audio_buffers) {
  base::AutoLock auto_lock(time_lock_);
  callbacks_++;

  // The table is filled with s(t) = kint16max*sin(Theta*t),
  // where Theta = 2*PI*fs.
  // We store the discrete time value |t| in a member to ensure that the
  // next pass starts at a correct state.
  int max_frames = cap_ > 0 ?
      std::min(audio_bus->frames(), cap_ - time_state_) : audio_bus->frames();
  for (int i = 0; i < max_frames; ++i)
    audio_bus->channel(0)[i] = sin(2.0 * M_PI * f_ * time_state_++);
  for (int i = 1; i < audio_bus->channels(); ++i) {
    memcpy(audio_bus->channel(i), audio_bus->channel(0),
           max_frames * sizeof(*audio_bus->channel(i)));
  }
  return max_frames;
}

void SineWaveAudioSource::OnError(AudioOutputStream* stream) {
  errors_++;
}

void SineWaveAudioSource::CapSamples(int cap) {
  base::AutoLock auto_lock(time_lock_);
  DCHECK_GT(cap, 0);
  cap_ = cap;
}

void SineWaveAudioSource::Reset() {
  base::AutoLock auto_lock(time_lock_);
  time_state_ = 0;
}

}  // namespace media
