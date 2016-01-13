// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#if defined(USE_ALSA)
#include "media/audio/alsa/audio_manager_alsa.h"
#else
#include "media/audio/fake_audio_manager.h"
#endif
#if defined(USE_CRAS)
#include "media/audio/cras/audio_manager_cras.h"
#endif
#if defined(USE_PULSEAUDIO)
#include "media/audio/pulse/audio_manager_pulse.h"
#endif
#include "media/base/media_switches.h"

namespace media {

enum LinuxAudioIO {
  kPulse,
  kAlsa,
  kCras,
  kAudioIOMax = kCras  // Must always be equal to largest logged entry.
};

AudioManager* CreateAudioManager(AudioLogFactory* audio_log_factory) {
#if defined(USE_CRAS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseCras)) {
    UMA_HISTOGRAM_ENUMERATION("Media.LinuxAudioIO", kCras, kAudioIOMax + 1);
    return new AudioManagerCras(audio_log_factory);
  }
#endif

#if defined(USE_PULSEAUDIO)
  AudioManager* manager = AudioManagerPulse::Create(audio_log_factory);
  if (manager) {
    UMA_HISTOGRAM_ENUMERATION("Media.LinuxAudioIO", kPulse, kAudioIOMax + 1);
    return manager;
  }
#endif

#if defined(USE_ALSA)
  UMA_HISTOGRAM_ENUMERATION("Media.LinuxAudioIO", kAlsa, kAudioIOMax + 1);
  return new AudioManagerAlsa(audio_log_factory);
#else
  return new FakeAudioManager(audio_log_factory);
#endif
}

}  // namespace media
