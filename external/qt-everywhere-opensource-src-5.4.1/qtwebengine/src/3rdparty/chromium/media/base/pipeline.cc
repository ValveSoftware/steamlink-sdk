// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer.h"
#include "media/base/clock.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/text_renderer.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_renderer.h"

using base::TimeDelta;

namespace media {

Pipeline::Pipeline(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    MediaLog* media_log)
    : task_runner_(task_runner),
      media_log_(media_log),
      running_(false),
      did_loading_progress_(false),
      volume_(1.0f),
      playback_rate_(0.0f),
      clock_(new Clock(&default_tick_clock_)),
      clock_state_(CLOCK_PAUSED),
      status_(PIPELINE_OK),
      state_(kCreated),
      audio_ended_(false),
      video_ended_(false),
      text_ended_(false),
      audio_buffering_state_(BUFFERING_HAVE_NOTHING),
      video_buffering_state_(BUFFERING_HAVE_NOTHING),
      demuxer_(NULL),
      creation_time_(default_tick_clock_.NowTicks()) {
  media_log_->AddEvent(media_log_->CreatePipelineStateChangedEvent(kCreated));
  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::PIPELINE_CREATED));
}

Pipeline::~Pipeline() {
  DCHECK(thread_checker_.CalledOnValidThread())
      << "Pipeline must be destroyed on same thread that created it";
  DCHECK(!running_) << "Stop() must complete before destroying object";
  DCHECK(stop_cb_.is_null());
  DCHECK(seek_cb_.is_null());

  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::PIPELINE_DESTROYED));
}

void Pipeline::Start(scoped_ptr<FilterCollection> collection,
                     const base::Closure& ended_cb,
                     const PipelineStatusCB& error_cb,
                     const PipelineStatusCB& seek_cb,
                     const PipelineMetadataCB& metadata_cb,
                     const base::Closure& preroll_completed_cb,
                     const base::Closure& duration_change_cb) {
  DCHECK(!ended_cb.is_null());
  DCHECK(!error_cb.is_null());
  DCHECK(!seek_cb.is_null());
  DCHECK(!metadata_cb.is_null());
  DCHECK(!preroll_completed_cb.is_null());

  base::AutoLock auto_lock(lock_);
  CHECK(!running_) << "Media pipeline is already running";
  running_ = true;

  filter_collection_ = collection.Pass();
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  seek_cb_ = seek_cb;
  metadata_cb_ = metadata_cb;
  preroll_completed_cb_ = preroll_completed_cb;
  duration_change_cb_ = duration_change_cb;

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&Pipeline::StartTask, base::Unretained(this)));
}

void Pipeline::Stop(const base::Closure& stop_cb) {
  base::AutoLock auto_lock(lock_);
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::StopTask, base::Unretained(this), stop_cb));
}

void Pipeline::Seek(TimeDelta time, const PipelineStatusCB& seek_cb) {
  base::AutoLock auto_lock(lock_);
  if (!running_) {
    NOTREACHED() << "Media pipeline isn't running";
    return;
  }

  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::SeekTask, base::Unretained(this), time, seek_cb));
}

bool Pipeline::IsRunning() const {
  base::AutoLock auto_lock(lock_);
  return running_;
}

float Pipeline::GetPlaybackRate() const {
  base::AutoLock auto_lock(lock_);
  return playback_rate_;
}

void Pipeline::SetPlaybackRate(float playback_rate) {
  if (playback_rate < 0.0f)
    return;

  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
  if (running_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &Pipeline::PlaybackRateChangedTask, base::Unretained(this),
        playback_rate));
  }
}

float Pipeline::GetVolume() const {
  base::AutoLock auto_lock(lock_);
  return volume_;
}

void Pipeline::SetVolume(float volume) {
  if (volume < 0.0f || volume > 1.0f)
    return;

  base::AutoLock auto_lock(lock_);
  volume_ = volume;
  if (running_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &Pipeline::VolumeChangedTask, base::Unretained(this), volume));
  }
}

TimeDelta Pipeline::GetMediaTime() const {
  base::AutoLock auto_lock(lock_);
  return clock_->Elapsed();
}

Ranges<TimeDelta> Pipeline::GetBufferedTimeRanges() const {
  base::AutoLock auto_lock(lock_);
  return buffered_time_ranges_;
}

TimeDelta Pipeline::GetMediaDuration() const {
  base::AutoLock auto_lock(lock_);
  return clock_->Duration();
}

bool Pipeline::DidLoadingProgress() {
  base::AutoLock auto_lock(lock_);
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

PipelineStatistics Pipeline::GetStatistics() const {
  base::AutoLock auto_lock(lock_);
  return statistics_;
}

void Pipeline::SetClockForTesting(Clock* clock) {
  clock_.reset(clock);
}

void Pipeline::SetErrorForTesting(PipelineStatus status) {
  SetError(status);
}

void Pipeline::SetState(State next_state) {
  if (state_ != kPlaying && next_state == kPlaying &&
      !creation_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("Media.TimeToPipelineStarted",
                        default_tick_clock_.NowTicks() - creation_time_);
    creation_time_ = base::TimeTicks();
  }

  DVLOG(1) << GetStateString(state_) << " -> " << GetStateString(next_state);

  state_ = next_state;
  media_log_->AddEvent(media_log_->CreatePipelineStateChangedEvent(next_state));
}

#define RETURN_STRING(state) case state: return #state;

const char* Pipeline::GetStateString(State state) {
  switch (state) {
    RETURN_STRING(kCreated);
    RETURN_STRING(kInitDemuxer);
    RETURN_STRING(kInitAudioRenderer);
    RETURN_STRING(kInitVideoRenderer);
    RETURN_STRING(kInitPrerolling);
    RETURN_STRING(kSeeking);
    RETURN_STRING(kPlaying);
    RETURN_STRING(kStopping);
    RETURN_STRING(kStopped);
  }
  NOTREACHED();
  return "INVALID";
}

#undef RETURN_STRING

Pipeline::State Pipeline::GetNextState() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null())
      << "State transitions don't happen when stopping";
  DCHECK_EQ(status_, PIPELINE_OK)
      << "State transitions don't happen when there's an error: " << status_;

  switch (state_) {
    case kCreated:
      return kInitDemuxer;

    case kInitDemuxer:
      if (demuxer_->GetStream(DemuxerStream::AUDIO))
        return kInitAudioRenderer;
      if (demuxer_->GetStream(DemuxerStream::VIDEO))
        return kInitVideoRenderer;
      return kInitPrerolling;

    case kInitAudioRenderer:
      if (demuxer_->GetStream(DemuxerStream::VIDEO))
        return kInitVideoRenderer;
      return kInitPrerolling;

    case kInitVideoRenderer:
      return kInitPrerolling;

    case kInitPrerolling:
      return kPlaying;

    case kSeeking:
      return kPlaying;

    case kPlaying:
    case kStopping:
    case kStopped:
      break;
  }
  NOTREACHED() << "State has no transition: " << state_;
  return state_;
}

void Pipeline::OnDemuxerError(PipelineStatus error) {
  SetError(error);
}

void Pipeline::AddTextStream(DemuxerStream* text_stream,
                             const TextTrackConfig& config) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
    &Pipeline::AddTextStreamTask, base::Unretained(this),
    text_stream, config));
}

void Pipeline::RemoveTextStream(DemuxerStream* text_stream) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
    &Pipeline::RemoveTextStreamTask, base::Unretained(this),
    text_stream));
}

void Pipeline::SetError(PipelineStatus error) {
  DCHECK(IsRunning());
  DCHECK_NE(PIPELINE_OK, error);
  VLOG(1) << "Media pipeline error: " << error;

  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::ErrorChangedTask, base::Unretained(this), error));

  media_log_->AddEvent(media_log_->CreatePipelineErrorEvent(error));
}

void Pipeline::OnAudioTimeUpdate(TimeDelta time, TimeDelta max_time) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_LE(time.InMicroseconds(), max_time.InMicroseconds());
  base::AutoLock auto_lock(lock_);

  if (clock_state_ == CLOCK_WAITING_FOR_AUDIO_TIME_UPDATE &&
      time < clock_->Elapsed()) {
    return;
  }

  if (state_ == kSeeking)
    return;

  clock_->SetTime(time, max_time);
  StartClockIfWaitingForTimeUpdate_Locked();
}

void Pipeline::OnVideoTimeUpdate(TimeDelta max_time) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (audio_renderer_)
    return;

  if (state_ == kSeeking)
    return;

  base::AutoLock auto_lock(lock_);
  DCHECK_NE(clock_state_, CLOCK_WAITING_FOR_AUDIO_TIME_UPDATE);
  clock_->SetMaxTime(max_time);
}

void Pipeline::SetDuration(TimeDelta duration) {
  DCHECK(IsRunning());
  media_log_->AddEvent(
      media_log_->CreateTimeEvent(
          MediaLogEvent::DURATION_SET, "duration", duration));
  UMA_HISTOGRAM_LONG_TIMES("Media.Duration", duration);

  base::AutoLock auto_lock(lock_);
  clock_->SetDuration(duration);
  if (!duration_change_cb_.is_null())
    duration_change_cb_.Run();
}

void Pipeline::OnStateTransition(PipelineStatus status) {
  // Force post to process state transitions after current execution frame.
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::StateTransitionTask, base::Unretained(this), status));
}

void Pipeline::StateTransitionTask(PipelineStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // No-op any state transitions if we're stopping.
  if (state_ == kStopping || state_ == kStopped)
    return;

  // Preserve existing abnormal status, otherwise update based on the result of
  // the previous operation.
  status_ = (status_ != PIPELINE_OK ? status_ : status);

  if (status_ != PIPELINE_OK) {
    ErrorChangedTask(status_);
    return;
  }

  // Guard against accidentally clearing |pending_callbacks_| for states that
  // use it as well as states that should not be using it.
  DCHECK_EQ(pending_callbacks_.get() != NULL,
            (state_ == kInitPrerolling || state_ == kSeeking));

  pending_callbacks_.reset();

  PipelineStatusCB done_cb = base::Bind(
      &Pipeline::OnStateTransition, base::Unretained(this));

  // Switch states, performing any entrance actions for the new state as well.
  SetState(GetNextState());
  switch (state_) {
    case kInitDemuxer:
      return InitializeDemuxer(done_cb);

    case kInitAudioRenderer:
      return InitializeAudioRenderer(done_cb);

    case kInitVideoRenderer:
      return InitializeVideoRenderer(done_cb);

    case kInitPrerolling:
      filter_collection_.reset();
      {
        base::AutoLock l(lock_);
        // We do not want to start the clock running. We only want to set the
        // base media time so our timestamp calculations will be correct.
        clock_->SetTime(demuxer_->GetStartTime(), demuxer_->GetStartTime());
      }
      if (!audio_renderer_ && !video_renderer_) {
        done_cb.Run(PIPELINE_ERROR_COULD_NOT_RENDER);
        return;
      }

      {
        PipelineMetadata metadata;
        metadata.has_audio = audio_renderer_;
        metadata.has_video = video_renderer_;
        metadata.timeline_offset = demuxer_->GetTimelineOffset();
        DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
        if (stream)
          metadata.natural_size = stream->video_decoder_config().natural_size();
        metadata_cb_.Run(metadata);
      }

      return DoInitialPreroll(done_cb);

    case kPlaying:
      PlaybackRateChangedTask(GetPlaybackRate());
      VolumeChangedTask(GetVolume());

      // We enter this state from either kInitPrerolling or kSeeking. As of now
      // both those states call Preroll(), which means by time we enter this
      // state we've already buffered enough data. Forcefully update the
      // buffering state, which start the clock and renderers and transition
      // into kPlaying state.
      //
      // TODO(scherkus): Remove after renderers are taught to fire buffering
      // state callbacks http://crbug.com/144683
      DCHECK(WaitingForEnoughData());
      if (audio_renderer_)
        BufferingStateChanged(&audio_buffering_state_, BUFFERING_HAVE_ENOUGH);
      if (video_renderer_)
        BufferingStateChanged(&video_buffering_state_, BUFFERING_HAVE_ENOUGH);
      return;

    case kStopping:
    case kStopped:
    case kCreated:
    case kSeeking:
      NOTREACHED() << "State has no transition: " << state_;
      return;
  }
}

// Note that the usage of base::Unretained() with the audio/video renderers
// in the following DoXXX() functions is considered safe as they are owned by
// |pending_callbacks_| and share the same lifetime.
//
// That being said, deleting the renderers while keeping |pending_callbacks_|
// running on the media thread would result in crashes.
void Pipeline::DoInitialPreroll(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;

  base::TimeDelta seek_timestamp = demuxer_->GetStartTime();

  // Preroll renderers.
  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Preroll, base::Unretained(audio_renderer_.get()),
        seek_timestamp));
  }

  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Preroll, base::Unretained(video_renderer_.get()),
        seek_timestamp));

    // TODO(scherkus): Remove after VideoRenderer is taught to fire buffering
    // state callbacks http://crbug.com/144683
    bound_fns.Push(base::Bind(&VideoRenderer::Play,
                              base::Unretained(video_renderer_.get())));
  }

  if (text_renderer_) {
    bound_fns.Push(base::Bind(
        &TextRenderer::Play, base::Unretained(text_renderer_.get())));
  }

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void Pipeline::DoSeek(
    base::TimeDelta seek_timestamp,
    const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;

  // Pause.
  if (text_renderer_) {
    bound_fns.Push(base::Bind(
        &TextRenderer::Pause, base::Unretained(text_renderer_.get())));
  }

  // Flush.
  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Flush, base::Unretained(audio_renderer_.get())));

    // TODO(scherkus): Remove after AudioRenderer is taught to fire buffering
    // state callbacks http://crbug.com/144683
    bound_fns.Push(base::Bind(&Pipeline::BufferingStateChanged,
                              base::Unretained(this),
                              &audio_buffering_state_,
                              BUFFERING_HAVE_NOTHING));
  }
  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Flush, base::Unretained(video_renderer_.get())));

    // TODO(scherkus): Remove after VideoRenderer is taught to fire buffering
    // state callbacks http://crbug.com/144683
    bound_fns.Push(base::Bind(&Pipeline::BufferingStateChanged,
                              base::Unretained(this),
                              &video_buffering_state_,
                              BUFFERING_HAVE_NOTHING));
  }
  if (text_renderer_) {
    bound_fns.Push(base::Bind(
        &TextRenderer::Flush, base::Unretained(text_renderer_.get())));
  }

  // Seek demuxer.
  bound_fns.Push(base::Bind(
      &Demuxer::Seek, base::Unretained(demuxer_), seek_timestamp));

  // Preroll renderers.
  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Preroll, base::Unretained(audio_renderer_.get()),
        seek_timestamp));
  }

  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Preroll, base::Unretained(video_renderer_.get()),
        seek_timestamp));

    // TODO(scherkus): Remove after renderers are taught to fire buffering
    // state callbacks http://crbug.com/144683
    bound_fns.Push(base::Bind(&VideoRenderer::Play,
                              base::Unretained(video_renderer_.get())));
  }

  if (text_renderer_) {
    bound_fns.Push(base::Bind(
        &TextRenderer::Play, base::Unretained(text_renderer_.get())));
  }

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void Pipeline::DoStop(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;

  if (demuxer_) {
    bound_fns.Push(base::Bind(
        &Demuxer::Stop, base::Unretained(demuxer_)));
  }

  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Stop, base::Unretained(audio_renderer_.get())));
  }

  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Stop, base::Unretained(video_renderer_.get())));
  }

  if (text_renderer_) {
    bound_fns.Push(base::Bind(
        &TextRenderer::Stop, base::Unretained(text_renderer_.get())));
  }

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void Pipeline::OnStopCompleted(PipelineStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopping);
  {
    base::AutoLock l(lock_);
    running_ = false;
  }

  SetState(kStopped);
  pending_callbacks_.reset();
  filter_collection_.reset();
  audio_renderer_.reset();
  video_renderer_.reset();
  text_renderer_.reset();
  demuxer_ = NULL;

  // If we stop during initialization/seeking we want to run |seek_cb_|
  // followed by |stop_cb_| so we don't leave outstanding callbacks around.
  if (!seek_cb_.is_null()) {
    base::ResetAndReturn(&seek_cb_).Run(status_);
    error_cb_.Reset();
  }
  if (!stop_cb_.is_null()) {
    error_cb_.Reset();
    base::ResetAndReturn(&stop_cb_).Run();

    // NOTE: pipeline may be deleted at this point in time as a result of
    // executing |stop_cb_|.
    return;
  }
  if (!error_cb_.is_null()) {
    DCHECK_NE(status_, PIPELINE_OK);
    base::ResetAndReturn(&error_cb_).Run(status_);
  }
}

void Pipeline::AddBufferedTimeRange(base::TimeDelta start,
                                    base::TimeDelta end) {
  DCHECK(IsRunning());
  base::AutoLock auto_lock(lock_);
  buffered_time_ranges_.Add(start, end);
  did_loading_progress_ = true;
}

void Pipeline::OnAudioRendererEnded() {
  // Force post to process ended tasks after current execution frame.
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::DoAudioRendererEnded, base::Unretained(this)));
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::AUDIO_ENDED));
}

void Pipeline::OnVideoRendererEnded() {
  // Force post to process ended tasks after current execution frame.
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::DoVideoRendererEnded, base::Unretained(this)));
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::VIDEO_ENDED));
}

void Pipeline::OnTextRendererEnded() {
  // Force post to process ended messages after current execution frame.
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::DoTextRendererEnded, base::Unretained(this)));
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::TEXT_ENDED));
}

// Called from any thread.
void Pipeline::OnUpdateStatistics(const PipelineStatistics& stats) {
  base::AutoLock auto_lock(lock_);
  statistics_.audio_bytes_decoded += stats.audio_bytes_decoded;
  statistics_.video_bytes_decoded += stats.video_bytes_decoded;
  statistics_.video_frames_decoded += stats.video_frames_decoded;
  statistics_.video_frames_dropped += stats.video_frames_dropped;
}

void Pipeline::StartTask() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  CHECK_EQ(kCreated, state_)
      << "Media pipeline cannot be started more than once";

  text_renderer_ = filter_collection_->GetTextRenderer();

  if (text_renderer_) {
    text_renderer_->Initialize(
        base::Bind(&Pipeline::OnTextRendererEnded, base::Unretained(this)));
  }

  StateTransitionTask(PIPELINE_OK);
}

void Pipeline::StopTask(const base::Closure& stop_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null());

  if (state_ == kStopped) {
    stop_cb.Run();
    return;
  }

  stop_cb_ = stop_cb;

  // We may already be stopping due to a runtime error.
  if (state_ == kStopping)
    return;

  SetState(kStopping);
  pending_callbacks_.reset();
  DoStop(base::Bind(&Pipeline::OnStopCompleted, base::Unretained(this)));
}

void Pipeline::ErrorChangedTask(PipelineStatus error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";

  if (state_ == kStopping || state_ == kStopped)
    return;

  SetState(kStopping);
  pending_callbacks_.reset();
  status_ = error;

  DoStop(base::Bind(&Pipeline::OnStopCompleted, base::Unretained(this)));
}

void Pipeline::PlaybackRateChangedTask(float playback_rate) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Playback rate changes are only carried out while playing.
  if (state_ != kPlaying)
    return;

  {
    base::AutoLock auto_lock(lock_);
    clock_->SetPlaybackRate(playback_rate);
  }

  if (audio_renderer_)
    audio_renderer_->SetPlaybackRate(playback_rate_);
  if (video_renderer_)
    video_renderer_->SetPlaybackRate(playback_rate_);
}

void Pipeline::VolumeChangedTask(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Volume changes are only carried out while playing.
  if (state_ != kPlaying)
    return;

  if (audio_renderer_)
    audio_renderer_->SetVolume(volume);
}

void Pipeline::SeekTask(TimeDelta time, const PipelineStatusCB& seek_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null());

  // Suppress seeking if we're not fully started.
  if (state_ != kPlaying) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive extra seek in unexpected state: " << state_;

    // TODO(scherkus): should we run the callback?  I'm tempted to say the API
    // will only execute the first Seek() request.
    DVLOG(1) << "Media pipeline has not started, ignoring seek to "
             << time.InMicroseconds() << " (current state: " << state_ << ")";
    return;
  }

  DCHECK(seek_cb_.is_null());

  SetState(kSeeking);
  base::TimeDelta seek_timestamp = std::max(time, demuxer_->GetStartTime());
  seek_cb_ = seek_cb;
  audio_ended_ = false;
  video_ended_ = false;
  text_ended_ = false;

  // Kick off seeking!
  {
    base::AutoLock auto_lock(lock_);
    PauseClockAndStopRendering_Locked();
    clock_->SetTime(seek_timestamp, seek_timestamp);
  }
  DoSeek(seek_timestamp, base::Bind(
      &Pipeline::OnStateTransition, base::Unretained(this)));
}

void Pipeline::DoAudioRendererEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != kPlaying)
    return;

  DCHECK(!audio_ended_);
  audio_ended_ = true;

  // Start clock since there is no more audio to trigger clock updates.
  {
    base::AutoLock auto_lock(lock_);
    clock_->SetMaxTime(clock_->Duration());
    StartClockIfWaitingForTimeUpdate_Locked();
  }

  RunEndedCallbackIfNeeded();
}

void Pipeline::DoVideoRendererEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != kPlaying)
    return;

  DCHECK(!video_ended_);
  video_ended_ = true;

  RunEndedCallbackIfNeeded();
}

void Pipeline::DoTextRendererEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != kPlaying)
    return;

  DCHECK(!text_ended_);
  text_ended_ = true;

  RunEndedCallbackIfNeeded();
}

void Pipeline::RunEndedCallbackIfNeeded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (audio_renderer_ && !audio_ended_)
    return;

  if (video_renderer_ && !video_ended_)
    return;

  if (text_renderer_ && text_renderer_->HasTracks() && !text_ended_)
    return;

  {
    base::AutoLock auto_lock(lock_);
    PauseClockAndStopRendering_Locked();
    clock_->SetTime(clock_->Duration(), clock_->Duration());
  }

  DCHECK_EQ(status_, PIPELINE_OK);
  ended_cb_.Run();
}

void Pipeline::AddTextStreamTask(DemuxerStream* text_stream,
                                 const TextTrackConfig& config) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // TODO(matthewjheaney): fix up text_ended_ when text stream
  // is added (http://crbug.com/321446).
  text_renderer_->AddTextStream(text_stream, config);
}

void Pipeline::RemoveTextStreamTask(DemuxerStream* text_stream) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  text_renderer_->RemoveTextStream(text_stream);
}

void Pipeline::InitializeDemuxer(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  demuxer_ = filter_collection_->GetDemuxer();
  demuxer_->Initialize(this, done_cb, text_renderer_);
}

void Pipeline::InitializeAudioRenderer(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  audio_renderer_ = filter_collection_->GetAudioRenderer();
  audio_renderer_->Initialize(
      demuxer_->GetStream(DemuxerStream::AUDIO),
      done_cb,
      base::Bind(&Pipeline::OnUpdateStatistics, base::Unretained(this)),
      base::Bind(&Pipeline::OnAudioUnderflow, base::Unretained(this)),
      base::Bind(&Pipeline::OnAudioTimeUpdate, base::Unretained(this)),
      base::Bind(&Pipeline::OnAudioRendererEnded, base::Unretained(this)),
      base::Bind(&Pipeline::SetError, base::Unretained(this)));
}

void Pipeline::InitializeVideoRenderer(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  video_renderer_ = filter_collection_->GetVideoRenderer();
  video_renderer_->Initialize(
      demuxer_->GetStream(DemuxerStream::VIDEO),
      demuxer_->GetLiveness() == Demuxer::LIVENESS_LIVE,
      done_cb,
      base::Bind(&Pipeline::OnUpdateStatistics, base::Unretained(this)),
      base::Bind(&Pipeline::OnVideoTimeUpdate, base::Unretained(this)),
      base::Bind(&Pipeline::OnVideoRendererEnded, base::Unretained(this)),
      base::Bind(&Pipeline::SetError, base::Unretained(this)),
      base::Bind(&Pipeline::GetMediaTime, base::Unretained(this)),
      base::Bind(&Pipeline::GetMediaDuration, base::Unretained(this)));
}

void Pipeline::OnAudioUnderflow() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &Pipeline::OnAudioUnderflow, base::Unretained(this)));
    return;
  }

  if (state_ != kPlaying)
    return;

  if (audio_renderer_)
    audio_renderer_->ResumeAfterUnderflow();
}

void Pipeline::BufferingStateChanged(BufferingState* buffering_state,
                                     BufferingState new_buffering_state) {
  DVLOG(1) << __FUNCTION__ << "(" << *buffering_state << ", "
           << " " << new_buffering_state << ") "
           << (buffering_state == &audio_buffering_state_ ? "audio" : "video");
  DCHECK(task_runner_->BelongsToCurrentThread());
  bool was_waiting_for_enough_data = WaitingForEnoughData();
  *buffering_state = new_buffering_state;

  // Renderer underflowed.
  if (!was_waiting_for_enough_data && WaitingForEnoughData()) {
    StartWaitingForEnoughData();
    return;
  }

  // Renderer prerolled.
  if (was_waiting_for_enough_data && !WaitingForEnoughData()) {
    StartPlayback();
    return;
  }
}

bool Pipeline::WaitingForEnoughData() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ != kPlaying)
    return false;
  if (audio_renderer_ && audio_buffering_state_ != BUFFERING_HAVE_ENOUGH)
    return true;
  if (video_renderer_ && video_buffering_state_ != BUFFERING_HAVE_ENOUGH)
    return true;
  return false;
}

void Pipeline::StartWaitingForEnoughData() {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, kPlaying);
  DCHECK(WaitingForEnoughData());

  base::AutoLock auto_lock(lock_);
  PauseClockAndStopRendering_Locked();
}

void Pipeline::StartPlayback() {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, kPlaying);
  DCHECK_EQ(clock_state_, CLOCK_PAUSED);
  DCHECK(!WaitingForEnoughData());

  if (audio_renderer_) {
    // We use audio stream to update the clock. So if there is such a
    // stream, we pause the clock until we receive a valid timestamp.
    base::AutoLock auto_lock(lock_);
    clock_state_ = CLOCK_WAITING_FOR_AUDIO_TIME_UPDATE;
    audio_renderer_->StartRendering();
  } else {
    base::AutoLock auto_lock(lock_);
    clock_state_ = CLOCK_PLAYING;
    clock_->SetMaxTime(clock_->Duration());
    clock_->Play();
  }

  preroll_completed_cb_.Run();
  if (!seek_cb_.is_null())
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
}

void Pipeline::PauseClockAndStopRendering_Locked() {
  lock_.AssertAcquired();
  switch (clock_state_) {
    case CLOCK_PAUSED:
      return;

    case CLOCK_WAITING_FOR_AUDIO_TIME_UPDATE:
      audio_renderer_->StopRendering();
      break;

    case CLOCK_PLAYING:
      if (audio_renderer_)
        audio_renderer_->StopRendering();
      clock_->Pause();
      break;
  }

  clock_state_ = CLOCK_PAUSED;
}

void Pipeline::StartClockIfWaitingForTimeUpdate_Locked() {
  lock_.AssertAcquired();
  if (clock_state_ != CLOCK_WAITING_FOR_AUDIO_TIME_UPDATE)
    return;

  clock_state_ = CLOCK_PLAYING;
  clock_->Play();
}

}  // namespace media
