// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_CLOCK_H_
#define MEDIA_FILTERS_AUDIO_CLOCK_H_

#include <deque>

#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// Models a queue of buffered audio in a playback pipeline for use with
// estimating the amount of delay in wall clock time. Takes changes in playback
// rate into account to handle scenarios where multiple rates may be present in
// a playback pipeline with large delay.
class MEDIA_EXPORT AudioClock {
 public:
  explicit AudioClock(int sample_rate);
  ~AudioClock();

  // |frames| amount of audio data scaled to |playback_rate| was written.
  // |delay_frames| is the current amount of hardware delay.
  // |timestamp| is the endpoint media timestamp of the audio data written.
  void WroteAudio(int frames,
                  int delay_frames,
                  float playback_rate,
                  base::TimeDelta timestamp);

  // |frames| amount of silence was written.
  // |delay_frames| is the current amount of hardware delay.
  void WroteSilence(int frames, int delay_frames);

  // Calculates the current media timestamp taking silence and changes in
  // playback rate into account.
  base::TimeDelta CurrentMediaTimestamp() const;

  // Returns the last endpoint timestamp provided to WroteAudio().
  base::TimeDelta last_endpoint_timestamp() const {
    return last_endpoint_timestamp_;
  }

 private:
  void TrimBufferedAudioToMatchDelay(int delay_frames);
  void PushBufferedAudio(int frames,
                         float playback_rate,
                         base::TimeDelta endpoint_timestamp);

  const int sample_rate_;

  // Initially set to kNoTimestamp(), otherwise is the last endpoint timestamp
  // delivered to WroteAudio(). A copy is kept outside of |buffered_audio_| to
  // handle the case where all of |buffered_audio_| has been replaced with
  // silence.
  base::TimeDelta last_endpoint_timestamp_;

  struct BufferedAudio {
    BufferedAudio(int frames,
                  float playback_rate,
                  base::TimeDelta endpoint_timestamp);

    int frames;
    float playback_rate;
    base::TimeDelta endpoint_timestamp;
  };

  std::deque<BufferedAudio> buffered_audio_;

  DISALLOW_COPY_AND_ASSIGN(AudioClock);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_CLOCK_H_
