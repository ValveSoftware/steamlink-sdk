// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_impl.h"

#include <math.h>

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_buffer_converter.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/audio_splicer.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/filters/audio_clock.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

namespace {

enum AudioRendererEvent {
  INITIALIZED,
  RENDER_ERROR,
  RENDER_EVENT_MAX = RENDER_ERROR,
};

void HistogramRendererEvent(AudioRendererEvent event) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.AudioRendererEvents", event, RENDER_EVENT_MAX + 1);
}

}  // namespace

AudioRendererImpl::AudioRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    media::AudioRendererSink* sink,
    ScopedVector<AudioDecoder> decoders,
    const SetDecryptorReadyCB& set_decryptor_ready_cb,
    AudioHardwareConfig* hardware_config)
    : task_runner_(task_runner),
      sink_(sink),
      audio_buffer_stream_(task_runner,
                           decoders.Pass(),
                           set_decryptor_ready_cb),
      hardware_config_(hardware_config),
      now_cb_(base::Bind(&base::TimeTicks::Now)),
      state_(kUninitialized),
      rendering_(false),
      sink_playing_(false),
      pending_read_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      preroll_aborted_(false),
      weak_factory_(this) {
  audio_buffer_stream_.set_splice_observer(base::Bind(
      &AudioRendererImpl::OnNewSpliceBuffer, weak_factory_.GetWeakPtr()));
  audio_buffer_stream_.set_config_change_observer(base::Bind(
      &AudioRendererImpl::OnConfigChange, weak_factory_.GetWeakPtr()));
}

AudioRendererImpl::~AudioRendererImpl() {
  // Stop() should have been called and |algorithm_| should have been destroyed.
  DCHECK(state_ == kUninitialized || state_ == kStopped);
  DCHECK(!algorithm_.get());
}

void AudioRendererImpl::StartRendering() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!rendering_);
  rendering_ = true;

  base::AutoLock auto_lock(lock_);
  // Wait for an eventual call to SetPlaybackRate() to start rendering.
  if (algorithm_->playback_rate() == 0) {
    DCHECK(!sink_playing_);
    return;
  }

  StartRendering_Locked();
}

void AudioRendererImpl::StartRendering_Locked() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kPlaying || state_ == kRebuffering || state_ == kUnderflow)
      << "state_=" << state_;
  DCHECK(!sink_playing_);
  DCHECK_NE(algorithm_->playback_rate(), 0);
  lock_.AssertAcquired();

  earliest_end_time_ = now_cb_.Run();
  sink_playing_ = true;

  base::AutoUnlock auto_unlock(lock_);
  sink_->Play();
}

void AudioRendererImpl::StopRendering() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(rendering_);
  rendering_ = false;

  base::AutoLock auto_lock(lock_);
  // Rendering should have already been stopped with a zero playback rate.
  if (algorithm_->playback_rate() == 0) {
    DCHECK(!sink_playing_);
    return;
  }

  StopRendering_Locked();
}

void AudioRendererImpl::StopRendering_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kPlaying || state_ == kRebuffering || state_ == kUnderflow)
      << "state_=" << state_;
  DCHECK(sink_playing_);
  lock_.AssertAcquired();

  sink_playing_ = false;

  base::AutoUnlock auto_unlock(lock_);
  sink_->Pause();
}

void AudioRendererImpl::Flush(const base::Closure& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kPlaying || state_ == kRebuffering || state_ == kUnderflow)
      << "state_=" << state_;
  DCHECK(flush_cb_.is_null());

  flush_cb_ = callback;

  if (pending_read_) {
    ChangeState_Locked(kFlushing);
    return;
  }

  ChangeState_Locked(kFlushed);
  DoFlush_Locked();
}

void AudioRendererImpl::DoFlush_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  DCHECK(!pending_read_);
  DCHECK_EQ(state_, kFlushed);

  audio_buffer_stream_.Reset(base::Bind(&AudioRendererImpl::ResetDecoderDone,
                                        weak_factory_.GetWeakPtr()));
}

void AudioRendererImpl::ResetDecoderDone() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock auto_lock(lock_);
    if (state_ == kStopped)
      return;

    DCHECK_EQ(state_, kFlushed);
    DCHECK(!flush_cb_.is_null());

    audio_clock_.reset(new AudioClock(audio_parameters_.sample_rate()));
    received_end_of_stream_ = false;
    rendered_end_of_stream_ = false;
    preroll_aborted_ = false;

    earliest_end_time_ = now_cb_.Run();
    splicer_->Reset();
    if (buffer_converter_)
      buffer_converter_->Reset();
    algorithm_->FlushBuffers();
  }
  base::ResetAndReturn(&flush_cb_).Run();
}

void AudioRendererImpl::Stop(const base::Closure& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!callback.is_null());

  // TODO(scherkus): Consider invalidating |weak_factory_| and replacing
  // task-running guards that check |state_| with DCHECK().

  {
    base::AutoLock auto_lock(lock_);

    if (state_ == kStopped) {
      task_runner_->PostTask(FROM_HERE, callback);
      return;
    }

    ChangeState_Locked(kStopped);
    algorithm_.reset();
    underflow_cb_.Reset();
    time_cb_.Reset();
    flush_cb_.Reset();
  }

  if (sink_) {
    sink_->Stop();
    sink_ = NULL;
  }

  audio_buffer_stream_.Stop(callback);
}

void AudioRendererImpl::Preroll(base::TimeDelta time,
                                const PipelineStatusCB& cb) {
  DVLOG(1) << __FUNCTION__ << "(" << time.InMicroseconds() << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(!sink_playing_);
  DCHECK_EQ(state_, kFlushed);
  DCHECK(!pending_read_) << "Pending read must complete before seeking";
  DCHECK(preroll_cb_.is_null());

  ChangeState_Locked(kPrerolling);
  preroll_cb_ = cb;
  preroll_timestamp_ = time;

  AttemptRead_Locked();
}

void AudioRendererImpl::Initialize(DemuxerStream* stream,
                                   const PipelineStatusCB& init_cb,
                                   const StatisticsCB& statistics_cb,
                                   const base::Closure& underflow_cb,
                                   const TimeCB& time_cb,
                                   const base::Closure& ended_cb,
                                   const PipelineStatusCB& error_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::AUDIO);
  DCHECK(!init_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!underflow_cb.is_null());
  DCHECK(!time_cb.is_null());
  DCHECK(!ended_cb.is_null());
  DCHECK(!error_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);
  DCHECK(sink_);

  state_ = kInitializing;

  init_cb_ = init_cb;
  underflow_cb_ = underflow_cb;
  time_cb_ = time_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;

  expecting_config_changes_ = stream->SupportsConfigChanges();
  if (!expecting_config_changes_) {
    // The actual buffer size is controlled via the size of the AudioBus
    // provided to Render(), so just choose something reasonable here for looks.
    int buffer_size = stream->audio_decoder_config().samples_per_second() / 100;
    audio_parameters_.Reset(
        AudioParameters::AUDIO_PCM_LOW_LATENCY,
        stream->audio_decoder_config().channel_layout(),
        ChannelLayoutToChannelCount(
            stream->audio_decoder_config().channel_layout()),
        0,
        stream->audio_decoder_config().samples_per_second(),
        stream->audio_decoder_config().bits_per_channel(),
        buffer_size);
    buffer_converter_.reset();
  } else {
    // TODO(rileya): Support hardware config changes
    const AudioParameters& hw_params = hardware_config_->GetOutputConfig();
    audio_parameters_.Reset(
        hw_params.format(),
        // Always use the source's channel layout and channel count to avoid
        // premature downmixing (http://crbug.com/379288), platform specific
        // issues around channel layouts (http://crbug.com/266674), and
        // unnecessary upmixing overhead.
        stream->audio_decoder_config().channel_layout(),
        ChannelLayoutToChannelCount(
            stream->audio_decoder_config().channel_layout()),
        hw_params.input_channels(),
        hw_params.sample_rate(),
        hw_params.bits_per_sample(),
        hardware_config_->GetHighLatencyBufferSize());
  }

  audio_clock_.reset(new AudioClock(audio_parameters_.sample_rate()));

  audio_buffer_stream_.Initialize(
      stream,
      false,
      statistics_cb,
      base::Bind(&AudioRendererImpl::OnAudioBufferStreamInitialized,
                 weak_factory_.GetWeakPtr()));
}

void AudioRendererImpl::OnAudioBufferStreamInitialized(bool success) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);

  if (state_ == kStopped) {
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_ABORT);
    return;
  }

  if (!success) {
    state_ = kUninitialized;
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  if (!audio_parameters_.IsValid()) {
    ChangeState_Locked(kUninitialized);
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  if (expecting_config_changes_)
    buffer_converter_.reset(new AudioBufferConverter(audio_parameters_));
  splicer_.reset(new AudioSplicer(audio_parameters_.sample_rate()));

  // We're all good! Continue initializing the rest of the audio renderer
  // based on the decoder format.
  algorithm_.reset(new AudioRendererAlgorithm());
  algorithm_->Initialize(0, audio_parameters_);

  ChangeState_Locked(kFlushed);

  HistogramRendererEvent(INITIALIZED);

  {
    base::AutoUnlock auto_unlock(lock_);
    sink_->Initialize(audio_parameters_, this);
    sink_->Start();

    // Some sinks play on start...
    sink_->Pause();
  }

  DCHECK(!sink_playing_);

  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void AudioRendererImpl::ResumeAfterUnderflow() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  if (state_ == kUnderflow) {
    // The "!preroll_aborted_" is a hack. If preroll is aborted, then we
    // shouldn't even reach the kUnderflow state to begin with. But for now
    // we're just making sure that the audio buffer capacity (i.e. the
    // number of bytes that need to be buffered for preroll to complete)
    // does not increase due to an aborted preroll.
    // TODO(vrk): Fix this bug correctly! (crbug.com/151352)
    if (!preroll_aborted_)
      algorithm_->IncreaseQueueCapacity();

    ChangeState_Locked(kRebuffering);
  }
}

void AudioRendererImpl::SetVolume(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(sink_);
  sink_->SetVolume(volume);
}

void AudioRendererImpl::DecodedAudioReady(
    AudioBufferStream::Status status,
    const scoped_refptr<AudioBuffer>& buffer) {
  DVLOG(2) << __FUNCTION__ << "(" << status << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(state_ != kUninitialized);

  CHECK(pending_read_);
  pending_read_ = false;

  if (status == AudioBufferStream::ABORTED ||
      status == AudioBufferStream::DEMUXER_READ_ABORTED) {
    HandleAbortedReadOrDecodeError(false);
    return;
  }

  if (status == AudioBufferStream::DECODE_ERROR) {
    HandleAbortedReadOrDecodeError(true);
    return;
  }

  DCHECK_EQ(status, AudioBufferStream::OK);
  DCHECK(buffer.get());

  if (state_ == kFlushing) {
    ChangeState_Locked(kFlushed);
    DoFlush_Locked();
    return;
  }

  if (expecting_config_changes_) {
    DCHECK(buffer_converter_);
    buffer_converter_->AddInput(buffer);
    while (buffer_converter_->HasNextBuffer()) {
      if (!splicer_->AddInput(buffer_converter_->GetNextBuffer())) {
        HandleAbortedReadOrDecodeError(true);
        return;
      }
    }
  } else {
    if (!splicer_->AddInput(buffer)) {
      HandleAbortedReadOrDecodeError(true);
      return;
    }
  }

  if (!splicer_->HasNextBuffer()) {
    AttemptRead_Locked();
    return;
  }

  bool need_another_buffer = false;
  while (splicer_->HasNextBuffer())
    need_another_buffer = HandleSplicerBuffer(splicer_->GetNextBuffer());

  if (!need_another_buffer && !CanRead_Locked())
    return;

  AttemptRead_Locked();
}

bool AudioRendererImpl::HandleSplicerBuffer(
    const scoped_refptr<AudioBuffer>& buffer) {
  if (buffer->end_of_stream()) {
    received_end_of_stream_ = true;

    // Transition to kPlaying if we are currently handling an underflow since
    // no more data will be arriving.
    if (state_ == kUnderflow || state_ == kRebuffering)
      ChangeState_Locked(kPlaying);
  } else {
    if (state_ == kPrerolling) {
      if (IsBeforePrerollTime(buffer))
        return true;

      // Trim off any additional time before the preroll timestamp.
      const base::TimeDelta trim_time =
          preroll_timestamp_ - buffer->timestamp();
      if (trim_time > base::TimeDelta()) {
        buffer->TrimStart(buffer->frame_count() *
                          (static_cast<double>(trim_time.InMicroseconds()) /
                           buffer->duration().InMicroseconds()));
      }
      // If the entire buffer was trimmed, request a new one.
      if (!buffer->frame_count())
        return true;
    }

    if (state_ != kUninitialized && state_ != kStopped)
      algorithm_->EnqueueBuffer(buffer);
  }

  switch (state_) {
    case kUninitialized:
    case kInitializing:
    case kFlushing:
      NOTREACHED();
      return false;

    case kFlushed:
      DCHECK(!pending_read_);
      return false;

    case kPrerolling:
      if (!buffer->end_of_stream() && !algorithm_->IsQueueFull())
        return true;
      ChangeState_Locked(kPlaying);
      base::ResetAndReturn(&preroll_cb_).Run(PIPELINE_OK);
      return false;

    case kPlaying:
    case kUnderflow:
      return false;

    case kRebuffering:
      if (!algorithm_->IsQueueFull())
        return true;
      ChangeState_Locked(kPlaying);
      return false;

    case kStopped:
      return false;
  }
  return false;
}

void AudioRendererImpl::AttemptRead() {
  base::AutoLock auto_lock(lock_);
  AttemptRead_Locked();
}

void AudioRendererImpl::AttemptRead_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (!CanRead_Locked())
    return;

  pending_read_ = true;
  audio_buffer_stream_.Read(base::Bind(&AudioRendererImpl::DecodedAudioReady,
                                       weak_factory_.GetWeakPtr()));
}

bool AudioRendererImpl::CanRead_Locked() {
  lock_.AssertAcquired();

  switch (state_) {
    case kUninitialized:
    case kInitializing:
    case kFlushed:
    case kFlushing:
    case kStopped:
      return false;

    case kPrerolling:
    case kPlaying:
    case kUnderflow:
    case kRebuffering:
      break;
  }

  return !pending_read_ && !received_end_of_stream_ &&
      !algorithm_->IsQueueFull();
}

void AudioRendererImpl::SetPlaybackRate(float playback_rate) {
  DVLOG(1) << __FUNCTION__ << "(" << playback_rate << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_GE(playback_rate, 0);
  DCHECK(sink_);

  base::AutoLock auto_lock(lock_);

  // We have two cases here:
  // Play: current_playback_rate == 0 && playback_rate != 0
  // Pause: current_playback_rate != 0 && playback_rate == 0
  float current_playback_rate = algorithm_->playback_rate();
  algorithm_->SetPlaybackRate(playback_rate);

  if (!rendering_)
    return;

  if (current_playback_rate == 0 && playback_rate != 0) {
    StartRendering_Locked();
    return;
  }

  if (current_playback_rate != 0 && playback_rate == 0) {
    StopRendering_Locked();
    return;
  }
}

bool AudioRendererImpl::IsBeforePrerollTime(
    const scoped_refptr<AudioBuffer>& buffer) {
  DCHECK_EQ(state_, kPrerolling);
  return buffer && !buffer->end_of_stream() &&
         (buffer->timestamp() + buffer->duration()) < preroll_timestamp_;
}

int AudioRendererImpl::Render(AudioBus* audio_bus,
                              int audio_delay_milliseconds) {
  const int requested_frames = audio_bus->frames();
  base::TimeDelta playback_delay = base::TimeDelta::FromMilliseconds(
      audio_delay_milliseconds);
  const int delay_frames = static_cast<int>(playback_delay.InSecondsF() *
                                            audio_parameters_.sample_rate());
  int frames_written = 0;
  base::Closure time_cb;
  base::Closure underflow_cb;
  {
    base::AutoLock auto_lock(lock_);

    // Ensure Stop() hasn't destroyed our |algorithm_| on the pipeline thread.
    if (!algorithm_) {
      audio_clock_->WroteSilence(requested_frames, delay_frames);
      return 0;
    }

    float playback_rate = algorithm_->playback_rate();
    if (playback_rate == 0) {
      audio_clock_->WroteSilence(requested_frames, delay_frames);
      return 0;
    }

    // Mute audio by returning 0 when not playing.
    if (state_ != kPlaying) {
      audio_clock_->WroteSilence(requested_frames, delay_frames);
      return 0;
    }

    // We use the following conditions to determine end of playback:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We received an end of stream buffer
    //   3) We haven't already signalled that we've ended
    //   4) Our estimated earliest end time has expired
    //
    // TODO(enal): we should replace (4) with a check that the browser has no
    // more audio data or at least use a delayed callback.
    //
    // We use the following conditions to determine underflow:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We have NOT received an end of stream buffer
    //   3) We are in the kPlaying state
    //
    // Otherwise the buffer has data we can send to the device.
    const base::TimeDelta media_timestamp_before_filling =
        audio_clock_->CurrentMediaTimestamp();
    if (algorithm_->frames_buffered() > 0) {
      frames_written = algorithm_->FillBuffer(audio_bus, requested_frames);
      audio_clock_->WroteAudio(
          frames_written, delay_frames, playback_rate, algorithm_->GetTime());
    }
    audio_clock_->WroteSilence(requested_frames - frames_written, delay_frames);

    if (frames_written == 0) {
      const base::TimeTicks now = now_cb_.Run();

      if (received_end_of_stream_ && !rendered_end_of_stream_ &&
          now >= earliest_end_time_) {
        rendered_end_of_stream_ = true;
        ended_cb_.Run();
      } else if (!received_end_of_stream_ && state_ == kPlaying) {
        ChangeState_Locked(kUnderflow);
        underflow_cb = underflow_cb_;
      } else {
        // We can't write any data this cycle. For example, we may have
        // sent all available data to the audio device while not reaching
        // |earliest_end_time_|.
      }
    }

    if (CanRead_Locked()) {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&AudioRendererImpl::AttemptRead,
                                        weak_factory_.GetWeakPtr()));
    }

    // We only want to execute |time_cb_| if time has progressed and we haven't
    // signaled end of stream yet.
    if (media_timestamp_before_filling !=
            audio_clock_->CurrentMediaTimestamp() &&
        !rendered_end_of_stream_) {
      time_cb = base::Bind(time_cb_,
                           audio_clock_->CurrentMediaTimestamp(),
                           audio_clock_->last_endpoint_timestamp());
    }

    if (frames_written > 0) {
      UpdateEarliestEndTime_Locked(
          frames_written, playback_delay, now_cb_.Run());
    }
  }

  if (!time_cb.is_null())
    task_runner_->PostTask(FROM_HERE, time_cb);

  if (!underflow_cb.is_null())
    underflow_cb.Run();

  DCHECK_LE(frames_written, requested_frames);
  return frames_written;
}

void AudioRendererImpl::UpdateEarliestEndTime_Locked(
    int frames_filled, const base::TimeDelta& playback_delay,
    const base::TimeTicks& time_now) {
  DCHECK_GT(frames_filled, 0);

  base::TimeDelta predicted_play_time = base::TimeDelta::FromMicroseconds(
      static_cast<float>(frames_filled) * base::Time::kMicrosecondsPerSecond /
      audio_parameters_.sample_rate());

  lock_.AssertAcquired();
  earliest_end_time_ = std::max(
      earliest_end_time_, time_now + playback_delay + predicted_play_time);
}

void AudioRendererImpl::OnRenderError() {
  // UMA data tells us this happens ~0.01% of the time. Trigger an error instead
  // of trying to gracefully fall back to a fake sink. It's very likely
  // OnRenderError() should be removed and the audio stack handle errors without
  // notifying clients. See http://crbug.com/234708 for details.
  HistogramRendererEvent(RENDER_ERROR);
  error_cb_.Run(PIPELINE_ERROR_DECODE);
}

void AudioRendererImpl::HandleAbortedReadOrDecodeError(bool is_decode_error) {
  lock_.AssertAcquired();

  PipelineStatus status = is_decode_error ? PIPELINE_ERROR_DECODE : PIPELINE_OK;
  switch (state_) {
    case kUninitialized:
    case kInitializing:
      NOTREACHED();
      return;
    case kFlushing:
      ChangeState_Locked(kFlushed);

      if (status == PIPELINE_OK) {
        DoFlush_Locked();
        return;
      }

      error_cb_.Run(status);
      base::ResetAndReturn(&flush_cb_).Run();
      return;
    case kPrerolling:
      // This is a signal for abort if it's not an error.
      preroll_aborted_ = !is_decode_error;
      ChangeState_Locked(kPlaying);
      base::ResetAndReturn(&preroll_cb_).Run(status);
      return;
    case kFlushed:
    case kPlaying:
    case kUnderflow:
    case kRebuffering:
    case kStopped:
      if (status != PIPELINE_OK)
        error_cb_.Run(status);
      return;
  }
}

void AudioRendererImpl::ChangeState_Locked(State new_state) {
  DVLOG(1) << __FUNCTION__ << " : " << state_ << " -> " << new_state;
  lock_.AssertAcquired();
  state_ = new_state;
}

void AudioRendererImpl::OnNewSpliceBuffer(base::TimeDelta splice_timestamp) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  splicer_->SetSpliceTimestamp(splice_timestamp);
}

void AudioRendererImpl::OnConfigChange() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(expecting_config_changes_);
  buffer_converter_->ResetTimestampState();
  // Drain flushed buffers from the converter so the AudioSplicer receives all
  // data ahead of any OnNewSpliceBuffer() calls.  Since discontinuities should
  // only appear after config changes, AddInput() should never fail here.
  while (buffer_converter_->HasNextBuffer())
    CHECK(splicer_->AddInput(buffer_converter_->GetNextBuffer()));
}

}  // namespace media
