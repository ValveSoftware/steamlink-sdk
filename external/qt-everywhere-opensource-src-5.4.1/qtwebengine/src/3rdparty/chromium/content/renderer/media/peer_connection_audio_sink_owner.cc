// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_audio_sink_owner.h"

#include "content/renderer/media/webrtc_audio_device_impl.h"

namespace content {

PeerConnectionAudioSinkOwner::PeerConnectionAudioSinkOwner(
    PeerConnectionAudioSink* sink)
    : delegate_(sink) {
}

int PeerConnectionAudioSinkOwner::OnData(const int16* audio_data,
                                         int sample_rate,
                                         int number_of_channels,
                                         int number_of_frames,
                                         const std::vector<int>& channels,
                                         int audio_delay_milliseconds,
                                         int current_volume,
                                         bool need_audio_processing,
                                         bool key_pressed) {
  base::AutoLock lock(lock_);
  if (delegate_) {
    return delegate_->OnData(audio_data,
                             sample_rate,
                             number_of_channels,
                             number_of_frames,
                             channels,
                             audio_delay_milliseconds,
                             current_volume,
                             need_audio_processing,
                             key_pressed);
  }

  return 0;
}

void PeerConnectionAudioSinkOwner::OnSetFormat(
    const media::AudioParameters& params) {
  base::AutoLock lock(lock_);
  if (delegate_)
    delegate_->OnSetFormat(params);
}

void PeerConnectionAudioSinkOwner::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  // Not forwarded at the moment.
}

void PeerConnectionAudioSinkOwner::Reset() {
  base::AutoLock lock(lock_);
  delegate_ = NULL;
}

bool PeerConnectionAudioSinkOwner::IsEqual(
    const MediaStreamAudioSink* other) const {
  DCHECK(other);
  return false;
}

bool PeerConnectionAudioSinkOwner::IsEqual(
    const PeerConnectionAudioSink* other) const {
  DCHECK(other);
  base::AutoLock lock(lock_);
  return (other == delegate_);
}

}  // namespace content
