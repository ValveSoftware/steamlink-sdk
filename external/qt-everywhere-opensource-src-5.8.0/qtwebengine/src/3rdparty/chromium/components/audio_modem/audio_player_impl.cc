// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audio_modem/audio_player_impl.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/audio_modem/public/audio_modem_types.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"

namespace {

const int kDefaultFrameCount = 1024;
const double kOutputVolumePercent = 1.0f;

}  // namespace

namespace audio_modem {

// Public methods.

AudioPlayerImpl::AudioPlayerImpl()
    : is_playing_(false), stream_(nullptr), frame_index_(0) {
}

AudioPlayerImpl::~AudioPlayerImpl() {
}

void AudioPlayerImpl::Initialize() {
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioPlayerImpl::InitializeOnAudioThread,
                 base::Unretained(this)));
}

void AudioPlayerImpl::Play(
    const scoped_refptr<media::AudioBusRefCounted>& samples) {
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioPlayerImpl::PlayOnAudioThread,
                 base::Unretained(this),
                 samples));
}

void AudioPlayerImpl::Stop() {
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioPlayerImpl::StopOnAudioThread, base::Unretained(this)));
}

void AudioPlayerImpl::Finalize() {
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioPlayerImpl::FinalizeOnAudioThread,
                 base::Unretained(this)));
}

// Private methods.

void AudioPlayerImpl::InitializeOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  stream_ = output_stream_for_testing_
                ? output_stream_for_testing_.get()
                : media::AudioManager::Get()->MakeAudioOutputStreamProxy(
                      media::AudioParameters(
                          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                          media::CHANNEL_LAYOUT_MONO,
                          kDefaultSampleRate,
                          kDefaultBitsPerSample,
                          kDefaultFrameCount),
                      std::string());

  if (!stream_ || !stream_->Open()) {
    LOG(ERROR) << "Failed to open an output stream.";
    if (stream_) {
      stream_->Close();
      stream_ = nullptr;
    }
    return;
  }
  stream_->SetVolume(kOutputVolumePercent);
}

void AudioPlayerImpl::PlayOnAudioThread(
    const scoped_refptr<media::AudioBusRefCounted>& samples) {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  if (!stream_ || is_playing_)
    return;

  {
    base::AutoLock al(state_lock_);
    samples_ = samples;
    frame_index_ = 0;
  }

  VLOG(3) << "Starting playback.";
  is_playing_ = true;
  stream_->Start(this);
}

void AudioPlayerImpl::StopOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  if (!stream_ || !is_playing_)
    return;

  VLOG(3) << "Stopping playback.";
  stream_->Stop();
  is_playing_ = false;
}

void AudioPlayerImpl::StopAndCloseOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  if (!stream_)
    return;

  StopOnAudioThread();
  stream_->Close();
  stream_ = nullptr;
}

void AudioPlayerImpl::FinalizeOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  StopAndCloseOnAudioThread();
  delete this;
}

int AudioPlayerImpl::OnMoreData(media::AudioBus* dest,
                                uint32_t /* total_bytes_delay */,
                                uint32_t /* frames_skipped */) {
  base::AutoLock al(state_lock_);
  // Continuously play our samples till explicitly told to stop.
  const int leftover_frames = samples_->frames() - frame_index_;
  const int frames_to_copy = std::min(dest->frames(), leftover_frames);

  samples_->CopyPartialFramesTo(frame_index_, frames_to_copy, 0, dest);
  frame_index_ += frames_to_copy;

  // If we didn't fill the destination audio bus, wrap around and fill the rest.
  if (leftover_frames <= dest->frames()) {
    samples_->CopyPartialFramesTo(
        0, dest->frames() - frames_to_copy, frames_to_copy, dest);
    frame_index_ = dest->frames() - frames_to_copy;
  }

  return dest->frames();
}

void AudioPlayerImpl::OnError(media::AudioOutputStream* /* stream */) {
  LOG(ERROR) << "Error during system sound reproduction.";
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioPlayerImpl::StopAndCloseOnAudioThread,
                 base::Unretained(this)));
}

}  // namespace audio_modem
