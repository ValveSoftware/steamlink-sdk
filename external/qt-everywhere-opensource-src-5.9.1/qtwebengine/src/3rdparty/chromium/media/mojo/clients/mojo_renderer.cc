// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_renderer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer_client.h"
#include "media/base/video_renderer_sink.h"
#include "media/mojo/clients/mojo_demuxer_stream_impl.h"
#include "media/renderers/video_overlay_factory.h"

namespace media {

MojoRenderer::MojoRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<VideoOverlayFactory> video_overlay_factory,
    VideoRendererSink* video_renderer_sink,
    mojom::RendererPtr remote_renderer)
    : task_runner_(task_runner),
      video_overlay_factory_(std::move(video_overlay_factory)),
      video_renderer_sink_(video_renderer_sink),
      remote_renderer_info_(remote_renderer.PassInterface()),
      client_binding_(this),
      media_time_interpolator_(&media_clock_) {
  DVLOG(1) << __FUNCTION__;
}

MojoRenderer::~MojoRenderer() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  CancelPendingCallbacks();
}

void MojoRenderer::Initialize(DemuxerStreamProvider* demuxer_stream_provider,
                              media::RendererClient* client,
                              const PipelineStatusCB& init_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(demuxer_stream_provider);

  if (encountered_error_) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(init_cb, PIPELINE_ERROR_INITIALIZATION_FAILED));
    return;
  }

  demuxer_stream_provider_ = demuxer_stream_provider;
  init_cb_ = init_cb;

  switch (demuxer_stream_provider_->GetType()) {
    case DemuxerStreamProvider::Type::STREAM:
      InitializeRendererFromStreams(client);
      break;
    case DemuxerStreamProvider::Type::URL:
      InitializeRendererFromUrl(client);
      break;
  }
}

void MojoRenderer::InitializeRendererFromStreams(
    media::RendererClient* client) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Create audio and video mojom::DemuxerStream and bind its lifetime to
  // the pipe.
  DemuxerStream* const audio =
      demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* const video =
      demuxer_stream_provider_->GetStream(DemuxerStream::VIDEO);

  mojom::DemuxerStreamPtr audio_stream;
  if (audio) {
    audio_stream_.reset(
        new MojoDemuxerStreamImpl(audio, GetProxy(&audio_stream)));
    // Using base::Unretained(this) is safe because |this| owns
    // |audio_stream_|, and the error handler can't be invoked once
    // |audio_stream_| is destroyed.
    audio_stream_->set_connection_error_handler(
        base::Bind(&MojoRenderer::OnDemuxerStreamConnectionError,
                   base::Unretained(this), DemuxerStream::AUDIO));
  }

  mojom::DemuxerStreamPtr video_stream;
  if (video) {
    video_stream_.reset(
        new MojoDemuxerStreamImpl(video, GetProxy(&video_stream)));
    // Using base::Unretained(this) is safe because |this| owns
    // |video_stream_|, and the error handler can't be invoked once
    // |video_stream_| is destroyed.
    video_stream_->set_connection_error_handler(
        base::Bind(&MojoRenderer::OnDemuxerStreamConnectionError,
                   base::Unretained(this), DemuxerStream::VIDEO));
  }

  BindRemoteRendererIfNeeded();

  mojom::RendererClientAssociatedPtrInfo client_ptr_info;
  client_binding_.Bind(&client_ptr_info, remote_renderer_.associated_group());

  // Using base::Unretained(this) is safe because |this| owns
  // |remote_renderer_|, and the callback won't be dispatched if
  // |remote_renderer_| is destroyed.
  remote_renderer_->Initialize(
      std::move(client_ptr_info), std::move(audio_stream),
      std::move(video_stream), base::nullopt, base::nullopt,
      base::Bind(&MojoRenderer::OnInitialized, base::Unretained(this), client));
}

void MojoRenderer::InitializeRendererFromUrl(media::RendererClient* client) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  BindRemoteRendererIfNeeded();

  mojom::RendererClientAssociatedPtrInfo client_ptr_info;
  client_binding_.Bind(&client_ptr_info, remote_renderer_.associated_group());

  MediaUrlParams url_params = demuxer_stream_provider_->GetMediaUrlParams();

  // Using base::Unretained(this) is safe because |this| owns
  // |remote_renderer_|, and the callback won't be dispatched if
  // |remote_renderer_| is destroyed.
  remote_renderer_->Initialize(
      std::move(client_ptr_info), mojom::DemuxerStreamPtr(),
      mojom::DemuxerStreamPtr(), url_params.media_url,
      url_params.first_party_for_cookies,
      base::Bind(&MojoRenderer::OnInitialized, base::Unretained(this), client));
}

void MojoRenderer::SetCdm(CdmContext* cdm_context,
                          const CdmAttachedCB& cdm_attached_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(cdm_context);
  DCHECK(!cdm_attached_cb.is_null());
  DCHECK(cdm_attached_cb_.is_null());

  if (encountered_error_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(cdm_attached_cb, false));
    return;
  }

  int32_t cdm_id = cdm_context->GetCdmId();
  if (cdm_id == CdmContext::kInvalidCdmId) {
    DVLOG(2) << "MojoRenderer only works with remote CDMs but the CDM ID "
                "is invalid.";
    task_runner_->PostTask(FROM_HERE, base::Bind(cdm_attached_cb, false));
    return;
  }

  BindRemoteRendererIfNeeded();

  cdm_attached_cb_ = cdm_attached_cb;
  remote_renderer_->SetCdm(
      cdm_id, base::Bind(&MojoRenderer::OnCdmAttached, base::Unretained(this)));
}

void MojoRenderer::Flush(const base::Closure& flush_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());
  DCHECK(!flush_cb.is_null());
  DCHECK(flush_cb_.is_null());

  if (encountered_error_) {
    task_runner_->PostTask(FROM_HERE, flush_cb);
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    if (media_time_interpolator_.interpolating())
      media_time_interpolator_.StopInterpolating();
  }

  flush_cb_ = flush_cb;
  remote_renderer_->Flush(
      base::Bind(&MojoRenderer::OnFlushed, base::Unretained(this)));
}

void MojoRenderer::StartPlayingFrom(base::TimeDelta time) {
  DVLOG(2) << __FUNCTION__ << "(" << time << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

  {
    base::AutoLock auto_lock(lock_);
    media_time_interpolator_.SetBounds(time, time, media_clock_.NowTicks());
    media_time_interpolator_.StartInterpolating();
  }

  remote_renderer_->StartPlayingFrom(time);
}

void MojoRenderer::SetPlaybackRate(double playback_rate) {
  DVLOG(2) << __FUNCTION__ << "(" << playback_rate << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

  remote_renderer_->SetPlaybackRate(playback_rate);

  {
    base::AutoLock auto_lock(lock_);
    media_time_interpolator_.SetPlaybackRate(playback_rate);
  }
}

void MojoRenderer::SetVolume(float volume) {
  DVLOG(2) << __FUNCTION__ << "(" << volume << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

  remote_renderer_->SetVolume(volume);
}

base::TimeDelta MojoRenderer::GetMediaTime() {
  base::AutoLock auto_lock(lock_);
  return media_time_interpolator_.GetInterpolatedTime();
}

bool MojoRenderer::HasAudio() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

  if (demuxer_stream_provider_->GetType() == DemuxerStreamProvider::Type::URL) {
    NOTIMPLEMENTED();
    return false;
  }

  return !!demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO);
}

bool MojoRenderer::HasVideo() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

  if (demuxer_stream_provider_->GetType() == DemuxerStreamProvider::Type::URL) {
    NOTIMPLEMENTED();
    return false;
  }

  return !!demuxer_stream_provider_->GetStream(DemuxerStream::VIDEO);
}

void MojoRenderer::OnTimeUpdate(base::TimeDelta time,
                                base::TimeDelta max_time,
                                base::TimeTicks capture_time) {
  DVLOG(4) << __FUNCTION__ << "(" << time << ", " << max_time << ", "
           << capture_time << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  media_time_interpolator_.SetBounds(time, max_time, capture_time);
}

void MojoRenderer::OnBufferingStateChange(BufferingState state) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnBufferingStateChange(state);
}

void MojoRenderer::OnEnded() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnEnded();
}

void MojoRenderer::InitiateScopedSurfaceRequest(
    const ReceiveSurfaceRequestTokenCB& receive_request_token_cb) {
  DVLOG(1) << __FUNCTION__;

  remote_renderer_->InitiateScopedSurfaceRequest(receive_request_token_cb);
}

void MojoRenderer::OnError() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(init_cb_.is_null());

  encountered_error_ = true;

  // TODO(tim): Should we plumb error code from remote renderer?
  // http://crbug.com/410451.
  client_->OnError(PIPELINE_ERROR_DECODE);
}

void MojoRenderer::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DVLOG(2) << __FUNCTION__ << ": " << size.ToString();
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (video_overlay_factory_) {
    video_renderer_sink_->PaintSingleFrame(
        video_overlay_factory_->CreateFrame(size));
  }
  client_->OnVideoNaturalSizeChange(size);
}

void MojoRenderer::OnDurationChange(base::TimeDelta duration) {
  DVLOG(2) << __FUNCTION__ << ": duration" << duration;
  client_->OnDurationChange(duration);
}

void MojoRenderer::OnVideoOpacityChange(bool opaque) {
  DVLOG(2) << __FUNCTION__ << ": " << opaque;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnVideoOpacityChange(opaque);
}

void MojoRenderer::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnStatisticsUpdate(stats);
}

void MojoRenderer::OnWaitingForDecryptionKey() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnWaitingForDecryptionKey();
}

void MojoRenderer::OnConnectionError() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  encountered_error_ = true;
  CancelPendingCallbacks();

  if (client_)
    client_->OnError(PIPELINE_ERROR_DECODE);
}

void MojoRenderer::OnDemuxerStreamConnectionError(DemuxerStream::Type type) {
  DVLOG(1) << __FUNCTION__ << ": " << type;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (type == DemuxerStream::AUDIO) {
    audio_stream_.reset();
  } else if (type == DemuxerStream::VIDEO) {
    video_stream_.reset();
  } else {
    NOTREACHED() << "Unexpected demuxer stream type: " << type;
  }
}

void MojoRenderer::BindRemoteRendererIfNeeded() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // If |remote_renderer_| has already been bound, do nothing.
  // Note that after Bind() is called, |remote_renderer_| is always bound even
  // after connection error.
  if (remote_renderer_.is_bound())
    return;

  // Bind |remote_renderer_| to the |task_runner_|.
  remote_renderer_.Bind(std::move(remote_renderer_info_));

  // Otherwise, set an error handler to catch the connection error.
  // Using base::Unretained(this) is safe because |this| owns
  // |remote_renderer_|, and the error handler can't be invoked once
  // |remote_renderer_| is destroyed.
  remote_renderer_.set_connection_error_handler(
      base::Bind(&MojoRenderer::OnConnectionError, base::Unretained(this)));
}

void MojoRenderer::OnInitialized(media::RendererClient* client, bool success) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!init_cb_.is_null());

  // Only set |client_| after initialization succeeded. No client methods should
  // be called before this.
  if (success)
    client_ = client;

  base::ResetAndReturn(&init_cb_).Run(
      success ? PIPELINE_OK : PIPELINE_ERROR_INITIALIZATION_FAILED);
}

void MojoRenderer::OnFlushed() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!flush_cb_.is_null());

  base::ResetAndReturn(&flush_cb_).Run();
}

void MojoRenderer::OnCdmAttached(bool success) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!cdm_attached_cb_.is_null());

  base::ResetAndReturn(&cdm_attached_cb_).Run(success);
}

void MojoRenderer::CancelPendingCallbacks() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);

  if (!flush_cb_.is_null())
    base::ResetAndReturn(&flush_cb_).Run();

  if (!cdm_attached_cb_.is_null())
    base::ResetAndReturn(&cdm_attached_cb_).Run(false);
}

}  // namespace media
