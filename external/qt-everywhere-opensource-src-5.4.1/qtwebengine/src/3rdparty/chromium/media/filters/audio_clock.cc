// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_clock.h"

#include "base/logging.h"
#include "media/base/buffers.h"

namespace media {

AudioClock::AudioClock(int sample_rate)
    : sample_rate_(sample_rate), last_endpoint_timestamp_(kNoTimestamp()) {
}

AudioClock::~AudioClock() {
}

void AudioClock::WroteAudio(int frames,
                            int delay_frames,
                            float playback_rate,
                            base::TimeDelta timestamp) {
  CHECK_GT(playback_rate, 0);
  CHECK(timestamp != kNoTimestamp());
  DCHECK_GE(frames, 0);
  DCHECK_GE(delay_frames, 0);

  if (last_endpoint_timestamp_ == kNoTimestamp())
    PushBufferedAudio(delay_frames, 0, kNoTimestamp());

  TrimBufferedAudioToMatchDelay(delay_frames);
  PushBufferedAudio(frames, playback_rate, timestamp);

  last_endpoint_timestamp_ = timestamp;
}

void AudioClock::WroteSilence(int frames, int delay_frames) {
  DCHECK_GE(frames, 0);
  DCHECK_GE(delay_frames, 0);

  if (last_endpoint_timestamp_ == kNoTimestamp())
    PushBufferedAudio(delay_frames, 0, kNoTimestamp());

  TrimBufferedAudioToMatchDelay(delay_frames);
  PushBufferedAudio(frames, 0, kNoTimestamp());
}

base::TimeDelta AudioClock::CurrentMediaTimestamp() const {
  int silence_frames = 0;
  for (size_t i = 0; i < buffered_audio_.size(); ++i) {
    // Account for silence ahead of the buffer closest to being played.
    if (buffered_audio_[i].playback_rate == 0) {
      silence_frames += buffered_audio_[i].frames;
      continue;
    }

    // Multiply by playback rate as frames represent time-scaled audio.
    return buffered_audio_[i].endpoint_timestamp -
           base::TimeDelta::FromMicroseconds(
               ((buffered_audio_[i].frames * buffered_audio_[i].playback_rate) +
                silence_frames) /
               sample_rate_ * base::Time::kMicrosecondsPerSecond);
  }

  // Either:
  //   1) AudioClock is uninitialziated and we'll return kNoTimestamp()
  //   2) All previously buffered audio has been replaced by silence,
  //      meaning media time is now at the last endpoint
  return last_endpoint_timestamp_;
}

void AudioClock::TrimBufferedAudioToMatchDelay(int delay_frames) {
  if (buffered_audio_.empty())
    return;

  size_t i = buffered_audio_.size() - 1;
  while (true) {
    if (buffered_audio_[i].frames <= delay_frames) {
      // Reached the end before accounting for all of |delay_frames|. This
      // means we haven't written enough audio data yet to account for hardware
      // delay. In this case, do nothing.
      if (i == 0)
        return;

      // Keep accounting for |delay_frames|.
      delay_frames -= buffered_audio_[i].frames;
      --i;
      continue;
    }

    // All of |delay_frames| has been accounted for: adjust amount of frames
    // left in current buffer. All preceeding elements with index < |i| should
    // be considered played out and hence discarded.
    buffered_audio_[i].frames = delay_frames;
    break;
  }

  // At this point |i| points at what will be the new head of |buffered_audio_|
  // however if it contains no audio it should be removed as well.
  if (buffered_audio_[i].frames == 0)
    ++i;

  buffered_audio_.erase(buffered_audio_.begin(), buffered_audio_.begin() + i);
}

void AudioClock::PushBufferedAudio(int frames,
                                   float playback_rate,
                                   base::TimeDelta endpoint_timestamp) {
  if (playback_rate == 0)
    DCHECK(endpoint_timestamp == kNoTimestamp());

  if (frames == 0)
    return;

  // Avoid creating extra elements where possible.
  if (!buffered_audio_.empty() &&
      buffered_audio_.back().playback_rate == playback_rate) {
    buffered_audio_.back().frames += frames;
    buffered_audio_.back().endpoint_timestamp = endpoint_timestamp;
    return;
  }

  buffered_audio_.push_back(
      BufferedAudio(frames, playback_rate, endpoint_timestamp));
}

AudioClock::BufferedAudio::BufferedAudio(int frames,
                                         float playback_rate,
                                         base::TimeDelta endpoint_timestamp)
    : frames(frames),
      playback_rate(playback_rate),
      endpoint_timestamp(endpoint_timestamp) {
}

}  // namespace media
