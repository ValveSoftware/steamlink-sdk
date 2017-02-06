// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_renderer_mixer_input.h"

#include <cmath>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_pool.h"

namespace media {

AudioRendererMixerInput::AudioRendererMixerInput(
    AudioRendererMixerPool* mixer_pool,
    int owner_id,
    const std::string& device_id,
    const url::Origin& security_origin,
    AudioLatency::LatencyType latency)
    : mixer_pool_(mixer_pool),
      started_(false),
      playing_(false),
      volume_(1.0f),
      owner_id_(owner_id),
      device_id_(device_id),
      security_origin_(security_origin),
      latency_(latency),
      mixer_(nullptr),
      callback_(nullptr),
      error_cb_(base::Bind(&AudioRendererMixerInput::OnRenderError,
                           base::Unretained(this))) {
  DCHECK(mixer_pool_);
}

AudioRendererMixerInput::~AudioRendererMixerInput() {
  DCHECK(!started_);
  DCHECK(!mixer_);
}

void AudioRendererMixerInput::Initialize(
    const AudioParameters& params,
    AudioRendererSink::RenderCallback* callback) {
  DCHECK(!started_);
  DCHECK(!mixer_);
  DCHECK(callback);

  params_ = params;
  callback_ = callback;
}

void AudioRendererMixerInput::Start() {
  DCHECK(!started_);
  DCHECK(!mixer_);
  DCHECK(callback_);  // Initialized.

  started_ = true;
  mixer_ = mixer_pool_->GetMixer(owner_id_, params_, latency_, device_id_,
                                 security_origin_, nullptr);
  if (!mixer_) {
    callback_->OnRenderError();
    return;
  }

  // Note: OnRenderError() may be called immediately after this call returns.
  mixer_->AddErrorCallback(error_cb_);

  if (!pending_switch_callback_.is_null()) {
    SwitchOutputDevice(pending_switch_device_id_,
                       pending_switch_security_origin_,
                       base::ResetAndReturn(&pending_switch_callback_));
  }
}

void AudioRendererMixerInput::Stop() {
  // Stop() may be called at any time, if Pause() hasn't been called we need to
  // remove our mixer input before shutdown.
  Pause();

  if (mixer_) {
    // TODO(dalecurtis): This is required so that |callback_| isn't called after
    // Stop() by an error event since it may outlive this ref-counted object. We
    // should instead have sane ownership semantics: http://crbug.com/151051
    mixer_->RemoveErrorCallback(error_cb_);
    mixer_pool_->ReturnMixer(mixer_);
    mixer_ = nullptr;
  }

  started_ = false;

  if (!pending_switch_callback_.is_null()) {
    base::ResetAndReturn(&pending_switch_callback_)
        .Run(OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
  }
}

void AudioRendererMixerInput::Play() {
  if (playing_ || !mixer_)
    return;

  mixer_->AddMixerInput(params_, this);
  playing_ = true;
}

void AudioRendererMixerInput::Pause() {
  if (!playing_ || !mixer_)
    return;

  mixer_->RemoveMixerInput(params_, this);
  playing_ = false;
}

bool AudioRendererMixerInput::SetVolume(double volume) {
  base::AutoLock auto_lock(volume_lock_);
  volume_ = volume;
  return true;
}

OutputDeviceInfo AudioRendererMixerInput::GetOutputDeviceInfo() {
  return mixer_
             ? mixer_->GetOutputDeviceInfo()
             : mixer_pool_->GetOutputDeviceInfo(owner_id_, 0 /* session_id */,
                                                device_id_, security_origin_);
}

bool AudioRendererMixerInput::CurrentThreadIsRenderingThread() {
  return mixer_->CurrentThreadIsRenderingThread();
}

void AudioRendererMixerInput::SwitchOutputDevice(
    const std::string& device_id,
    const url::Origin& security_origin,
    const OutputDeviceStatusCB& callback) {
  if (!mixer_) {
    if (pending_switch_callback_.is_null()) {
      pending_switch_callback_ = callback;
      pending_switch_device_id_ = device_id;
      pending_switch_security_origin_ = security_origin;
    } else {
      callback.Run(OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
    }

    return;
  }

  DCHECK(pending_switch_callback_.is_null());
  if (device_id == device_id_) {
    callback.Run(OUTPUT_DEVICE_STATUS_OK);
    return;
  }

  OutputDeviceStatus new_mixer_status = OUTPUT_DEVICE_STATUS_ERROR_INTERNAL;
  AudioRendererMixer* new_mixer =
      mixer_pool_->GetMixer(owner_id_, params_, latency_, device_id,
                            security_origin, &new_mixer_status);
  if (new_mixer_status != OUTPUT_DEVICE_STATUS_OK) {
    callback.Run(new_mixer_status);
    return;
  }

  bool was_playing = playing_;
  Stop();
  device_id_ = device_id;
  security_origin_ = security_origin;
  mixer_ = new_mixer;
  mixer_->AddErrorCallback(error_cb_);
  started_ = true;

  if (was_playing)
    Play();

  callback.Run(OUTPUT_DEVICE_STATUS_OK);
}

double AudioRendererMixerInput::ProvideInput(AudioBus* audio_bus,
                                             uint32_t frames_delayed) {
  int frames_filled = callback_->Render(audio_bus, frames_delayed, 0);

  // AudioConverter expects unfilled frames to be zeroed.
  if (frames_filled < audio_bus->frames()) {
    audio_bus->ZeroFramesPartial(
        frames_filled, audio_bus->frames() - frames_filled);
  }

  // We're reading |volume_| from the audio device thread and must avoid racing
  // with the main/media thread calls to SetVolume(). See thread safety comment
  // in the header file.
  {
    base::AutoLock auto_lock(volume_lock_);
    return frames_filled > 0 ? volume_ : 0;
  }
}

void AudioRendererMixerInput::OnRenderError() {
  callback_->OnRenderError();
}

}  // namespace media
