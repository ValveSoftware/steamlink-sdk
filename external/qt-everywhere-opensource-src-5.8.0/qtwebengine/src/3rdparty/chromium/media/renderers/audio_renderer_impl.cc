// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/audio_renderer_impl.h"

#include <math.h>
#include <stddef.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_buffer_converter.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_splicer.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/renderer_client.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/audio_clock.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

AudioRendererImpl::AudioRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    media::AudioRendererSink* sink,
    ScopedVector<AudioDecoder> decoders,
    const scoped_refptr<MediaLog>& media_log)
    : task_runner_(task_runner),
      expecting_config_changes_(false),
      sink_(sink),
      audio_buffer_stream_(
          new AudioBufferStream(task_runner, std::move(decoders), media_log)),
      media_log_(media_log),
      client_(nullptr),
      tick_clock_(new base::DefaultTickClock()),
      last_audio_memory_usage_(0),
      last_decoded_sample_rate_(0),
      playback_rate_(0.0),
      state_(kUninitialized),
      buffering_state_(BUFFERING_HAVE_NOTHING),
      rendering_(false),
      sink_playing_(false),
      pending_read_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      is_suspending_(false),
      weak_factory_(this) {
  audio_buffer_stream_->set_splice_observer(base::Bind(
      &AudioRendererImpl::OnNewSpliceBuffer, weak_factory_.GetWeakPtr()));
  audio_buffer_stream_->set_config_change_observer(base::Bind(
      &AudioRendererImpl::OnConfigChange, weak_factory_.GetWeakPtr()));

// Suspend and resume work differently on Android and are handled at a higher
// level than here. OnSuspend() notifications will be delivered a few seconds
// after an application is backgrounded, even if it should still be playing.
// See http://crbug.com/623066 for more details.
#if !defined(OS_ANDROID)
  // Tests may not have a power monitor.
  base::PowerMonitor* monitor = base::PowerMonitor::Get();
  if (!monitor)
    return;

  // PowerObserver's must be added and removed from the same thread, but we
  // won't remove the observer until we're destructed on |task_runner_| so we
  // must post it here if we're on the wrong thread.
  if (task_runner_->BelongsToCurrentThread()) {
    monitor->AddObserver(this);
  } else {
    // Safe to post this without a WeakPtr because this class must be destructed
    // on the same thread and construction has not completed yet.
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&base::PowerMonitor::AddObserver,
                                      base::Unretained(monitor), this));
  }
#endif
  // Do not add anything below this line since the above actions are only safe
  // as the last lines of the constructor.
}

AudioRendererImpl::~AudioRendererImpl() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
#if !defined(OS_ANDROID)
  if (base::PowerMonitor::Get())
    base::PowerMonitor::Get()->RemoveObserver(this);
#endif

  // If Render() is in progress, this call will wait for Render() to finish.
  // After this call, the |sink_| will not call back into |this| anymore.
  sink_->Stop();

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_ABORT);
}

void AudioRendererImpl::StartTicking() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!rendering_);
  rendering_ = true;

  base::AutoLock auto_lock(lock_);
  // Wait for an eventual call to SetPlaybackRate() to start rendering.
  if (playback_rate_ == 0) {
    DCHECK(!sink_playing_);
    return;
  }

  StartRendering_Locked();
}

void AudioRendererImpl::StartRendering_Locked() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPlaying);
  DCHECK(!sink_playing_);
  DCHECK_NE(playback_rate_, 0.0);
  lock_.AssertAcquired();

  sink_playing_ = true;

  base::AutoUnlock auto_unlock(lock_);
  sink_->Play();
}

void AudioRendererImpl::StopTicking() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(rendering_);
  rendering_ = false;

  base::AutoLock auto_lock(lock_);
  // Rendering should have already been stopped with a zero playback rate.
  if (playback_rate_ == 0) {
    DCHECK(!sink_playing_);
    return;
  }

  StopRendering_Locked();
}

void AudioRendererImpl::StopRendering_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPlaying);
  DCHECK(sink_playing_);
  lock_.AssertAcquired();

  sink_playing_ = false;

  base::AutoUnlock auto_unlock(lock_);
  sink_->Pause();
  stop_rendering_time_ = last_render_time_;
}

void AudioRendererImpl::SetMediaTime(base::TimeDelta time) {
  DVLOG(1) << __FUNCTION__ << "(" << time << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(!rendering_);
  DCHECK_EQ(state_, kFlushed);

  start_timestamp_ = time;
  ended_timestamp_ = kInfiniteDuration();
  last_render_time_ = stop_rendering_time_ = base::TimeTicks();
  first_packet_timestamp_ = kNoTimestamp();
  last_media_timestamp_ = base::TimeDelta();
  audio_clock_.reset(new AudioClock(time, audio_parameters_.sample_rate()));
}

base::TimeDelta AudioRendererImpl::CurrentMediaTime() {
  base::AutoLock auto_lock(lock_);

  // Return the current time based on the known extents of the rendered audio
  // data plus an estimate based on the last time those values were calculated.
  base::TimeDelta current_media_time = audio_clock_->front_timestamp();
  if (!last_render_time_.is_null()) {
    current_media_time +=
        (tick_clock_->NowTicks() - last_render_time_) * playback_rate_;
    if (current_media_time > audio_clock_->back_timestamp())
      current_media_time = audio_clock_->back_timestamp();
  }

  // Clamp current media time to the last reported value, this prevents higher
  // level clients from seeing time go backwards based on inaccurate or spurious
  // delay values reported to the AudioClock.
  //
  // It is expected that such events are transient and will be recovered as
  // rendering continues over time.
  if (current_media_time < last_media_timestamp_) {
    DVLOG(2) << __FUNCTION__ << ": " << last_media_timestamp_
             << " (clamped), actual: " << current_media_time;
    return last_media_timestamp_;
  }

  DVLOG(2) << __FUNCTION__ << ": " << current_media_time;
  last_media_timestamp_ = current_media_time;
  return current_media_time;
}

bool AudioRendererImpl::GetWallClockTimes(
    const std::vector<base::TimeDelta>& media_timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  base::AutoLock auto_lock(lock_);
  DCHECK(wall_clock_times->empty());

  // When playback is paused (rate is zero), assume a rate of 1.0.
  const double playback_rate = playback_rate_ ? playback_rate_ : 1.0;
  const bool is_time_moving = sink_playing_ && playback_rate_ &&
                              !last_render_time_.is_null() &&
                              stop_rendering_time_.is_null() && !is_suspending_;

  // Pre-compute the time until playback of the audio buffer extents, since
  // these values are frequently used below.
  const base::TimeDelta time_until_front =
      audio_clock_->TimeUntilPlayback(audio_clock_->front_timestamp());
  const base::TimeDelta time_until_back =
      audio_clock_->TimeUntilPlayback(audio_clock_->back_timestamp());

  if (media_timestamps.empty()) {
    // Return the current media time as a wall clock time while accounting for
    // frames which may be in the process of play out.
    wall_clock_times->push_back(std::min(
        std::max(tick_clock_->NowTicks(), last_render_time_ + time_until_front),
        last_render_time_ + time_until_back));
    return is_time_moving;
  }

  wall_clock_times->reserve(media_timestamps.size());
  for (const auto& media_timestamp : media_timestamps) {
    // When time was or is moving and the requested media timestamp is within
    // range of played out audio, we can provide an exact conversion.
    if (!last_render_time_.is_null() &&
        media_timestamp >= audio_clock_->front_timestamp() &&
        media_timestamp <= audio_clock_->back_timestamp()) {
      wall_clock_times->push_back(
          last_render_time_ + audio_clock_->TimeUntilPlayback(media_timestamp));
      continue;
    }

    base::TimeDelta base_timestamp, time_until_playback;
    if (media_timestamp < audio_clock_->front_timestamp()) {
      base_timestamp = audio_clock_->front_timestamp();
      time_until_playback = time_until_front;
    } else {
      base_timestamp = audio_clock_->back_timestamp();
      time_until_playback = time_until_back;
    }

    // In practice, most calls will be estimates given the relatively small
    // window in which clients can get the actual time.
    wall_clock_times->push_back(last_render_time_ + time_until_playback +
                                (media_timestamp - base_timestamp) /
                                    playback_rate);
  }

  return is_time_moving;
}

TimeSource* AudioRendererImpl::GetTimeSource() {
  return this;
}

void AudioRendererImpl::Flush(const base::Closure& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPlaying);
  DCHECK(flush_cb_.is_null());

  flush_cb_ = callback;
  ChangeState_Locked(kFlushing);

  if (pending_read_)
    return;

  ChangeState_Locked(kFlushed);
  DoFlush_Locked();
}

void AudioRendererImpl::DoFlush_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  DCHECK(!pending_read_);
  DCHECK_EQ(state_, kFlushed);

  audio_buffer_stream_->Reset(base::Bind(&AudioRendererImpl::ResetDecoderDone,
                                         weak_factory_.GetWeakPtr()));
}

void AudioRendererImpl::ResetDecoderDone() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock auto_lock(lock_);

    DCHECK_EQ(state_, kFlushed);
    DCHECK(!flush_cb_.is_null());

    received_end_of_stream_ = false;
    rendered_end_of_stream_ = false;

    // Flush() may have been called while underflowed/not fully buffered.
    if (buffering_state_ != BUFFERING_HAVE_NOTHING)
      SetBufferingState_Locked(BUFFERING_HAVE_NOTHING);

    splicer_->Reset();
    if (buffer_converter_)
      buffer_converter_->Reset();
    algorithm_->FlushBuffers();
  }

  // Changes in buffering state are always posted. Flush callback must only be
  // run after buffering state has been set back to nothing.
  task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&flush_cb_));
}

void AudioRendererImpl::StartPlaying() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(!sink_playing_);
  DCHECK_EQ(state_, kFlushed);
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);
  DCHECK(!pending_read_) << "Pending read must complete before seeking";

  ChangeState_Locked(kPlaying);
  AttemptRead_Locked();
}

void AudioRendererImpl::Initialize(DemuxerStream* stream,
                                   CdmContext* cdm_context,
                                   RendererClient* client,
                                   const PipelineStatusCB& init_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(client);
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::AUDIO);
  DCHECK(!init_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);
  DCHECK(sink_.get());

  state_ = kInitializing;
  client_ = client;

  // Always post |init_cb_| because |this| could be destroyed if initialization
  // failed.
  init_cb_ = BindToCurrentLoop(init_cb);

  const AudioParameters& hw_params =
      sink_->GetOutputDeviceInfo().output_params();
  expecting_config_changes_ = stream->SupportsConfigChanges();
  if (!expecting_config_changes_ || !hw_params.IsValid() ||
      hw_params.format() == AudioParameters::AUDIO_FAKE) {
    // The actual buffer size is controlled via the size of the AudioBus
    // provided to Render(), so just choose something reasonable here for looks.
    int buffer_size = stream->audio_decoder_config().samples_per_second() / 100;
    audio_parameters_.Reset(
        AudioParameters::AUDIO_PCM_LOW_LATENCY,
        stream->audio_decoder_config().channel_layout(),
        stream->audio_decoder_config().samples_per_second(),
        stream->audio_decoder_config().bits_per_channel(),
        buffer_size);
    buffer_converter_.reset();
  } else {
    // To allow for seamless sample rate adaptations (i.e. changes from say
    // 16kHz to 48kHz), always resample to the hardware rate.
    int sample_rate = hw_params.sample_rate();
    int preferred_buffer_size = hw_params.frames_per_buffer();

#if defined(OS_CHROMEOS)
    // On ChromeOS let the OS level resampler handle resampling unless the
    // initial sample rate is too low; this allows support for sample rate
    // adaptations where necessary.
    if (stream->audio_decoder_config().samples_per_second() >= 44100) {
      sample_rate = stream->audio_decoder_config().samples_per_second();
      preferred_buffer_size = 0;  // No preference.
    }
#endif

    int stream_channel_count = ChannelLayoutToChannelCount(
        stream->audio_decoder_config().channel_layout());

    bool try_supported_channel_layouts = false;
#if defined(OS_WIN)
    try_supported_channel_layouts =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kTrySupportedChannelLayouts);
#endif

    // We don't know how to up-mix for DISCRETE layouts (fancy multichannel
    // hardware with non-standard speaker arrangement). Instead, pretend the
    // hardware layout is stereo and let the OS take care of further up-mixing
    // to the discrete layout (http://crbug.com/266674). Additionally, pretend
    // hardware is stereo whenever kTrySupportedChannelLayouts is set. This flag
    // is for savvy users who want stereo content to output in all surround
    // speakers. Using the actual layout (likely 5.1 or higher) will mean our
    // mixer will attempt to up-mix stereo source streams to just the left/right
    // speaker of the 5.1 setup, nulling out the other channels
    // (http://crbug.com/177872).
    ChannelLayout hw_channel_layout =
        hw_params.channel_layout() == CHANNEL_LAYOUT_DISCRETE ||
                try_supported_channel_layouts
            ? CHANNEL_LAYOUT_STEREO
            : hw_params.channel_layout();
    int hw_channel_count = ChannelLayoutToChannelCount(hw_channel_layout);

    // The layout we pass to |audio_parameters_| will be used for the lifetime
    // of this audio renderer, regardless of changes to hardware and/or stream
    // properties. Below we choose the max of stream layout vs. hardware layout
    // to leave room for changes to the hardware and/or stream (i.e. avoid
    // premature down-mixing - http://crbug.com/379288).
    // If stream_channels < hw_channels:
    //   Taking max means we up-mix to hardware layout. If stream later changes
    //   to have more channels, we aren't locked into down-mixing to the
    //   initial stream layout.
    // If stream_channels > hw_channels:
    //   We choose to output stream's layout, meaning mixing is a no-op for the
    //   renderer. Browser-side will down-mix to the hardware config. If the
    //   hardware later changes to equal stream channels, browser-side will stop
    //   down-mixing and use the data from all stream channels.
    ChannelLayout renderer_channel_layout =
        hw_channel_count > stream_channel_count
            ? hw_channel_layout
            : stream->audio_decoder_config().channel_layout();

    audio_parameters_.Reset(hw_params.format(), renderer_channel_layout,
                            sample_rate, hw_params.bits_per_sample(),
                            media::AudioLatency::GetHighLatencyBufferSize(
                                sample_rate, preferred_buffer_size));
  }

  audio_clock_.reset(
      new AudioClock(base::TimeDelta(), audio_parameters_.sample_rate()));

  audio_buffer_stream_->Initialize(
      stream, base::Bind(&AudioRendererImpl::OnAudioBufferStreamInitialized,
                         weak_factory_.GetWeakPtr()),
      cdm_context, base::Bind(&AudioRendererImpl::OnStatisticsUpdate,
                              weak_factory_.GetWeakPtr()),
      base::Bind(&AudioRendererImpl::OnWaitingForDecryptionKey,
                 weak_factory_.GetWeakPtr()));
}

void AudioRendererImpl::OnAudioBufferStreamInitialized(bool success) {
  DVLOG(1) << __FUNCTION__ << ": " << success;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);

  if (!success) {
    state_ = kUninitialized;
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  if (!audio_parameters_.IsValid()) {
    DVLOG(1) << __FUNCTION__ << ": Invalid audio parameters: "
             << audio_parameters_.AsHumanReadableString();
    ChangeState_Locked(kUninitialized);
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  if (expecting_config_changes_)
    buffer_converter_.reset(new AudioBufferConverter(audio_parameters_));
  splicer_.reset(new AudioSplicer(audio_parameters_.sample_rate(), media_log_));

  // We're all good! Continue initializing the rest of the audio renderer
  // based on the decoder format.
  algorithm_.reset(new AudioRendererAlgorithm());
  algorithm_->Initialize(audio_parameters_);

  ChangeState_Locked(kFlushed);

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

void AudioRendererImpl::OnPlaybackError(PipelineStatus error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnError(error);
}

void AudioRendererImpl::OnPlaybackEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnEnded();
}

void AudioRendererImpl::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnStatisticsUpdate(stats);
}

void AudioRendererImpl::OnBufferingStateChange(BufferingState state) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnBufferingStateChange(state);
}

void AudioRendererImpl::OnWaitingForDecryptionKey() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnWaitingForDecryptionKey();
}

void AudioRendererImpl::SetVolume(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(sink_.get());
  sink_->SetVolume(volume);
}

void AudioRendererImpl::OnSuspend() {
  base::AutoLock auto_lock(lock_);
  is_suspending_ = true;
}

void AudioRendererImpl::OnResume() {
  base::AutoLock auto_lock(lock_);
  is_suspending_ = false;
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
    HandleAbortedReadOrDecodeError(PIPELINE_OK);
    return;
  }

  if (status == AudioBufferStream::DECODE_ERROR) {
    HandleAbortedReadOrDecodeError(PIPELINE_ERROR_DECODE);
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
    if (last_decoded_sample_rate_ &&
        buffer->sample_rate() != last_decoded_sample_rate_) {
      DVLOG(1) << __FUNCTION__ << " Updating audio sample_rate."
               << " ts:" << buffer->timestamp().InMicroseconds()
               << " old:" << last_decoded_sample_rate_
               << " new:" << buffer->sample_rate();
      OnConfigChange();
    }
    last_decoded_sample_rate_ = buffer->sample_rate();

    DCHECK(buffer_converter_);
    buffer_converter_->AddInput(buffer);
    while (buffer_converter_->HasNextBuffer()) {
      if (!splicer_->AddInput(buffer_converter_->GetNextBuffer())) {
        HandleAbortedReadOrDecodeError(AUDIO_RENDERER_ERROR_SPLICE_FAILED);
        return;
      }
    }
  } else {
    // TODO(chcunningham, tguilbert): Figure out if we want to support implicit
    // config changes during src=. Doing so requires resampling each individual
    // stream which is inefficient when there are many tags in a page.
    //
    // Check if the buffer we received matches the expected configuration.
    // Note: We explicitly do not check channel layout here to avoid breaking
    // weird behavior with multichannel wav files: http://crbug.com/600538.
    if (!buffer->end_of_stream() &&
        (buffer->sample_rate() != audio_parameters_.sample_rate() ||
         buffer->channel_count() != audio_parameters_.channels())) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unsupported midstream configuration change!"
          << " Sample Rate: " << buffer->sample_rate() << " vs "
          << audio_parameters_.sample_rate()
          << ", Channels: " << buffer->channel_count() << " vs "
          << audio_parameters_.channels();
      HandleAbortedReadOrDecodeError(PIPELINE_ERROR_DECODE);
      return;
    }

    if (!splicer_->AddInput(buffer)) {
      HandleAbortedReadOrDecodeError(AUDIO_RENDERER_ERROR_SPLICE_FAILED);
      return;
    }
  }

  if (!splicer_->HasNextBuffer()) {
    AttemptRead_Locked();
    return;
  }

  bool need_another_buffer = false;
  while (splicer_->HasNextBuffer())
    need_another_buffer = HandleSplicerBuffer_Locked(splicer_->GetNextBuffer());

  if (!need_another_buffer && !CanRead_Locked())
    return;

  AttemptRead_Locked();
}

bool AudioRendererImpl::HandleSplicerBuffer_Locked(
    const scoped_refptr<AudioBuffer>& buffer) {
  lock_.AssertAcquired();
  if (buffer->end_of_stream()) {
    received_end_of_stream_ = true;
  } else {
    if (state_ == kPlaying) {
      if (IsBeforeStartTime(buffer))
        return true;

      // Trim off any additional time before the start timestamp.
      const base::TimeDelta trim_time = start_timestamp_ - buffer->timestamp();
      if (trim_time > base::TimeDelta()) {
        buffer->TrimStart(buffer->frame_count() *
                          (static_cast<double>(trim_time.InMicroseconds()) /
                           buffer->duration().InMicroseconds()));
      }
      // If the entire buffer was trimmed, request a new one.
      if (!buffer->frame_count())
        return true;
    }

    if (state_ != kUninitialized)
      algorithm_->EnqueueBuffer(buffer);
  }

  // Store the timestamp of the first packet so we know when to start actual
  // audio playback.
  if (first_packet_timestamp_ == kNoTimestamp())
    first_packet_timestamp_ = buffer->timestamp();

  const size_t memory_usage = algorithm_->GetMemoryUsage();
  PipelineStatistics stats;
  stats.audio_memory_usage = memory_usage - last_audio_memory_usage_;
  last_audio_memory_usage_ = memory_usage;
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&AudioRendererImpl::OnStatisticsUpdate,
                                    weak_factory_.GetWeakPtr(), stats));

  switch (state_) {
    case kUninitialized:
    case kInitializing:
    case kFlushing:
      NOTREACHED();
      return false;

    case kFlushed:
      DCHECK(!pending_read_);
      return false;

    case kPlaying:
      if (buffer->end_of_stream() || algorithm_->IsQueueFull()) {
        if (buffering_state_ == BUFFERING_HAVE_NOTHING)
          SetBufferingState_Locked(BUFFERING_HAVE_ENOUGH);
        return false;
      }
      return true;
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
  audio_buffer_stream_->Read(base::Bind(&AudioRendererImpl::DecodedAudioReady,
                                        weak_factory_.GetWeakPtr()));
}

bool AudioRendererImpl::CanRead_Locked() {
  lock_.AssertAcquired();

  switch (state_) {
    case kUninitialized:
    case kInitializing:
    case kFlushing:
    case kFlushed:
      return false;

    case kPlaying:
      break;
  }

  return !pending_read_ && !received_end_of_stream_ &&
      !algorithm_->IsQueueFull();
}

void AudioRendererImpl::SetPlaybackRate(double playback_rate) {
  DVLOG(1) << __FUNCTION__ << "(" << playback_rate << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_GE(playback_rate, 0);
  DCHECK(sink_.get());

  base::AutoLock auto_lock(lock_);

  // We have two cases here:
  // Play: current_playback_rate == 0 && playback_rate != 0
  // Pause: current_playback_rate != 0 && playback_rate == 0
  double current_playback_rate = playback_rate_;
  playback_rate_ = playback_rate;

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

bool AudioRendererImpl::IsBeforeStartTime(
    const scoped_refptr<AudioBuffer>& buffer) {
  DCHECK_EQ(state_, kPlaying);
  return buffer.get() && !buffer->end_of_stream() &&
         (buffer->timestamp() + buffer->duration()) < start_timestamp_;
}

int AudioRendererImpl::Render(AudioBus* audio_bus,
                              uint32_t frames_delayed,
                              uint32_t frames_skipped) {
  const int frames_requested = audio_bus->frames();
  DVLOG(4) << __FUNCTION__ << " frames_delayed:" << frames_delayed
           << " frames_skipped:" << frames_skipped
           << " frames_requested:" << frames_requested;

  int frames_written = 0;
  {
    base::AutoLock auto_lock(lock_);
    last_render_time_ = tick_clock_->NowTicks();

    if (!stop_rendering_time_.is_null()) {
      audio_clock_->CompensateForSuspendedWrites(
          last_render_time_ - stop_rendering_time_, frames_delayed);
      stop_rendering_time_ = base::TimeTicks();
    }

    // Ensure Stop() hasn't destroyed our |algorithm_| on the pipeline thread.
    if (!algorithm_) {
      audio_clock_->WroteAudio(0, frames_requested, frames_delayed,
                               playback_rate_);
      return 0;
    }

    if (playback_rate_ == 0 || is_suspending_) {
      audio_clock_->WroteAudio(0, frames_requested, frames_delayed,
                               playback_rate_);
      return 0;
    }

    // Mute audio by returning 0 when not playing.
    if (state_ != kPlaying) {
      audio_clock_->WroteAudio(0, frames_requested, frames_delayed,
                               playback_rate_);
      return 0;
    }

    // Delay playback by writing silence if we haven't reached the first
    // timestamp yet; this can occur if the video starts before the audio.
    if (algorithm_->frames_buffered() > 0) {
      CHECK_NE(first_packet_timestamp_, kNoTimestamp());
      CHECK_GE(first_packet_timestamp_, base::TimeDelta());
      const base::TimeDelta play_delay =
          first_packet_timestamp_ - audio_clock_->back_timestamp();
      if (play_delay > base::TimeDelta()) {
        DCHECK_EQ(frames_written, 0);

        // Don't multiply |play_delay| out since it can be a huge value on
        // poorly encoded media and multiplying by the sample rate could cause
        // the value to overflow.
        if (play_delay.InSecondsF() > static_cast<double>(frames_requested) /
                                          audio_parameters_.sample_rate()) {
          frames_written = frames_requested;
        } else {
          frames_written =
              play_delay.InSecondsF() * audio_parameters_.sample_rate();
        }

        audio_bus->ZeroFramesPartial(0, frames_written);
      }

      // If there's any space left, actually render the audio; this is where the
      // aural magic happens.
      if (frames_written < frames_requested) {
        frames_written += algorithm_->FillBuffer(
            audio_bus, frames_written, frames_requested - frames_written,
            playback_rate_);
      }
    }

    // We use the following conditions to determine end of playback:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We received an end of stream buffer
    //   3) We haven't already signalled that we've ended
    //   4) We've played all known audio data sent to hardware
    //
    // We use the following conditions to determine underflow:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We have NOT received an end of stream buffer
    //   3) We are in the kPlaying state
    //
    // Otherwise the buffer has data we can send to the device.
    //
    // Per the TimeSource API the media time should always increase even after
    // we've rendered all known audio data. Doing so simplifies scenarios where
    // we have other sources of media data that need to be scheduled after audio
    // data has ended.
    //
    // That being said, we don't want to advance time when underflowed as we
    // know more decoded frames will eventually arrive. If we did, we would
    // throw things out of sync when said decoded frames arrive.
    int frames_after_end_of_stream = 0;
    if (frames_written == 0) {
      if (received_end_of_stream_) {
        if (ended_timestamp_ == kInfiniteDuration())
          ended_timestamp_ = audio_clock_->back_timestamp();
        frames_after_end_of_stream = frames_requested;
      } else if (state_ == kPlaying &&
                 buffering_state_ != BUFFERING_HAVE_NOTHING) {
        algorithm_->IncreaseQueueCapacity();
        SetBufferingState_Locked(BUFFERING_HAVE_NOTHING);
      }
    }

    audio_clock_->WroteAudio(frames_written + frames_after_end_of_stream,
                             frames_requested, frames_delayed, playback_rate_);

    if (CanRead_Locked()) {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&AudioRendererImpl::AttemptRead,
                                        weak_factory_.GetWeakPtr()));
    }

    if (audio_clock_->front_timestamp() >= ended_timestamp_ &&
        !rendered_end_of_stream_) {
      rendered_end_of_stream_ = true;
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&AudioRendererImpl::OnPlaybackEnded,
                                        weak_factory_.GetWeakPtr()));
    }
  }

  DCHECK_LE(frames_written, frames_requested);
  return frames_written;
}

void AudioRendererImpl::OnRenderError() {
  MEDIA_LOG(ERROR, media_log_) << "audio render error";

  // Post to |task_runner_| as this is called on the audio callback thread.
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&AudioRendererImpl::OnPlaybackError,
                            weak_factory_.GetWeakPtr(), AUDIO_RENDERER_ERROR));
}

void AudioRendererImpl::HandleAbortedReadOrDecodeError(PipelineStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

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

      MEDIA_LOG(ERROR, media_log_) << "audio error during flushing, status: "
                                   << MediaLog::PipelineStatusToString(status);
      client_->OnError(status);
      base::ResetAndReturn(&flush_cb_).Run();
      return;

    case kFlushed:
    case kPlaying:
      if (status != PIPELINE_OK) {
        MEDIA_LOG(ERROR, media_log_)
            << "audio error during playing, status: "
            << MediaLog::PipelineStatusToString(status);
        client_->OnError(status);
      }
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

void AudioRendererImpl::SetBufferingState_Locked(
    BufferingState buffering_state) {
  DVLOG(1) << __FUNCTION__ << " : " << buffering_state_ << " -> "
           << buffering_state;
  DCHECK_NE(buffering_state_, buffering_state);
  lock_.AssertAcquired();
  buffering_state_ = buffering_state;

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&AudioRendererImpl::OnBufferingStateChange,
                            weak_factory_.GetWeakPtr(), buffering_state_));
}

}  // namespace media
