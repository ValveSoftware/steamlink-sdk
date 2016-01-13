// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_audio_renderer_sink.h"

#include "base/logging.h"

namespace media {

FakeAudioRendererSink::FakeAudioRendererSink()
    : state_(kUninitialized),
      callback_(NULL) {
}

FakeAudioRendererSink::~FakeAudioRendererSink() {
  DCHECK(!callback_);
}

void FakeAudioRendererSink::Initialize(const AudioParameters& params,
                                       RenderCallback* callback) {
  DCHECK_EQ(state_, kUninitialized);
  DCHECK(!callback_);
  DCHECK(callback);

  callback_ = callback;
  ChangeState(kInitialized);
}

void FakeAudioRendererSink::Start() {
  DCHECK_EQ(state_, kInitialized);
  ChangeState(kStarted);
}

void FakeAudioRendererSink::Stop() {
  callback_ = NULL;
  ChangeState(kStopped);
}

void FakeAudioRendererSink::Pause() {
  DCHECK(state_ == kStarted || state_ == kPlaying) << "state_ " << state_;
  ChangeState(kPaused);
}

void FakeAudioRendererSink::Play() {
  DCHECK(state_ == kStarted || state_ == kPaused) << "state_ " << state_;
  DCHECK_EQ(state_, kPaused);
  ChangeState(kPlaying);
}

bool FakeAudioRendererSink::SetVolume(double volume) {
  return true;
}

bool FakeAudioRendererSink::Render(AudioBus* dest, int audio_delay_milliseconds,
                                   int* frames_written) {
  if (state_ != kPlaying)
    return false;

  *frames_written = callback_->Render(dest, audio_delay_milliseconds);
  return true;
}

void FakeAudioRendererSink::OnRenderError() {
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kStopped);

  callback_->OnRenderError();
}

void FakeAudioRendererSink::ChangeState(State new_state) {
  static const char* kStateNames[] = {
    "kUninitialized",
    "kInitialized",
    "kStarted",
    "kPaused",
    "kPlaying",
    "kStopped"
  };

  DVLOG(1) << __FUNCTION__ << " : "
           << kStateNames[state_] << " -> " << kStateNames[new_state];
  state_ = new_state;
}

}  // namespace media
