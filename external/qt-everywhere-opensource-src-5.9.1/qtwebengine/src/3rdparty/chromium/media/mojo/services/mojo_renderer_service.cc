// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_service.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_keys.h"
#include "media/base/media_url_demuxer.h"
#include "media/base/renderer.h"
#include "media/base/video_renderer_sink.h"
#include "media/mojo/services/demuxer_stream_provider_shim.h"
#include "media/mojo/services/mojo_cdm_service_context.h"

namespace media {

// Time interval to update media time.
const int kTimeUpdateIntervalMs = 50;

// static
mojo::StrongBindingPtr<mojom::Renderer> MojoRendererService::Create(
    base::WeakPtr<MojoCdmServiceContext> mojo_cdm_service_context,
    scoped_refptr<AudioRendererSink> audio_sink,
    std::unique_ptr<VideoRendererSink> video_sink,
    std::unique_ptr<media::Renderer> renderer,
    InitiateSurfaceRequestCB initiate_surface_request_cb,
    mojo::InterfaceRequest<mojom::Renderer> request) {
  MojoRendererService* service = new MojoRendererService(
      mojo_cdm_service_context, std::move(audio_sink), std::move(video_sink),
      std::move(renderer), initiate_surface_request_cb);

  mojo::StrongBindingPtr<mojom::Renderer> binding =
      mojo::MakeStrongBinding<mojom::Renderer>(base::WrapUnique(service),
                                               std::move(request));

  service->binding_ = binding;

  return binding;
}

MojoRendererService::MojoRendererService(
    base::WeakPtr<MojoCdmServiceContext> mojo_cdm_service_context,
    scoped_refptr<AudioRendererSink> audio_sink,
    std::unique_ptr<VideoRendererSink> video_sink,
    std::unique_ptr<media::Renderer> renderer,
    InitiateSurfaceRequestCB initiate_surface_request_cb)
    : mojo_cdm_service_context_(mojo_cdm_service_context),
      state_(STATE_UNINITIALIZED),
      playback_rate_(0),
      audio_sink_(std::move(audio_sink)),
      video_sink_(std::move(video_sink)),
      renderer_(std::move(renderer)),
      initiate_surface_request_cb_(initiate_surface_request_cb),
      weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(renderer_);

  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoRendererService::~MojoRendererService() {}

void MojoRendererService::Initialize(
    mojom::RendererClientAssociatedPtrInfo client,
    mojom::DemuxerStreamPtr audio,
    mojom::DemuxerStreamPtr video,
    const base::Optional<GURL>& media_url,
    const base::Optional<GURL>& first_party_for_cookies,
    const InitializeCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  client_.Bind(std::move(client));
  state_ = STATE_INITIALIZING;

  if (media_url == base::nullopt) {
    stream_provider_.reset(new DemuxerStreamProviderShim(
        std::move(audio), std::move(video),
        base::Bind(&MojoRendererService::OnStreamReady, weak_this_, callback)));
    return;
  }

  DCHECK(!audio);
  DCHECK(!video);
  DCHECK(!media_url.value().is_empty());
  DCHECK(first_party_for_cookies);
  stream_provider_.reset(new MediaUrlDemuxer(nullptr, media_url.value(),
                                             first_party_for_cookies.value()));
  renderer_->Initialize(
      stream_provider_.get(), this,
      base::Bind(&MojoRendererService::OnRendererInitializeDone, weak_this_,
                 callback));
}

void MojoRendererService::Flush(const FlushCallback& callback) {
  DVLOG(2) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_PLAYING);

  state_ = STATE_FLUSHING;
  CancelPeriodicMediaTimeUpdates();
  renderer_->Flush(
      base::Bind(&MojoRendererService::OnFlushCompleted, weak_this_, callback));
}

void MojoRendererService::StartPlayingFrom(base::TimeDelta time_delta) {
  DVLOG(2) << __FUNCTION__ << ": " << time_delta;
  renderer_->StartPlayingFrom(time_delta);
  SchedulePeriodicMediaTimeUpdates();
}

void MojoRendererService::SetPlaybackRate(double playback_rate) {
  DVLOG(2) << __FUNCTION__ << ": " << playback_rate;
  DCHECK(state_ == STATE_PLAYING || state_ == STATE_ERROR);
  playback_rate_ = playback_rate;
  renderer_->SetPlaybackRate(playback_rate);
}

void MojoRendererService::SetVolume(float volume) {
  renderer_->SetVolume(volume);
}

void MojoRendererService::SetCdm(int32_t cdm_id,
                                 const SetCdmCallback& callback) {
  if (!mojo_cdm_service_context_) {
    DVLOG(1) << "CDM service context not available.";
    callback.Run(false);
    return;
  }

  scoped_refptr<MediaKeys> cdm = mojo_cdm_service_context_->GetCdm(cdm_id);
  if (!cdm) {
    DVLOG(1) << "CDM not found: " << cdm_id;
    callback.Run(false);
    return;
  }

  CdmContext* cdm_context = cdm->GetCdmContext();
  if (!cdm_context) {
    DVLOG(1) << "CDM context not available: " << cdm_id;
    callback.Run(false);
    return;
  }

  renderer_->SetCdm(cdm_context, base::Bind(&MojoRendererService::OnCdmAttached,
                                            weak_this_, cdm, callback));
}

void MojoRendererService::OnError(PipelineStatus error) {
  DVLOG(1) << __FUNCTION__ << "(" << error << ")";
  state_ = STATE_ERROR;
  client_->OnError();
}

void MojoRendererService::OnEnded() {
  DVLOG(1) << __FUNCTION__;
  CancelPeriodicMediaTimeUpdates();
  client_->OnEnded();
}

void MojoRendererService::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DVLOG(3) << __FUNCTION__;
  client_->OnStatisticsUpdate(stats);
}

void MojoRendererService::OnBufferingStateChange(BufferingState state) {
  DVLOG(2) << __FUNCTION__ << "(" << state << ")";
  client_->OnBufferingStateChange(state);
}

void MojoRendererService::OnWaitingForDecryptionKey() {
  DVLOG(1) << __FUNCTION__;
  client_->OnWaitingForDecryptionKey();
}

void MojoRendererService::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DVLOG(2) << __FUNCTION__ << "(" << size.ToString() << ")";
  client_->OnVideoNaturalSizeChange(size);
}

void MojoRendererService::OnDurationChange(base::TimeDelta duration) {
  client_->OnDurationChange(duration);
}

void MojoRendererService::OnVideoOpacityChange(bool opaque) {
  DVLOG(2) << __FUNCTION__ << "(" << opaque << ")";
  client_->OnVideoOpacityChange(opaque);
}

void MojoRendererService::OnStreamReady(
    const base::Callback<void(bool)>& callback) {
  DCHECK_EQ(state_, STATE_INITIALIZING);

  renderer_->Initialize(
      stream_provider_.get(), this,
      base::Bind(&MojoRendererService::OnRendererInitializeDone, weak_this_,
                 callback));
}

void MojoRendererService::OnRendererInitializeDone(
    const base::Callback<void(bool)>& callback,
    PipelineStatus status) {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_INITIALIZING);

  if (status != PIPELINE_OK) {
    state_ = STATE_ERROR;
    callback.Run(false);
    return;
  }

  state_ = STATE_PLAYING;
  callback.Run(true);
}

void MojoRendererService::UpdateMediaTime(bool force) {
  const base::TimeDelta media_time = renderer_->GetMediaTime();
  if (!force && media_time == last_media_time_)
    return;

  base::TimeDelta max_time = media_time;
  // Allow some slop to account for delays in scheduling time update tasks.
  if (time_update_timer_.IsRunning() && (playback_rate_ > 0))
    max_time += base::TimeDelta::FromMilliseconds(2 * kTimeUpdateIntervalMs);

  client_->OnTimeUpdate(media_time, max_time, base::TimeTicks::Now());
  last_media_time_ = media_time;
}

void MojoRendererService::CancelPeriodicMediaTimeUpdates() {
  DVLOG(2) << __FUNCTION__;

  time_update_timer_.Stop();
  UpdateMediaTime(false);
}

void MojoRendererService::SchedulePeriodicMediaTimeUpdates() {
  DVLOG(2) << __FUNCTION__;

  UpdateMediaTime(true);
  time_update_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeUpdateIntervalMs),
      base::Bind(&MojoRendererService::UpdateMediaTime, weak_this_, false));
}

void MojoRendererService::OnFlushCompleted(const FlushCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_FLUSHING);
  state_ = STATE_PLAYING;
  callback.Run();
}

void MojoRendererService::OnCdmAttached(
    scoped_refptr<MediaKeys> cdm,
    const base::Callback<void(bool)>& callback,
    bool success) {
  DVLOG(1) << __FUNCTION__ << "(" << success << ")";

  if (success)
    cdm_ = cdm;

  callback.Run(success);
}

void MojoRendererService::InitiateScopedSurfaceRequest(
    const InitiateScopedSurfaceRequestCallback& callback) {
  if (initiate_surface_request_cb_.is_null()) {
    // |renderer_| is likely not of type MediaPlayerRenderer.
    // This is an unexpected call, and the connection should be closed.
    mojo::ReportBadMessage("Unexpected call to InitiateScopedSurfaceRequest.");
    binding_->Close();
    return;
  }

  callback.Run(initiate_surface_request_cb_.Run());
}
}  // namespace media
