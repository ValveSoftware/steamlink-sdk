// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_renderer_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "media/remoting/remoting_cdm_context.h"

namespace media {

RemotingRendererController::RemotingRendererController(
    scoped_refptr<RemotingSourceImpl> remoting_source)
    : remoting_source_(remoting_source), weak_factory_(this) {
  remoting_source_->AddClient(this);
}

RemotingRendererController::~RemotingRendererController() {
  DCHECK(thread_checker_.CalledOnValidThread());
  remoting_source_->RemoveClient(this);
}

void RemotingRendererController::OnStarted(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (success) {
    VLOG(1) << "Remoting started successively.";
    if (remote_rendering_started_) {
      DCHECK(!switch_renderer_cb_.is_null());
      switch_renderer_cb_.Run();
    } else {
      remoting_source_->StopRemoting(this);
    }
  } else {
    VLOG(1) << "Failed to start remoting.";
    remote_rendering_started_ = false;
  }
}

void RemotingRendererController::OnSessionStateChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "OnSessionStateChanged: " << remoting_source_->state();
  UpdateAndMaybeSwitch();
}

void RemotingRendererController::OnEnteredFullscreen() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_fullscreen_ = true;
  UpdateAndMaybeSwitch();
}

void RemotingRendererController::OnExitedFullscreen() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_fullscreen_ = false;
  UpdateAndMaybeSwitch();
}

void RemotingRendererController::OnSetCdm(CdmContext* cdm_context) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto* remoting_cdm_context = RemotingCdmContext::From(cdm_context);
  if (!remoting_cdm_context)
    return;

  remoting_source_->RemoveClient(this);
  remoting_source_ = remoting_cdm_context->GetRemotingSource();
  remoting_source_->AddClient(this);  // Calls OnSessionStateChanged().
  UpdateAndMaybeSwitch();
}

void RemotingRendererController::SetSwitchRendererCallback(
    const base::Closure& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!cb.is_null());

  switch_renderer_cb_ = cb;
  UpdateAndMaybeSwitch();
}

base::WeakPtr<remoting::RpcBroker> RemotingRendererController::GetRpcBroker()
    const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return remoting_source_->GetRpcBroker()->GetWeakPtr();
}

void RemotingRendererController::StartDataPipe(
    std::unique_ptr<mojo::DataPipe> audio_data_pipe,
    std::unique_ptr<mojo::DataPipe> video_data_pipe,
    const RemotingSourceImpl::DataPipeStartCallback& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  remoting_source_->StartDataPipe(std::move(audio_data_pipe),
                                  std::move(video_data_pipe), done_callback);
}

void RemotingRendererController::OnMetadataChanged(
    const PipelineMetadata& metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());

  pipeline_metadata_ = metadata;

  is_encrypted_ = false;
  if (has_video()) {
    video_decoder_config_ = metadata.video_decoder_config;
    is_encrypted_ |= video_decoder_config_.is_encrypted();
  }
  if (has_audio()) {
    audio_decoder_config_ = metadata.audio_decoder_config;
    is_encrypted_ |= audio_decoder_config_.is_encrypted();
  }
  UpdateAndMaybeSwitch();
}

bool RemotingRendererController::IsVideoCodecSupported() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_video());

  switch (video_decoder_config_.codec()) {
    case VideoCodec::kCodecH264:
    case VideoCodec::kCodecVP8:
      return true;
    default:
      VLOG(2) << "Remoting does not support video codec: "
              << video_decoder_config_.codec();
      return false;
  }
}

bool RemotingRendererController::IsAudioCodecSupported() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_audio());

  switch (audio_decoder_config_.codec()) {
    case AudioCodec::kCodecAAC:
    case AudioCodec::kCodecMP3:
    case AudioCodec::kCodecPCM:
    case AudioCodec::kCodecVorbis:
    case AudioCodec::kCodecFLAC:
    case AudioCodec::kCodecAMR_NB:
    case AudioCodec::kCodecAMR_WB:
    case AudioCodec::kCodecPCM_MULAW:
    case AudioCodec::kCodecGSM_MS:
    case AudioCodec::kCodecPCM_S16BE:
    case AudioCodec::kCodecPCM_S24BE:
    case AudioCodec::kCodecOpus:
    case AudioCodec::kCodecEAC3:
    case AudioCodec::kCodecPCM_ALAW:
    case AudioCodec::kCodecALAC:
    case AudioCodec::kCodecAC3:
      return true;
    default:
      VLOG(2) << "Remoting does not support audio codec: "
              << audio_decoder_config_.codec();
      return false;
  }
}

bool RemotingRendererController::ShouldBeRemoting() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (switch_renderer_cb_.is_null())
    return false;  // No way to switch to a RemotingRenderImpl.

  const RemotingSessionState state = remoting_source_->state();
  if (is_encrypted_) {
    // Due to technical limitations when playing encrypted content, once a
    // remoting session has been started, always return true here to indicate
    // that the RemotingRendererImpl should be used. In the stopped states,
    // RemotingRendererImpl will display an interstitial to notify the user that
    // local rendering cannot be resumed.
    return state == RemotingSessionState::SESSION_STARTED ||
           state == RemotingSessionState::SESSION_STOPPING ||
           state == RemotingSessionState::SESSION_PERMANENTLY_STOPPED;
  }

  switch (state) {
    case SESSION_UNAVAILABLE:
      return false;  // Cannot remote media without a remote sink.
    case SESSION_CAN_START:
    case SESSION_STARTING:
    case SESSION_STARTED:
      break;  // Media remoting is possible, assuming other requirments are met.
    case SESSION_STOPPING:
    case SESSION_PERMANENTLY_STOPPED:
      return false;  // Use local rendering after stopping remoting.
  }
  if ((!has_audio() && !has_video()) ||
      (has_video() && !IsVideoCodecSupported()) ||
      (has_audio() && !IsAudioCodecSupported()))
    return false;

  // Normally, entering fullscreen is the signal that starts remote rendering.
  // However, current technical limitations require encrypted content be remoted
  // without waiting for a user signal.
  return is_fullscreen_;
}

void RemotingRendererController::UpdateAndMaybeSwitch() {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool should_be_remoting = ShouldBeRemoting();

  if (remote_rendering_started_ == should_be_remoting)
    return;

  // Switch between local renderer and remoting renderer.
  remote_rendering_started_ = should_be_remoting;

  if (remote_rendering_started_) {
    DCHECK(!switch_renderer_cb_.is_null());
    if (remoting_source_->state() ==
        RemotingSessionState::SESSION_PERMANENTLY_STOPPED) {
      switch_renderer_cb_.Run();
      return;
    }
    // |switch_renderer_cb_.Run()| will be called after remoting is started
    // successfully.
    remoting_source_->StartRemoting(this);
  } else {
    // For encrypted content, it's only valid to switch to remoting renderer,
    // and never back to the local renderer. The RemotingCdmController will
    // force-stop the session when remoting has ended; so no need to call
    // StopRemoting() from here.
    DCHECK(!is_encrypted_);
    switch_renderer_cb_.Run();
    remoting_source_->StopRemoting(this);
  }
}

}  // namespace media
