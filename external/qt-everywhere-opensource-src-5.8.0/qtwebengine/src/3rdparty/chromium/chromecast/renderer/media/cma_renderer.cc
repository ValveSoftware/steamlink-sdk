// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/cma_renderer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/media/cma/base/balanced_media_task_runner_factory.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/demuxer_stream_adapter.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"
#include "chromecast/media/cma/pipeline/media_pipeline_client.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "chromecast/renderer/media/audio_pipeline_proxy.h"
#include "chromecast/renderer/media/media_pipeline_proxy.h"
#include "chromecast/renderer/media/video_pipeline_proxy.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer_client.h"
#include "media/base/time_delta_interpolator.h"
#include "media/base/video_renderer_sink.h"
#include "media/renderers/video_overlay_factory.h"
#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {

namespace {

// Maximum difference between audio frame PTS and video frame PTS
// for frames read from the DemuxerStream.
const base::TimeDelta kMaxDeltaFetcher(base::TimeDelta::FromMilliseconds(2000));

void MediaPipelineClientDummyCallback() {
}

}  // namespace

CmaRenderer::CmaRenderer(std::unique_ptr<MediaPipelineProxy> media_pipeline,
                         ::media::VideoRendererSink* video_renderer_sink,
                         ::media::GpuVideoAcceleratorFactories* gpu_factories)
    : media_task_runner_factory_(
          new BalancedMediaTaskRunnerFactory(kMaxDeltaFetcher)),
      media_pipeline_(std::move(media_pipeline)),
      audio_pipeline_(media_pipeline_->GetAudioPipeline()),
      video_pipeline_(media_pipeline_->GetVideoPipeline()),
      video_renderer_sink_(video_renderer_sink),
      state_(kUninitialized),
      is_pending_transition_(false),
      has_audio_(false),
      has_video_(false),
      received_audio_eos_(false),
      received_video_eos_(false),
      gpu_factories_(gpu_factories),
      time_interpolator_(
          new ::media::TimeDeltaInterpolator(&default_tick_clock_)),
      playback_rate_(1.0),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();

  time_interpolator_->SetUpperBound(base::TimeDelta());
}

CmaRenderer::~CmaRenderer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(::media::PIPELINE_ERROR_ABORT);
  else if (!flush_cb_.is_null())
    base::ResetAndReturn(&flush_cb_).Run();

  if (has_audio_ || has_video_)
    media_pipeline_->Stop();
}

void CmaRenderer::Initialize(
    ::media::DemuxerStreamProvider* demuxer_stream_provider,
    ::media::RendererClient* client,
    const ::media::PipelineStatusCB& init_cb) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, kUninitialized) << state_;
  DCHECK(!init_cb.is_null());
  DCHECK(demuxer_stream_provider->GetStream(::media::DemuxerStream::AUDIO) ||
         demuxer_stream_provider->GetStream(::media::DemuxerStream::VIDEO));

  // Deferred from ctor so as to initialise on correct thread.
  video_overlay_factory_.reset(
      new ::media::VideoOverlayFactory(gpu_factories_));

  BeginStateTransition();

  demuxer_stream_provider_ = demuxer_stream_provider;
  client_ = client;

  MediaPipelineClient media_pipeline_client;
  media_pipeline_client.error_cb =
      ::media::BindToCurrentLoop(base::Bind(&CmaRenderer::OnError, weak_this_));
  media_pipeline_client.buffering_state_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaRenderer::OnBufferingNotification, weak_this_));
  media_pipeline_client.time_update_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaRenderer::OnPlaybackTimeUpdated, weak_this_));
  media_pipeline_client.pipeline_backend_created_cb =
      base::Bind(&MediaPipelineClientDummyCallback);
  media_pipeline_client.pipeline_backend_destroyed_cb =
      base::Bind(&MediaPipelineClientDummyCallback);
  media_pipeline_->SetClient(media_pipeline_client);

  init_cb_ = init_cb;
  InitializeAudioPipeline();
}

void CmaRenderer::Flush(const base::Closure& flush_cb) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  BeginStateTransition();

  DCHECK(flush_cb_.is_null());
  flush_cb_ = flush_cb;

  if (state_ == kError) {
    OnError(::media::PIPELINE_ERROR_ABORT);
    return;
  }

  DCHECK_EQ(state_, kPlaying) << state_;
  media_pipeline_->Flush(::media::BindToCurrentLoop(
      base::Bind(&CmaRenderer::OnFlushDone, weak_this_)));

  {
    base::AutoLock auto_lock(time_interpolator_lock_);
    time_interpolator_->StopInterpolating();
  }
}

void CmaRenderer::StartPlayingFrom(base::TimeDelta time) {
  CMALOG(kLogControl) << __FUNCTION__ << ": " << time.InMilliseconds();
  DCHECK(thread_checker_.CalledOnValidThread());
  BeginStateTransition();

  if (state_ == kError) {
    client_->OnError(::media::PIPELINE_ERROR_ABORT);
    CompleteStateTransition(kError);
    return;
  }

  {
    base::AutoLock auto_lock(time_interpolator_lock_);
    time_interpolator_.reset(
        new ::media::TimeDeltaInterpolator(&default_tick_clock_));
    time_interpolator_->SetPlaybackRate(playback_rate_);
    time_interpolator_->SetBounds(time, time);
    time_interpolator_->StartInterpolating();
  }

  received_audio_eos_ = false;
  received_video_eos_ = false;

  DCHECK_EQ(state_, kFlushed) << state_;
  // Immediately update transition to playing.
  // Error case will be handled on response from host.
  media_pipeline_->StartPlayingFrom(time);
  CompleteStateTransition(kPlaying);
}

void CmaRenderer::SetPlaybackRate(double playback_rate) {
  CMALOG(kLogControl) << __FUNCTION__ << ": " << playback_rate;
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_->SetPlaybackRate(playback_rate);
  playback_rate_ = playback_rate;

  {
    base::AutoLock auto_lock(time_interpolator_lock_);
    time_interpolator_->SetPlaybackRate(playback_rate);
  }
}

void CmaRenderer::SetVolume(float volume) {
  CMALOG(kLogControl) << __FUNCTION__ << ": " << volume;
  DCHECK(thread_checker_.CalledOnValidThread());
  audio_pipeline_->SetVolume(volume);
}

base::TimeDelta CmaRenderer::GetMediaTime() {
  base::AutoLock auto_lock(time_interpolator_lock_);
  return time_interpolator_->GetInterpolatedTime();
}

bool CmaRenderer::HasAudio() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return has_audio_;
}

bool CmaRenderer::HasVideo() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return has_video_;
}

void CmaRenderer::SetCdm(::media::CdmContext* cdm_context,
                         const ::media::CdmAttachedCB& cdm_attached_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(ENABLE_BROWSER_CDMS)
  media_pipeline_->SetCdm(cdm_context->GetCdmId());
#endif
  cdm_attached_cb.Run(true);
}

void CmaRenderer::InitializeAudioPipeline() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, kUninitialized) << state_;
  DCHECK(!init_cb_.is_null());

  ::media::DemuxerStream* stream =
      demuxer_stream_provider_->GetStream(::media::DemuxerStream::AUDIO);
  ::media::PipelineStatusCB audio_initialization_done_cb =
      ::media::BindToCurrentLoop(
          base::Bind(&CmaRenderer::OnAudioPipelineInitializeDone,
                     weak_this_,
                     stream != nullptr));
  if (!stream) {
    CMALOG(kLogControl) << __FUNCTION__ << ": no audio stream, skipping init.";
    audio_initialization_done_cb.Run(::media::PIPELINE_OK);
    return;
  }

  // Receive events from the audio pipeline.
  AvPipelineClient av_pipeline_client;
  av_pipeline_client.wait_for_key_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaRenderer::OnWaitForKey, weak_this_, true));
  av_pipeline_client.eos_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaRenderer::OnEosReached, weak_this_, true));
  av_pipeline_client.playback_error_cb =
      ::media::BindToCurrentLoop(base::Bind(&CmaRenderer::OnError, weak_this_));
  av_pipeline_client.statistics_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaRenderer::OnStatisticsUpdated, weak_this_));
  audio_pipeline_->SetClient(av_pipeline_client);

  std::unique_ptr<CodedFrameProvider> frame_provider(new DemuxerStreamAdapter(
      base::ThreadTaskRunnerHandle::Get(), media_task_runner_factory_, stream));

  const ::media::AudioDecoderConfig& config = stream->audio_decoder_config();
  if (config.codec() == ::media::kCodecAAC)
    stream->EnableBitstreamConverter();

  media_pipeline_->InitializeAudio(config, std::move(frame_provider),
                                   audio_initialization_done_cb);
}

void CmaRenderer::OnAudioPipelineInitializeDone(
    bool audio_stream_present,
    ::media::PipelineStatus status) {
  CMALOG(kLogControl) << __FUNCTION__ << ": state=" << state_;
  DCHECK(thread_checker_.CalledOnValidThread());

  // OnError() may be fired at any time, even before initialization is complete.
  if (state_ == kError)
    return;

  DCHECK_EQ(state_, kUninitialized) << state_;
  DCHECK(!init_cb_.is_null());
  if (status != ::media::PIPELINE_OK) {
    base::ResetAndReturn(&init_cb_).Run(status);
    return;
  }

  has_audio_ = audio_stream_present;
  InitializeVideoPipeline();
}

void CmaRenderer::InitializeVideoPipeline() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, kUninitialized) << state_;
  DCHECK(!init_cb_.is_null());

  ::media::DemuxerStream* stream =
      demuxer_stream_provider_->GetStream(::media::DemuxerStream::VIDEO);
  ::media::PipelineStatusCB video_initialization_done_cb =
      ::media::BindToCurrentLoop(
          base::Bind(&CmaRenderer::OnVideoPipelineInitializeDone,
                     weak_this_,
                     stream != nullptr));
  if (!stream) {
    CMALOG(kLogControl) << __FUNCTION__ << ": no video stream, skipping init.";
    video_initialization_done_cb.Run(::media::PIPELINE_OK);
    return;
  }

  // Receive events from the video pipeline.
  VideoPipelineClient client;
  client.av_pipeline_client.wait_for_key_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaRenderer::OnWaitForKey, weak_this_, false));
  client.av_pipeline_client.eos_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaRenderer::OnEosReached, weak_this_, false));
  client.av_pipeline_client.playback_error_cb =
      ::media::BindToCurrentLoop(base::Bind(&CmaRenderer::OnError, weak_this_));
  client.av_pipeline_client.statistics_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaRenderer::OnStatisticsUpdated, weak_this_));
  client.natural_size_changed_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaRenderer::OnNaturalSizeChanged, weak_this_));
  video_pipeline_->SetClient(client);

  std::unique_ptr<CodedFrameProvider> frame_provider(new DemuxerStreamAdapter(
      base::ThreadTaskRunnerHandle::Get(), media_task_runner_factory_, stream));

  const ::media::VideoDecoderConfig& config = stream->video_decoder_config();
  if (config.codec() == ::media::kCodecH264)
    stream->EnableBitstreamConverter();

  std::vector<::media::VideoDecoderConfig> configs;
  configs.push_back(config);
  media_pipeline_->InitializeVideo(configs, std::move(frame_provider),
                                   video_initialization_done_cb);
}

void CmaRenderer::OnVideoPipelineInitializeDone(
    bool video_stream_present,
    ::media::PipelineStatus status) {
  CMALOG(kLogControl) << __FUNCTION__ << ": state=" << state_;
  DCHECK(thread_checker_.CalledOnValidThread());

  // OnError() may be fired at any time, even before initialization is complete.
  if (state_ == kError)
    return;

  DCHECK_EQ(state_, kUninitialized) << state_;
  DCHECK(!init_cb_.is_null());
  if (status != ::media::PIPELINE_OK) {
    base::ResetAndReturn(&init_cb_).Run(status);
    return;
  }

  has_video_ = video_stream_present;
  CompleteStateTransition(kFlushed);
  base::ResetAndReturn(&init_cb_).Run(::media::PIPELINE_OK);
}

void CmaRenderer::OnWaitForKey(bool is_audio) {
  client_->OnWaitingForDecryptionKey();
}

void CmaRenderer::OnEosReached(bool is_audio) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ != kPlaying) {
    LOG(WARNING) << __FUNCTION__ << " Ignoring a late EOS event";
    return;
  }

  if (is_audio) {
    DCHECK(!received_audio_eos_);
    received_audio_eos_ = true;
  } else {
    DCHECK(!received_video_eos_);
    received_video_eos_ = true;
  }

  bool audio_finished = !has_audio_ || received_audio_eos_;
  bool video_finished = !has_video_ || received_video_eos_;
  CMALOG(kLogControl) << __FUNCTION__ << " audio_finished=" << audio_finished
                      << " video_finished=" << video_finished;
  if (audio_finished && video_finished)
    client_->OnEnded();
}

void CmaRenderer::OnStatisticsUpdated(
    const ::media::PipelineStatistics& stats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  client_->OnStatisticsUpdate(stats);
}

void CmaRenderer::OnNaturalSizeChanged(const gfx::Size& size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  video_renderer_sink_->PaintSingleFrame(
      video_overlay_factory_->CreateFrame(size));
  client_->OnVideoNaturalSizeChange(size);
}

void CmaRenderer::OnPlaybackTimeUpdated(base::TimeDelta time,
                                        base::TimeDelta max_time,
                                        base::TimeTicks capture_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ != kPlaying) {
    LOG(WARNING) << "Ignoring a late time update";
    return;
  }

  // TODO(halliwell): arguably, TimeDeltaInterpolator::SetBounds should perform
  // this calculation to avoid calling TimeTicks::Now twice (it's slower and has
  // potential accuracy problems).
  base::TimeDelta lower_bound =
      std::min(max_time, time + base::TimeTicks::Now() - capture_time);

  base::AutoLock auto_lock(time_interpolator_lock_);
  time_interpolator_->SetBounds(lower_bound, max_time);
}

void CmaRenderer::OnBufferingNotification(
    ::media::BufferingState buffering_state) {
  CMALOG(kLogControl) << __FUNCTION__ << ": state=" << state_
                      << ", buffering=" << buffering_state;
  DCHECK(thread_checker_.CalledOnValidThread());
  client_->OnBufferingStateChange(buffering_state);
}

void CmaRenderer::OnFlushDone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == kError) {
    // If OnError was called while the flush was in progress,
    // |flush_cb_| must be null.
    DCHECK(flush_cb_.is_null());
    return;
  }

  CompleteStateTransition(kFlushed);
  base::ResetAndReturn(&flush_cb_).Run();
}

void CmaRenderer::OnError(::media::PipelineStatus error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(::media::PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";
  LOG(ERROR) << "CMA error encountered: " << error;

  State old_state = state_;
  CompleteStateTransition(kError);

  if (old_state != kError) {
    if (!init_cb_.is_null()) {
      base::ResetAndReturn(&init_cb_).Run(error);
      return;
    }
    client_->OnError(error);
  }

  // After OnError() returns, the pipeline may destroy |this|.
  if (!flush_cb_.is_null())
    base::ResetAndReturn(&flush_cb_).Run();
}

void CmaRenderer::BeginStateTransition() {
  DCHECK(!is_pending_transition_) << state_;
  is_pending_transition_ = true;
}

void CmaRenderer::CompleteStateTransition(State new_state) {
  state_ = new_state;
  is_pending_transition_ = false;
}

}  // namespace media
}  // namespace chromecast
