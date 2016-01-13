// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_sink_owner.h"

#include "content/public/renderer/media_stream_audio_sink.h"
#include "media/audio/audio_parameters.h"

namespace content {

MediaStreamAudioSinkOwner::MediaStreamAudioSinkOwner(MediaStreamAudioSink* sink)
    : delegate_(sink) {
}

int MediaStreamAudioSinkOwner::OnData(const int16* audio_data,
                                      int sample_rate,
                                      int number_of_channels,
                                      int number_of_frames,
                                      const std::vector<int>& channels,
                                      int audio_delay_milliseconds,
                                      int current_volume,
                                      bool need_audio_processing,
                                      bool key_pressed) {
  base::AutoLock lock(lock_);
  // TODO(xians): Investigate on the possibility of not calling out with the
  // lock.
  if (delegate_) {
    delegate_->OnData(audio_data,
                      sample_rate,
                      number_of_channels,
                      number_of_frames);
  }

  return 0;
}

void MediaStreamAudioSinkOwner::OnSetFormat(
    const media::AudioParameters& params) {
  base::AutoLock lock(lock_);
  if (delegate_)
    delegate_->OnSetFormat(params);
}

void MediaStreamAudioSinkOwner::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  base::AutoLock lock(lock_);
  if (delegate_)
    delegate_->OnReadyStateChanged(state);
}

void MediaStreamAudioSinkOwner::Reset() {
  base::AutoLock lock(lock_);
  delegate_ = NULL;
}

bool MediaStreamAudioSinkOwner::IsEqual(
    const MediaStreamAudioSink* other) const {
  DCHECK(other);
  base::AutoLock lock(lock_);
  return (other == delegate_);
}

bool MediaStreamAudioSinkOwner::IsEqual(
    const PeerConnectionAudioSink* other) const {
  DCHECK(other);
  return false;
}

}  // namespace content
