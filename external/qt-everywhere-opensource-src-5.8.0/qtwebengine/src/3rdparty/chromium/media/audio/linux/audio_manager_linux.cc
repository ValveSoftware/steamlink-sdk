// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "media/base/media_switches.h"

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

namespace media {

enum LinuxAudioIO {
  kPulse,
  kAlsa,
  kCras,
  kAudioIOMax = kCras  // Must always be equal to largest logged entry.
};

ScopedAudioManagerPtr CreateAudioManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
    AudioLogFactory* audio_log_factory) {
#if defined(USE_CRAS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseCras)) {
    UMA_HISTOGRAM_ENUMERATION("Media.LinuxAudioIO", kCras, kAudioIOMax + 1);
    return ScopedAudioManagerPtr(
        new AudioManagerCras(std::move(task_runner),
                             std::move(worker_task_runner), audio_log_factory));
  }
#endif

#if defined(USE_PULSEAUDIO)
  // Do not move task runners when creating AudioManagerPulse.
  // If the creation fails, we need to use the task runners to create other
  // AudioManager implementations.
  std::unique_ptr<AudioManagerPulse, AudioManagerDeleter> manager(
      new AudioManagerPulse(task_runner, worker_task_runner,
                            audio_log_factory));
  if (manager->Init()) {
    UMA_HISTOGRAM_ENUMERATION("Media.LinuxAudioIO", kPulse, kAudioIOMax + 1);
    return std::move(manager);
  }
  DVLOG(1) << "PulseAudio is not available on the OS";
#endif

#if defined(USE_ALSA)
  UMA_HISTOGRAM_ENUMERATION("Media.LinuxAudioIO", kAlsa, kAudioIOMax + 1);
  return ScopedAudioManagerPtr(
      new AudioManagerAlsa(std::move(task_runner),
                           std::move(worker_task_runner), audio_log_factory));
#else
  return ScopedAudioManagerPtr(
      new FakeAudioManager(std::move(task_runner),
                           std::move(worker_task_runner), audio_log_factory));
#endif
}

}  // namespace media
