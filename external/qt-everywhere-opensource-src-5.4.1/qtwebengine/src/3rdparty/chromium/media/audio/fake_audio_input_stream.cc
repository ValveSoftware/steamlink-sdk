// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_input_stream.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"

using base::TimeTicks;
using base::TimeDelta;

namespace media {

namespace {

// These values are based on experiments for local-to-local
// PeerConnection to demonstrate audio/video synchronization.
const int kBeepDurationMilliseconds = 20;
const int kBeepFrequency = 400;

// Intervals between two automatic beeps.
const int kAutomaticBeepIntervalInMs = 500;

// Automatic beep will be triggered every |kAutomaticBeepIntervalInMs| unless
// users explicitly call BeepOnce(), which will disable the automatic beep.
struct BeepContext {
  BeepContext() : beep_once(false), automatic(true) {}
  base::Lock beep_lock;
  bool beep_once;
  bool automatic;
};

static base::LazyInstance<BeepContext> g_beep_context =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

AudioInputStream* FakeAudioInputStream::MakeFakeStream(
    AudioManagerBase* manager,
    const AudioParameters& params) {
  return new FakeAudioInputStream(manager, params);
}

FakeAudioInputStream::FakeAudioInputStream(AudioManagerBase* manager,
                                           const AudioParameters& params)
    : audio_manager_(manager),
      callback_(NULL),
      buffer_size_((params.channels() * params.bits_per_sample() *
                    params.frames_per_buffer()) /
                   8),
      params_(params),
      thread_("FakeAudioRecordingThread"),
      callback_interval_(base::TimeDelta::FromMilliseconds(
          (params.frames_per_buffer() * 1000) / params.sample_rate())),
      beep_duration_in_buffers_(kBeepDurationMilliseconds *
                                params.sample_rate() /
                                params.frames_per_buffer() /
                                1000),
      beep_generated_in_buffers_(0),
      beep_period_in_frames_(params.sample_rate() / kBeepFrequency),
      frames_elapsed_(0),
      audio_bus_(AudioBus::Create(params)) {
}

FakeAudioInputStream::~FakeAudioInputStream() {}

bool FakeAudioInputStream::Open() {
  buffer_.reset(new uint8[buffer_size_]);
  memset(buffer_.get(), 0, buffer_size_);
  audio_bus_->Zero();
  return true;
}

void FakeAudioInputStream::Start(AudioInputCallback* callback)  {
  DCHECK(!thread_.IsRunning());
  DCHECK(!callback_);
  callback_ = callback;
  last_callback_time_ = TimeTicks::Now();
  thread_.Start();
  thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeAudioInputStream::DoCallback, base::Unretained(this)),
      callback_interval_);
}

void FakeAudioInputStream::DoCallback() {
  DCHECK(callback_);

  const TimeTicks now = TimeTicks::Now();
  base::TimeDelta next_callback_time =
      last_callback_time_ + callback_interval_ * 2 - now;

  // If we are falling behind, try to catch up as much as we can in the next
  // callback.
  if (next_callback_time < base::TimeDelta())
    next_callback_time = base::TimeDelta();

  // Accumulate the time from the last beep.
  interval_from_last_beep_ += now - last_callback_time_;

  last_callback_time_ = now;

  memset(buffer_.get(), 0, buffer_size_);

  bool should_beep = false;
  {
    BeepContext* beep_context = g_beep_context.Pointer();
    base::AutoLock auto_lock(beep_context->beep_lock);
    if (beep_context->automatic) {
      base::TimeDelta delta = interval_from_last_beep_ -
          TimeDelta::FromMilliseconds(kAutomaticBeepIntervalInMs);
      if (delta > base::TimeDelta()) {
        should_beep = true;
        interval_from_last_beep_ = delta;
      }
    } else {
      should_beep = beep_context->beep_once;
      beep_context->beep_once = false;
    }
  }

  // If this object was instructed to generate a beep or has started to
  // generate a beep sound.
  if (should_beep || beep_generated_in_buffers_) {
    // Compute the number of frames to output high value. Then compute the
    // number of bytes based on channels and bits per channel.
    int high_frames = beep_period_in_frames_ / 2;
    int high_bytes = high_frames * params_.bits_per_sample() *
        params_.channels() / 8;

    // Separate high and low with the same number of bytes to generate a
    // square wave.
    int position = 0;
    while (position + high_bytes <= buffer_size_) {
      // Write high values first.
      memset(buffer_.get() + position, 128, high_bytes);
      // Then leave low values in the buffer with |high_bytes|.
      position += high_bytes * 2;
    }

    ++beep_generated_in_buffers_;
    if (beep_generated_in_buffers_ >= beep_duration_in_buffers_)
      beep_generated_in_buffers_ = 0;
  }

  audio_bus_->FromInterleaved(
      buffer_.get(), audio_bus_->frames(), params_.bits_per_sample() / 8);
  callback_->OnData(this, audio_bus_.get(), buffer_size_, 1.0);
  frames_elapsed_ += params_.frames_per_buffer();

  thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeAudioInputStream::DoCallback, base::Unretained(this)),
      next_callback_time);
}

void FakeAudioInputStream::Stop() {
  thread_.Stop();
  callback_ = NULL;
}

void FakeAudioInputStream::Close() {
  audio_manager_->ReleaseInputStream(this);
}

double FakeAudioInputStream::GetMaxVolume() {
  return 1.0;
}

void FakeAudioInputStream::SetVolume(double volume) {
}

double FakeAudioInputStream::GetVolume() {
  return 1.0;
}

void FakeAudioInputStream::SetAutomaticGainControl(bool enabled) {}

bool FakeAudioInputStream::GetAutomaticGainControl() {
  return true;
}

// static
void FakeAudioInputStream::BeepOnce() {
  BeepContext* beep_context = g_beep_context.Pointer();
  base::AutoLock auto_lock(beep_context->beep_lock);
  beep_context->beep_once = true;
  beep_context->automatic = false;
}

}  // namespace media
