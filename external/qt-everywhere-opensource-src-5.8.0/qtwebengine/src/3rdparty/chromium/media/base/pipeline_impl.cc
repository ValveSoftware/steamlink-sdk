// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/demuxer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/base/serial_runner.h"
#include "media/base/text_renderer.h"
#include "media/base/text_track_config.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"

static const double kDefaultPlaybackRate = 0.0;
static const float kDefaultVolume = 1.0f;

namespace media {

class PipelineImpl::RendererWrapper : public DemuxerHost,
                                      public RendererClient {
 public:
  RendererWrapper(scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
                  scoped_refptr<MediaLog> media_log);
  ~RendererWrapper() final;

  void Start(Demuxer* demuxer,
             std::unique_ptr<Renderer> renderer,
             std::unique_ptr<TextRenderer> text_renderer,
             base::WeakPtr<PipelineImpl> weak_pipeline);
  void Stop(const base::Closure& stop_cb);
  void Seek(base::TimeDelta time);
  void Suspend();
  void Resume(std::unique_ptr<Renderer> renderer, base::TimeDelta time);
  void SetPlaybackRate(double playback_rate);
  void SetVolume(float volume);
  base::TimeDelta GetMediaTime() const;
  Ranges<base::TimeDelta> GetBufferedTimeRanges() const;
  bool DidLoadingProgress();
  PipelineStatistics GetStatistics() const;
  void SetCdm(CdmContext* cdm_context, const CdmAttachedCB& cdm_attached_cb);

 private:
  // Contains state shared between main and media thread.
  // Main thread can only read. Media thread can both - read and write.
  // So it is not necessary to lock when reading from the media thread.
  // This struct should only contain state that is not immediately needed by
  // PipelineClient and can be cached on the media thread until queried.
  // Alternatively we could cache it on the main thread by posting the
  // notification to the main thread. But some of the state change notifications
  // (OnStatisticsUpdate and OnBufferedTimeRangesChanged) arrive much more
  // frequently than needed. Posting all those notifications to the main thread
  // causes performance issues: crbug.com/619975.
  struct SharedState {
    // TODO(scherkus): Enforce that Renderer is only called on a single thread,
    // even for accessing media time http://crbug.com/370634
    std::unique_ptr<Renderer> renderer;

    // True when OnBufferedTimeRangesChanged() has been called more recently
    // than DidLoadingProgress().
    bool did_loading_progress = false;

    // Amount of available buffered data as reported by Demuxer.
    Ranges<base::TimeDelta> buffered_time_ranges;

    // Accumulated statistics reported by the renderer.
    PipelineStatistics statistics;

    // The media timestamp to return while the pipeline is suspended.
    // Otherwise set to kNoTimestamp().
    base::TimeDelta suspend_timestamp = kNoTimestamp();
  };

  // DemuxerHost implementaion.
  void OnBufferedTimeRangesChanged(const Ranges<base::TimeDelta>& ranges) final;
  void SetDuration(base::TimeDelta duration) final;
  void OnDemuxerError(PipelineStatus error) final;
  void AddTextStream(DemuxerStream* text_stream,
                     const TextTrackConfig& config) final;
  void RemoveTextStream(DemuxerStream* text_stream) final;

  // RendererClient implementation.
  void OnError(PipelineStatus error) final;
  void OnEnded() final;
  void OnStatisticsUpdate(const PipelineStatistics& stats) final;
  void OnBufferingStateChange(BufferingState state) final;
  void OnWaitingForDecryptionKey() final;
  void OnVideoNaturalSizeChange(const gfx::Size& size) final;
  void OnVideoOpacityChange(bool opaque) final;

  // TextRenderer tasks and notifications.
  void OnTextRendererEnded();
  void AddTextStreamTask(DemuxerStream* text_stream,
                         const TextTrackConfig& config);
  void RemoveTextStreamTask(DemuxerStream* text_stream);

  // Common handlers for notifications from renderers and demuxer.
  void OnPipelineError(PipelineStatus error);
  void OnCdmAttached(const CdmAttachedCB& cdm_attached_cb,
                     CdmContext* cdm_context,
                     bool success);
  void CheckPlaybackEnded();

  // State transition tasks.
  void DoSeek(base::TimeDelta seek_timestamp, const PipelineStatusCB& done_cb);
  void DoStop(const base::Closure& done_cb);
  void SetState(State next_state);
  State GetNextState() const;
  void StateTransitionTask(PipelineStatus status);
  void InitializeDemuxer(const PipelineStatusCB& done_cb);
  void InitializeRenderer(const PipelineStatusCB& done_cb);
  void DestroyRenderer();
  void ReportMetadata();

  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const scoped_refptr<MediaLog> media_log_;

  base::WeakPtr<PipelineImpl> weak_pipeline_;
  Demuxer* demuxer_;
  std::unique_ptr<TextRenderer> text_renderer_;
  double playback_rate_;
  float volume_;
  CdmContext* cdm_context_;

  // Lock used to serialize |shared_state_|.
  mutable base::Lock shared_state_lock_;

  // State shared between main and media thread.
  SharedState shared_state_;

  // Current state of the pipeline.
  State state_;

  // Status of the pipeline.  Initialized to PIPELINE_OK which indicates that
  // the pipeline is operating correctly. Any other value indicates that the
  // pipeline is stopped or is stopping.  Clients can call the Stop() method to
  // reset the pipeline state, and restore this to PIPELINE_OK.
  PipelineStatus status_;

  // The timestamp to start playback from after starting/seeking/resuming has
  // completed.
  base::TimeDelta start_timestamp_;

  // Whether we've received the audio/video/text ended events.
  bool renderer_ended_;
  bool text_renderer_ended_;

  // Series of tasks to Start(), Seek(), and Resume().
  std::unique_ptr<SerialRunner> pending_callbacks_;

  base::WeakPtr<RendererWrapper> weak_this_;
  base::WeakPtrFactory<RendererWrapper> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(RendererWrapper);
};

PipelineImpl::RendererWrapper::RendererWrapper(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    scoped_refptr<MediaLog> media_log)
    : media_task_runner_(std::move(media_task_runner)),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      media_log_(std::move(media_log)),
      demuxer_(nullptr),
      playback_rate_(kDefaultPlaybackRate),
      volume_(kDefaultVolume),
      cdm_context_(nullptr),
      state_(kCreated),
      status_(PIPELINE_OK),
      renderer_ended_(false),
      text_renderer_ended_(false),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  media_log_->AddEvent(media_log_->CreatePipelineStateChangedEvent(kCreated));
}

PipelineImpl::RendererWrapper::~RendererWrapper() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCreated || state_ == kStopped);
}

void PipelineImpl::RendererWrapper::Start(
    Demuxer* demuxer,
    std::unique_ptr<Renderer> renderer,
    std::unique_ptr<TextRenderer> text_renderer,
    base::WeakPtr<PipelineImpl> weak_pipeline) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(kCreated, state_) << "Received start in unexpected state: "
                              << state_;

  DCHECK(!demuxer_);
  DCHECK(!shared_state_.renderer);
  DCHECK(!text_renderer_);
  DCHECK(!renderer_ended_);
  DCHECK(!text_renderer_ended_);
  DCHECK(!weak_pipeline_);
  demuxer_ = demuxer;
  {
    base::AutoLock auto_lock(shared_state_lock_);
    shared_state_.renderer = std::move(renderer);
  }
  text_renderer_ = std::move(text_renderer);
  if (text_renderer_) {
    text_renderer_->Initialize(
        base::Bind(&RendererWrapper::OnTextRendererEnded, weak_this_));
  }
  weak_pipeline_ = weak_pipeline;

  StateTransitionTask(PIPELINE_OK);
}

void PipelineImpl::RendererWrapper::Stop(const base::Closure& stop_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ != kStopping && state_ != kStopped);

  SetState(kStopping);

  if (shared_state_.statistics.video_frames_decoded > 0) {
    UMA_HISTOGRAM_COUNTS("Media.DroppedFrameCount",
                         shared_state_.statistics.video_frames_dropped);
  }

  // If we stop during starting/seeking/suspending/resuming we don't want to
  // leave outstanding callbacks around. The callbacks also do not get run if
  // the pipeline is stopped before it had a chance to complete outstanding
  // tasks.
  pending_callbacks_.reset();

  DoStop(stop_cb);
}

void PipelineImpl::RendererWrapper::Seek(base::TimeDelta time) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Suppress seeking if we're not fully started.
  if (state_ != kPlaying) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive seek in unexpected state: " << state_;
    OnPipelineError(PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  const base::TimeDelta seek_timestamp =
      std::max(time, demuxer_->GetStartTime());

  SetState(kSeeking);
  renderer_ended_ = false;
  text_renderer_ended_ = false;
  start_timestamp_ = seek_timestamp;

  DoSeek(seek_timestamp,
         base::Bind(&RendererWrapper::StateTransitionTask, weak_this_));
}

void PipelineImpl::RendererWrapper::Suspend() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Suppress suspending if we're not playing.
  if (state_ != kPlaying) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive suspend in unexpected state: " << state_;
    OnPipelineError(PIPELINE_ERROR_INVALID_STATE);
    return;
  }
  DCHECK(shared_state_.renderer);
  DCHECK(!pending_callbacks_.get());

  SetState(kSuspending);

  // Freeze playback and record the media time before flushing. (Flushing clears
  // the value.)
  shared_state_.renderer->SetPlaybackRate(0.0);
  {
    base::AutoLock auto_lock(shared_state_lock_);
    shared_state_.suspend_timestamp = shared_state_.renderer->GetMediaTime();
    DCHECK(shared_state_.suspend_timestamp != kNoTimestamp());
  }

  // Queue the asynchronous actions required to stop playback. (Matches setup in
  // DoSeek().)
  // TODO(sandersd): Share implementation with DoSeek().
  SerialRunner::Queue fns;

  if (text_renderer_) {
    fns.Push(base::Bind(&TextRenderer::Pause,
                        base::Unretained(text_renderer_.get())));
  }

  fns.Push(base::Bind(&Renderer::Flush,
                      base::Unretained(shared_state_.renderer.get())));

  if (text_renderer_) {
    fns.Push(base::Bind(&TextRenderer::Flush,
                        base::Unretained(text_renderer_.get())));
  }

  pending_callbacks_ = SerialRunner::Run(
      fns, base::Bind(&RendererWrapper::StateTransitionTask, weak_this_));
}

void PipelineImpl::RendererWrapper::Resume(std::unique_ptr<Renderer> renderer,
                                           base::TimeDelta timestamp) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Suppress resuming if we're not suspended.
  if (state_ != kSuspended) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive resume in unexpected state: " << state_;
    OnPipelineError(PIPELINE_ERROR_INVALID_STATE);
    return;
  }
  DCHECK(!shared_state_.renderer);
  DCHECK(!pending_callbacks_.get());

  SetState(kResuming);

  {
    base::AutoLock auto_lock(shared_state_lock_);
    shared_state_.renderer = std::move(renderer);
  }

  // Set up for a seek. (Matches setup in SeekTask().)
  // TODO(sandersd): Share implementation with SeekTask().
  renderer_ended_ = false;
  text_renderer_ended_ = false;
  start_timestamp_ = std::max(timestamp, demuxer_->GetStartTime());

  // Queue the asynchronous actions required to start playback. Unlike DoSeek(),
  // we need to initialize the renderer ourselves (we don't want to enter state
  // kInitDemuxer, and even if we did the current code would seek to the start
  // instead of |timestamp|).
  SerialRunner::Queue fns;

  fns.Push(
      base::Bind(&Demuxer::Seek, base::Unretained(demuxer_), start_timestamp_));

  fns.Push(base::Bind(&RendererWrapper::InitializeRenderer, weak_this_));

  pending_callbacks_ = SerialRunner::Run(
      fns, base::Bind(&RendererWrapper::StateTransitionTask, weak_this_));
}

void PipelineImpl::RendererWrapper::SetPlaybackRate(double playback_rate) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  playback_rate_ = playback_rate;
  if (state_ == kPlaying)
    shared_state_.renderer->SetPlaybackRate(playback_rate_);
}

void PipelineImpl::RendererWrapper::SetVolume(float volume) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  volume_ = volume;
  if (state_ == kPlaying)
    shared_state_.renderer->SetVolume(volume_);
}

base::TimeDelta PipelineImpl::RendererWrapper::GetMediaTime() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(shared_state_lock_);
  if (shared_state_.suspend_timestamp != kNoTimestamp())
    return shared_state_.suspend_timestamp;
  return shared_state_.renderer ? shared_state_.renderer->GetMediaTime()
                                : base::TimeDelta();
}

Ranges<base::TimeDelta> PipelineImpl::RendererWrapper::GetBufferedTimeRanges()
    const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(shared_state_lock_);
  return shared_state_.buffered_time_ranges;
}

bool PipelineImpl::RendererWrapper::DidLoadingProgress() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(shared_state_lock_);
  bool did_progress = shared_state_.did_loading_progress;
  shared_state_.did_loading_progress = false;
  return did_progress;
}

PipelineStatistics PipelineImpl::RendererWrapper::GetStatistics() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(shared_state_lock_);
  return shared_state_.statistics;
}

void PipelineImpl::RendererWrapper::SetCdm(
    CdmContext* cdm_context,
    const CdmAttachedCB& cdm_attached_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (!shared_state_.renderer) {
    cdm_context_ = cdm_context;
    cdm_attached_cb.Run(true);
    return;
  }

  shared_state_.renderer->SetCdm(
      cdm_context, base::Bind(&RendererWrapper::OnCdmAttached, weak_this_,
                              cdm_attached_cb, cdm_context));
}

void PipelineImpl::RendererWrapper::OnBufferedTimeRangesChanged(
    const Ranges<base::TimeDelta>& ranges) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  base::AutoLock auto_lock(shared_state_lock_);
  shared_state_.did_loading_progress = true;
  shared_state_.buffered_time_ranges = ranges;
}

void PipelineImpl::RendererWrapper::SetDuration(base::TimeDelta duration) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  media_log_->AddEvent(media_log_->CreateTimeEvent(MediaLogEvent::DURATION_SET,
                                                   "duration", duration));
  UMA_HISTOGRAM_LONG_TIMES("Media.Duration", duration);

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::OnDurationChange, weak_pipeline_, duration));
}

void PipelineImpl::RendererWrapper::OnDemuxerError(PipelineStatus error) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::OnPipelineError, weak_this_, error));
}

void PipelineImpl::RendererWrapper::AddTextStream(
    DemuxerStream* text_stream,
    const TextTrackConfig& config) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RendererWrapper::AddTextStreamTask, weak_this_,
                            text_stream, config));
}

void PipelineImpl::RendererWrapper::RemoveTextStream(
    DemuxerStream* text_stream) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RendererWrapper::RemoveTextStreamTask, weak_this_,
                            text_stream));
}

void PipelineImpl::RendererWrapper::OnError(PipelineStatus error) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::OnPipelineError, weak_this_, error));
}

void PipelineImpl::RendererWrapper::OnEnded() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::ENDED));

  if (state_ != kPlaying)
    return;

  DCHECK(!renderer_ended_);
  renderer_ended_ = true;
  CheckPlaybackEnded();
}

void PipelineImpl::RendererWrapper::OnStatisticsUpdate(
    const PipelineStatistics& stats) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(shared_state_lock_);
  shared_state_.statistics.audio_bytes_decoded += stats.audio_bytes_decoded;
  shared_state_.statistics.video_bytes_decoded += stats.video_bytes_decoded;
  shared_state_.statistics.video_frames_decoded += stats.video_frames_decoded;
  shared_state_.statistics.video_frames_dropped += stats.video_frames_dropped;
  shared_state_.statistics.audio_memory_usage += stats.audio_memory_usage;
  shared_state_.statistics.video_memory_usage += stats.video_memory_usage;
}

void PipelineImpl::RendererWrapper::OnBufferingStateChange(
    BufferingState state) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __FUNCTION__ << "(" << state << ") ";

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::OnBufferingStateChange, weak_pipeline_, state));
}

void PipelineImpl::RendererWrapper::OnWaitingForDecryptionKey() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::OnWaitingForDecryptionKey, weak_pipeline_));
}

void PipelineImpl::RendererWrapper::OnVideoNaturalSizeChange(
    const gfx::Size& size) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::OnVideoNaturalSizeChange,
                            weak_pipeline_, size));
}

void PipelineImpl::RendererWrapper::OnVideoOpacityChange(bool opaque) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::OnVideoOpacityChange, weak_pipeline_, opaque));
}

void PipelineImpl::RendererWrapper::OnTextRendererEnded() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::TEXT_ENDED));

  if (state_ != kPlaying)
    return;

  DCHECK(!text_renderer_ended_);
  text_renderer_ended_ = true;
  CheckPlaybackEnded();
}

void PipelineImpl::RendererWrapper::AddTextStreamTask(
    DemuxerStream* text_stream,
    const TextTrackConfig& config) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // TODO(matthewjheaney): fix up text_ended_ when text stream
  // is added (http://crbug.com/321446).
  if (text_renderer_)
    text_renderer_->AddTextStream(text_stream, config);
}

void PipelineImpl::RendererWrapper::RemoveTextStreamTask(
    DemuxerStream* text_stream) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (text_renderer_)
    text_renderer_->RemoveTextStream(text_stream);
}

void PipelineImpl::RendererWrapper::OnPipelineError(PipelineStatus error) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";

  // Preserve existing abnormal status.
  if (status_ != PIPELINE_OK)
    return;

  // Don't report pipeline error events to the media log here. The embedder
  // will log this when Client::OnError is called. If the pipeline is already
  // stopped or stopping we also don't want to log any event. In case we are
  // suspending or suspended, the error may be recoverable, so don't propagate
  // it now, instead let the subsequent seek during resume propagate it if
  // it's unrecoverable.
  if (state_ == kStopping || state_ == kStopped || state_ == kSuspending ||
      state_ == kSuspended) {
    return;
  }

  status_ = error;
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::OnError, weak_pipeline_, error));
}

void PipelineImpl::RendererWrapper::OnCdmAttached(
    const CdmAttachedCB& cdm_attached_cb,
    CdmContext* cdm_context,
    bool success) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (success)
    cdm_context_ = cdm_context;
  cdm_attached_cb.Run(success);
}

void PipelineImpl::RendererWrapper::CheckPlaybackEnded() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (shared_state_.renderer && !renderer_ended_)
    return;

  if (text_renderer_ && text_renderer_->HasTracks() && !text_renderer_ended_)
    return;

  DCHECK_EQ(status_, PIPELINE_OK);
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::OnEnded, weak_pipeline_));
}

// Note that the usage of base::Unretained() with the renderers is considered
// safe as they are owned by |pending_callbacks_| and share the same lifetime.
//
// That being said, deleting the renderers while keeping |pending_callbacks_|
// running on the media thread would result in crashes.
void PipelineImpl::RendererWrapper::DoSeek(base::TimeDelta seek_timestamp,
                                           const PipelineStatusCB& done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  DCHECK_EQ(state_, kSeeking);
  SerialRunner::Queue bound_fns;

  // Pause.
  if (text_renderer_) {
    bound_fns.Push(base::Bind(&TextRenderer::Pause,
                              base::Unretained(text_renderer_.get())));
  }

  // Flush.
  DCHECK(shared_state_.renderer);
  bound_fns.Push(base::Bind(&Renderer::Flush,
                            base::Unretained(shared_state_.renderer.get())));

  if (text_renderer_) {
    bound_fns.Push(base::Bind(&TextRenderer::Flush,
                              base::Unretained(text_renderer_.get())));
  }

  // Seek demuxer.
  bound_fns.Push(
      base::Bind(&Demuxer::Seek, base::Unretained(demuxer_), seek_timestamp));

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void PipelineImpl::RendererWrapper::DoStop(const base::Closure& done_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopping);
  DCHECK(!pending_callbacks_.get());

  DestroyRenderer();
  text_renderer_.reset();

  if (demuxer_) {
    demuxer_->Stop();
    demuxer_ = NULL;
  }

  SetState(kStopped);

  // Post the stop callback to enqueue it after the tasks that may have been
  // posted by Demuxer and Renderer during stopping. Note that in theory the
  // tasks posted by Demuxer/Renderer may post even more tasks that will get
  // enqueued after |done_cb|. This may be problematic because Demuxer may
  // get destroyed as soon as |done_cb| is run. In practice this is not a
  // problem, but ideally Demuxer should be destroyed on the media thread.
  media_task_runner_->PostTask(FROM_HERE, done_cb);
}

void PipelineImpl::RendererWrapper::SetState(State next_state) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << PipelineImpl::GetStateString(state_) << " -> "
           << PipelineImpl::GetStateString(next_state);

  state_ = next_state;
  media_log_->AddEvent(media_log_->CreatePipelineStateChangedEvent(next_state));
}

PipelineImpl::State PipelineImpl::RendererWrapper::GetNextState() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(status_, PIPELINE_OK)
      << "State transitions don't happen when there's an error: " << status_;

  switch (state_) {
    case kCreated:
      return kInitDemuxer;

    case kInitDemuxer:
      return kInitRenderer;

    case kInitRenderer:
    case kSeeking:
      return kPlaying;

    case kSuspending:
      return kSuspended;

    case kSuspended:
      return kResuming;

    case kResuming:
      return kPlaying;

    case kPlaying:
    case kStopping:
    case kStopped:
      break;
  }
  NOTREACHED() << "State has no transition: " << state_;
  return state_;
}
void PipelineImpl::RendererWrapper::StateTransitionTask(PipelineStatus status) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // No-op any state transitions if we're stopping or already encountered error.
  if (state_ == kStopping || state_ == kStopped || status_ != PIPELINE_OK)
    return;

  // Report error from the previous operation.
  if (status != PIPELINE_OK) {
    OnPipelineError(status);
    return;
  }

  // Guard against accidentally clearing |pending_callbacks_| for states that
  // use it as well as states that should not be using it.
  DCHECK_EQ(pending_callbacks_.get() != NULL,
            state_ == kSeeking || state_ == kSuspending || state_ == kResuming);

  pending_callbacks_.reset();

  PipelineStatusCB done_cb =
      base::Bind(&RendererWrapper::StateTransitionTask, weak_this_);

  // Switch states, performing any entrance actions for the new state as well.
  SetState(GetNextState());
  switch (state_) {
    case kInitDemuxer:
      return InitializeDemuxer(done_cb);

    case kInitRenderer:
      // When the state_ transfers to kInitRenderer, it means the demuxer has
      // finished parsing the init info. It should call ReportMetadata in case
      // meeting 'decode' error when passing media segment but WebMediaPlayer's
      // ready_state_ is still ReadyStateHaveNothing. In that case, it will
      // treat it as NetworkStateFormatError not NetworkStateDecodeError.
      ReportMetadata();
      start_timestamp_ = demuxer_->GetStartTime();

      return InitializeRenderer(done_cb);

    case kPlaying:
      DCHECK(start_timestamp_ >= base::TimeDelta());
      shared_state_.renderer->StartPlayingFrom(start_timestamp_);
      {
        base::AutoLock auto_lock(shared_state_lock_);
        shared_state_.suspend_timestamp = kNoTimestamp();
      }

      if (text_renderer_)
        text_renderer_->StartPlaying();

      main_task_runner_->PostTask(
          FROM_HERE, base::Bind(&PipelineImpl::OnSeekDone, weak_pipeline_,
                                start_timestamp_));

      shared_state_.renderer->SetPlaybackRate(playback_rate_);
      shared_state_.renderer->SetVolume(volume_);
      return;

    case kSuspended:
      DestroyRenderer();
      {
        base::AutoLock auto_lock(shared_state_lock_);
        shared_state_.statistics.audio_memory_usage = 0;
        shared_state_.statistics.video_memory_usage = 0;
      }
      main_task_runner_->PostTask(
          FROM_HERE, base::Bind(&PipelineImpl::OnSuspendDone, weak_pipeline_,
                                shared_state_.suspend_timestamp));
      return;

    case kStopping:
    case kStopped:
    case kCreated:
    case kSeeking:
    case kSuspending:
    case kResuming:
      NOTREACHED() << "State has no transition: " << state_;
      return;
  }
}

void PipelineImpl::RendererWrapper::InitializeDemuxer(
    const PipelineStatusCB& done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  demuxer_->Initialize(this, done_cb, !!text_renderer_);
}

void PipelineImpl::RendererWrapper::InitializeRenderer(
    const PipelineStatusCB& done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (!demuxer_->GetStream(DemuxerStream::AUDIO) &&
      !demuxer_->GetStream(DemuxerStream::VIDEO)) {
    done_cb.Run(PIPELINE_ERROR_COULD_NOT_RENDER);
    return;
  }

  if (cdm_context_)
    shared_state_.renderer->SetCdm(cdm_context_,
                                   base::Bind(&IgnoreCdmAttached));

  shared_state_.renderer->Initialize(demuxer_, this, done_cb);
}

void PipelineImpl::RendererWrapper::DestroyRenderer() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Destroy the renderer outside the lock scope to avoid holding the lock
  // while renderer is being destroyed (in case Renderer destructor is costly).
  std::unique_ptr<Renderer> renderer;
  {
    base::AutoLock auto_lock(shared_state_lock_);
    renderer.swap(shared_state_.renderer);
  }
}

void PipelineImpl::RendererWrapper::ReportMetadata() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  PipelineMetadata metadata;
  metadata.timeline_offset = demuxer_->GetTimelineOffset();
  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  if (stream) {
    metadata.has_video = true;
    metadata.natural_size = stream->video_decoder_config().natural_size();
    metadata.video_rotation = stream->video_rotation();
  }
  if (demuxer_->GetStream(DemuxerStream::AUDIO)) {
    metadata.has_audio = true;
  }

  main_task_runner_->PostTask(FROM_HERE, base::Bind(&PipelineImpl::OnMetadata,
                                                    weak_pipeline_, metadata));
}

PipelineImpl::PipelineImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    MediaLog* media_log)
    : media_task_runner_(media_task_runner),
      media_log_(media_log),
      client_(nullptr),
      playback_rate_(kDefaultPlaybackRate),
      volume_(kDefaultVolume),
      weak_factory_(this) {
  DVLOG(2) << __FUNCTION__;
  renderer_wrapper_.reset(new RendererWrapper(media_task_runner_, media_log_));
}

PipelineImpl::~PipelineImpl() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!client_) << "Stop() must complete before destroying object";
  DCHECK(seek_cb_.is_null());
  DCHECK(suspend_cb_.is_null());
  DCHECK(!weak_factory_.HasWeakPtrs());

  // RendererWrapper is deleted on the media thread.
  media_task_runner_->DeleteSoon(FROM_HERE, renderer_wrapper_.release());
}

void PipelineImpl::Start(Demuxer* demuxer,
                         std::unique_ptr<Renderer> renderer,
                         Client* client,
                         const PipelineStatusCB& seek_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(demuxer);
  DCHECK(renderer);
  DCHECK(client);
  DCHECK(!seek_cb.is_null());

  DCHECK(!client_);
  DCHECK(seek_cb_.is_null());
  client_ = client;
  seek_cb_ = seek_cb;

  std::unique_ptr<TextRenderer> text_renderer;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableInbandTextTracks)) {
    text_renderer.reset(new TextRenderer(
        media_task_runner_,
        BindToCurrentLoop(base::Bind(&PipelineImpl::OnAddTextTrack,
                                     weak_factory_.GetWeakPtr()))));
  }

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::Start,
                 base::Unretained(renderer_wrapper_.get()), demuxer,
                 base::Passed(&renderer), base::Passed(&text_renderer),
                 weak_factory_.GetWeakPtr()));
}

void PipelineImpl::Stop() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!IsRunning()) {
    DVLOG(2) << "Media pipeline isn't running. Ignoring Stop()";
    return;
  }

  if (media_task_runner_->BelongsToCurrentThread()) {
    // This path is executed by unittests that share media and main threads.
    base::Closure stop_cb = base::Bind(&base::DoNothing);
    media_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RendererWrapper::Stop,
                   base::Unretained(renderer_wrapper_.get()), stop_cb));
  } else {
    // This path is executed by production code where the two task runners -
    // main and media - live on different threads.
    //
    // TODO(alokp): We should not have to wait for the RendererWrapper::Stop.
    // RendererWrapper holds a raw reference to Demuxer, which in turn holds a
    // raw reference to DataSource. Both Demuxer and DataSource need to live
    // until RendererWrapper is stopped. If RendererWrapper owned Demuxer and
    // Demuxer owned DataSource, we could simply let RendererWrapper get lazily
    // destroyed on the media thread.
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::Closure stop_cb =
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter));
    // If posting the task fails or the posted task fails to run,
    // we will wait here forever. So add a CHECK to make sure we do not run
    // into those situations.
    CHECK(media_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RendererWrapper::Stop,
                   base::Unretained(renderer_wrapper_.get()), stop_cb)));
    waiter.Wait();
  }

  // Once the pipeline is stopped, nothing is reported back to the client.
  // Reset all callbacks and client handle.
  seek_cb_.Reset();
  suspend_cb_.Reset();
  client_ = nullptr;

  // Invalidate self weak pointers effectively canceling all pending
  // notifications in the message queue.
  weak_factory_.InvalidateWeakPtrs();
}

void PipelineImpl::Seek(base::TimeDelta time, const PipelineStatusCB& seek_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!seek_cb.is_null());

  if (!IsRunning()) {
    DLOG(ERROR) << "Media pipeline isn't running. Ignoring Seek().";
    return;
  }

  DCHECK(seek_cb_.is_null());
  seek_cb_ = seek_cb;
  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RendererWrapper::Seek,
                            base::Unretained(renderer_wrapper_.get()), time));
}

void PipelineImpl::Suspend(const PipelineStatusCB& suspend_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(!suspend_cb.is_null());

  DCHECK(IsRunning());
  DCHECK(suspend_cb_.is_null());
  suspend_cb_ = suspend_cb;

  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RendererWrapper::Suspend,
                            base::Unretained(renderer_wrapper_.get())));
}

void PipelineImpl::Resume(std::unique_ptr<Renderer> renderer,
                          base::TimeDelta time,
                          const PipelineStatusCB& seek_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(renderer);
  DCHECK(!seek_cb.is_null());

  DCHECK(IsRunning());
  DCHECK(seek_cb_.is_null());
  seek_cb_ = seek_cb;

  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RendererWrapper::Resume,
                            base::Unretained(renderer_wrapper_.get()),
                            base::Passed(&renderer), time));
}

bool PipelineImpl::IsRunning() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return !!client_;
}

double PipelineImpl::GetPlaybackRate() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return playback_rate_;
}

void PipelineImpl::SetPlaybackRate(double playback_rate) {
  DVLOG(2) << __FUNCTION__ << "(" << playback_rate << ")";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (playback_rate < 0.0)
    return;

  playback_rate_ = playback_rate;
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::SetPlaybackRate,
                 base::Unretained(renderer_wrapper_.get()), playback_rate_));
}

float PipelineImpl::GetVolume() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return volume_;
}

void PipelineImpl::SetVolume(float volume) {
  DVLOG(2) << __FUNCTION__ << "(" << volume << ")";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (volume < 0.0f || volume > 1.0f)
    return;

  volume_ = volume;
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::SetVolume,
                 base::Unretained(renderer_wrapper_.get()), volume_));
}

base::TimeDelta PipelineImpl::GetMediaTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return renderer_wrapper_->GetMediaTime();
}

Ranges<base::TimeDelta> PipelineImpl::GetBufferedTimeRanges() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return renderer_wrapper_->GetBufferedTimeRanges();
}

base::TimeDelta PipelineImpl::GetMediaDuration() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return duration_;
}

bool PipelineImpl::DidLoadingProgress() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return renderer_wrapper_->DidLoadingProgress();
}

PipelineStatistics PipelineImpl::GetStatistics() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return renderer_wrapper_->GetStatistics();
}

void PipelineImpl::SetCdm(CdmContext* cdm_context,
                          const CdmAttachedCB& cdm_attached_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(cdm_context);
  DCHECK(!cdm_attached_cb.is_null());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::SetCdm,
                 base::Unretained(renderer_wrapper_.get()), cdm_context,
                 media::BindToCurrentLoop(cdm_attached_cb)));
}

#define RETURN_STRING(state) \
  case state:                \
    return #state;

// static
const char* PipelineImpl::GetStateString(State state) {
  switch (state) {
    RETURN_STRING(kCreated);
    RETURN_STRING(kInitDemuxer);
    RETURN_STRING(kInitRenderer);
    RETURN_STRING(kSeeking);
    RETURN_STRING(kPlaying);
    RETURN_STRING(kStopping);
    RETURN_STRING(kStopped);
    RETURN_STRING(kSuspending);
    RETURN_STRING(kSuspended);
    RETURN_STRING(kResuming);
  }
  NOTREACHED();
  return "INVALID";
}

#undef RETURN_STRING

void PipelineImpl::OnError(PipelineStatus error) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";
  DCHECK(IsRunning());

  // If the error happens during starting/seeking/suspending/resuming,
  // report the error via the completion callback for those tasks.
  // Else report error via the client interface.
  if (!seek_cb_.is_null()) {
    base::ResetAndReturn(&seek_cb_).Run(error);
  } else if (!suspend_cb_.is_null()) {
    base::ResetAndReturn(&suspend_cb_).Run(error);
  } else {
    DCHECK(client_);
    client_->OnError(error);
  }

  // Any kind of error stops the pipeline.
  Stop();
}

void PipelineImpl::OnEnded() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnEnded();
}

void PipelineImpl::OnMetadata(PipelineMetadata metadata) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnMetadata(metadata);
}

void PipelineImpl::OnBufferingStateChange(BufferingState state) {
  DVLOG(2) << __FUNCTION__ << "(" << state << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnBufferingStateChange(state);
}

void PipelineImpl::OnDurationChange(base::TimeDelta duration) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  duration_ = duration;

  DCHECK(client_);
  client_->OnDurationChange();
}

void PipelineImpl::OnAddTextTrack(const TextTrackConfig& config,
                                  const AddTextTrackDoneCB& done_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnAddTextTrack(config, done_cb);
}

void PipelineImpl::OnWaitingForDecryptionKey() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnWaitingForDecryptionKey();
}

void PipelineImpl::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnVideoNaturalSizeChange(size);
}

void PipelineImpl::OnVideoOpacityChange(bool opaque) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnVideoOpacityChange(opaque);
}

void PipelineImpl::OnSeekDone(base::TimeDelta start_time) {
  DVLOG(3) << __FUNCTION__ << "(" << start_time.InMicroseconds() << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(!seek_cb_.is_null());
  base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
}

void PipelineImpl::OnSuspendDone(base::TimeDelta suspend_time) {
  DVLOG(3) << __FUNCTION__ << "(" << suspend_time.InMicroseconds() << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(!suspend_cb_.is_null());
  base::ResetAndReturn(&suspend_cb_).Run(PIPELINE_OK);
}

}  // namespace media
