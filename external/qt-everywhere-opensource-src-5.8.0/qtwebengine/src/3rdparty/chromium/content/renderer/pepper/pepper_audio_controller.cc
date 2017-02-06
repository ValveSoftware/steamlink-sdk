// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_audio_controller.h"

#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/ppb_audio_impl.h"
#include "content/renderer/render_frame_impl.h"

namespace content {

PepperAudioController::PepperAudioController(
    PepperPluginInstanceImpl* instance)
    : instance_(instance) {
  DCHECK(instance_);
}

PepperAudioController::~PepperAudioController() {
  if (instance_)
    OnPepperInstanceDeleted();
}

void PepperAudioController::AddInstance(PPB_Audio_Impl* audio) {
  if (!instance_)
    return;
  if (ppb_audios_.count(audio))
    return;

  if (ppb_audios_.empty()) {
    RenderFrameImpl* render_frame = instance_->render_frame();
    if (render_frame)
      render_frame->PepperStartsPlayback(instance_);
  }

  ppb_audios_.insert(audio);
}

void PepperAudioController::RemoveInstance(PPB_Audio_Impl* audio) {
  if (!instance_)
    return;
  if (!ppb_audios_.count(audio))
    return;

  ppb_audios_.erase(audio);

  if (ppb_audios_.empty())
    NotifyPlaybackStopsOnEmpty();
}

void PepperAudioController::SetVolume(double volume) {
  if (!instance_)
    return;

  for (auto* ppb_audio : ppb_audios_)
    ppb_audio->SetVolume(volume);
}

void PepperAudioController::OnPepperInstanceDeleted() {
  DCHECK(instance_);

  if (!ppb_audios_.empty())
    NotifyPlaybackStopsOnEmpty();

  ppb_audios_.clear();
  instance_ = nullptr;
}

void PepperAudioController::NotifyPlaybackStopsOnEmpty() {
  DCHECK(instance_);

  RenderFrameImpl* render_frame = instance_->render_frame();
  if (render_frame)
    render_frame->PepperStopsPlayback(instance_);
}

}  // namespace content
