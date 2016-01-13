// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/null_audio_sink.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "media/audio/fake_audio_consumer.h"
#include "media/base/audio_hash.h"

namespace media {

NullAudioSink::NullAudioSink(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : initialized_(false),
      playing_(false),
      callback_(NULL),
      task_runner_(task_runner) {
}

NullAudioSink::~NullAudioSink() {}

void NullAudioSink::Initialize(const AudioParameters& params,
                               RenderCallback* callback) {
  DCHECK(!initialized_);
  fake_consumer_.reset(new FakeAudioConsumer(task_runner_, params));
  callback_ = callback;
  initialized_ = true;
}

void NullAudioSink::Start() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!playing_);
}

void NullAudioSink::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Stop may be called at any time, so we have to check before stopping.
  if (fake_consumer_)
    fake_consumer_->Stop();
}

void NullAudioSink::Play() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(initialized_);

  if (playing_)
    return;

  fake_consumer_->Start(base::Bind(
      &NullAudioSink::CallRender, base::Unretained(this)));
  playing_ = true;
}

void NullAudioSink::Pause() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!playing_)
    return;

  fake_consumer_->Stop();
  playing_ = false;
}

bool NullAudioSink::SetVolume(double volume) {
  // Audio is always muted.
  return volume == 0.0;
}

void NullAudioSink::CallRender(AudioBus* audio_bus) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  int frames_received = callback_->Render(audio_bus, 0);
  if (!audio_hash_ || frames_received <= 0)
    return;

  audio_hash_->Update(audio_bus, frames_received);
}

void NullAudioSink::StartAudioHashForTesting() {
  DCHECK(!initialized_);
  audio_hash_.reset(new AudioHash());
}

std::string NullAudioSink::GetAudioHashForTesting() {
  return audio_hash_ ? audio_hash_->ToString() : std::string();
}

}  // namespace media
