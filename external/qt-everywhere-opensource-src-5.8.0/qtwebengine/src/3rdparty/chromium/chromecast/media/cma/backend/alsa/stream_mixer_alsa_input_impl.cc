// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa_input_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "media/base/audio_bus.h"
#include "media/base/multi_channel_resampler.h"

#define RUN_ON_MIXER_THREAD(callback, ...)                         \
  if (!mixer_task_runner_->BelongsToCurrentThread()) {             \
    POST_TASK_TO_MIXER_THREAD(&StreamMixerAlsaInputImpl::callback, \
                              ##__VA_ARGS__);                      \
    return;                                                        \
  }

#define POST_TASK_TO_MIXER_THREAD(task, ...) \
  mixer_task_runner_->PostTask(              \
      FROM_HERE, base::Bind(task, base::Unretained(this), ##__VA_ARGS__));

#define RUN_ON_CALLER_THREAD(callback, ...)                         \
  if (!caller_task_runner_->BelongsToCurrentThread()) {             \
    POST_TASK_TO_CALLER_THREAD(&StreamMixerAlsaInputImpl::callback, \
                               ##__VA_ARGS__);                      \
    return;                                                         \
  }

#define POST_TASK_TO_CALLER_THREAD(task, ...) \
  caller_task_runner_->PostTask(FROM_HERE,    \
                                base::Bind(task, weak_this_, ##__VA_ARGS__));

namespace chromecast {
namespace media {

namespace {

const int kNumOutputChannels = 2;
const int64_t kMaxInputQueueUs = 90000;
const int64_t kFadeMs = 15;
// Number of samples to report as readable when paused. When paused, the mixer
// will still pull this many frames each time it tries to write frames, but we
// fill the frames with silence.
const int kPausedReadSamples = 512;
const int kDefaultReadSize = ::media::SincResampler::kDefaultRequestSize;

}  // namespace

StreamMixerAlsaInputImpl::StreamMixerAlsaInputImpl(
    StreamMixerAlsaInput::Delegate* delegate,
    int input_samples_per_second,
    bool primary,
    StreamMixerAlsa* mixer)
    : delegate_(delegate),
      input_samples_per_second_(input_samples_per_second),
      primary_(primary),
      mixer_(mixer),
      mixer_task_runner_(mixer_->task_runner()),
      caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      state_(kStateUninitialized),
      volume_multiplier_(1.0f),
      queued_frames_(0),
      queued_frames_including_resampler_(0),
      current_buffer_offset_(0),
      max_queued_frames_(kMaxInputQueueUs * input_samples_per_second /
                         base::Time::kMicrosecondsPerSecond),
      fade_frames_remaining_(0),
      fade_out_frames_total_(0),
      zeroed_frames_(0),
      weak_factory_(this) {
  DCHECK(delegate_);
  DCHECK(mixer_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

StreamMixerAlsaInputImpl::~StreamMixerAlsaInputImpl() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
}

int StreamMixerAlsaInputImpl::input_samples_per_second() const {
  return input_samples_per_second_;
}

float StreamMixerAlsaInputImpl::volume_multiplier() const {
  return volume_multiplier_;
}

bool StreamMixerAlsaInputImpl::primary() const {
  return primary_;
}

bool StreamMixerAlsaInputImpl::IsDeleting() const {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  return (state_ == kStateFinalFade || state_ == kStateDeleted);
}

void StreamMixerAlsaInputImpl::Initialize(
    const MediaPipelineBackendAlsa::RenderingDelay& mixer_rendering_delay) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(!IsDeleting());
  if (mixer_->output_samples_per_second() != input_samples_per_second_) {
    double resample_ratio = static_cast<double>(input_samples_per_second_) /
                            mixer_->output_samples_per_second();
    resampler_.reset(new ::media::MultiChannelResampler(
        kNumOutputChannels, resample_ratio, kDefaultReadSize,
        base::Bind(&StreamMixerAlsaInputImpl::ReadCB, base::Unretained(this))));
    resampler_->PrimeWithSilence();
  }
  mixer_rendering_delay_ = mixer_rendering_delay;
  fade_out_frames_total_ = NormalFadeFrames();
  fade_frames_remaining_ = NormalFadeFrames();
}

void StreamMixerAlsaInputImpl::PreventDelegateCalls() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();

  base::AutoLock lock(queue_lock_);
  pending_data_ = nullptr;
}

void StreamMixerAlsaInputImpl::PrepareToDelete(
    const OnReadyToDeleteCb& delete_cb) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(!delete_cb.is_null());
  DCHECK(!IsDeleting());
  delete_cb_ = delete_cb;
  if (state_ != kStateNormalPlayback && state_ != kStateFadingOut &&
      state_ != kStateGotEos) {
    DeleteThis();
    return;
  }

  {
    base::AutoLock lock(queue_lock_);
    if (state_ == kStateGotEos) {
      fade_out_frames_total_ = queued_frames_including_resampler_;
      fade_frames_remaining_ = queued_frames_including_resampler_;
    } else if (state_ == kStateNormalPlayback) {
      fade_out_frames_total_ =
          std::min(static_cast<int>(queued_frames_including_resampler_),
                   NormalFadeFrames());
      fade_frames_remaining_ = fade_out_frames_total_;
    }
  }

  state_ = kStateFinalFade;
  if (fade_frames_remaining_ == 0) {
    DeleteThis();
  } else {
    // Tell the mixer that some more data might be available (since when fading
    // out, we can drain the queue completely).
    mixer_->OnFramesQueued();
  }
}

void StreamMixerAlsaInputImpl::DeleteThis() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  state_ = kStateDeleted;
  if (!delete_cb_.is_null())
    base::ResetAndReturn(&delete_cb_).Run(this);
}

void StreamMixerAlsaInputImpl::WritePcm(
    const scoped_refptr<DecoderBufferBase>& data) {
  MediaPipelineBackendAlsa::RenderingDelay delay;
  {
    base::AutoLock lock(queue_lock_);
    if (queued_frames_ >= max_queued_frames_) {
      DCHECK(!pending_data_);
      pending_data_ = data;
      return;
    }
    delay = QueueData(data);
  }
  // Alert the |mixer_| on the mixer thread.
  DidQueueData(data->end_of_stream());
  PostPcmCallback(delay);
}

// Must only be called when queue_lock_ is locked.
MediaPipelineBackendAlsa::RenderingDelay StreamMixerAlsaInputImpl::QueueData(
    const scoped_refptr<DecoderBufferBase>& data) {
  queue_lock_.AssertAcquired();
  if (!data->end_of_stream()) {
    int frames = data->data_size() / (kNumOutputChannels * sizeof(float));
    queue_.push_back(data);
    queued_frames_ += frames;
    queued_frames_including_resampler_ += frames;
  }

  MediaPipelineBackendAlsa::RenderingDelay delay = mixer_rendering_delay_;
  delay.delay_microseconds += static_cast<int64_t>(
      queued_frames_including_resampler_ * base::Time::kMicrosecondsPerSecond /
      input_samples_per_second_);
  return delay;
}

void StreamMixerAlsaInputImpl::PostPcmCallback(
    const MediaPipelineBackendAlsa::RenderingDelay& delay) {
  RUN_ON_CALLER_THREAD(PostPcmCallback, delay);
  delegate_->OnWritePcmCompletion(MediaPipelineBackendAlsa::kBufferSuccess,
                                  delay);
}

void StreamMixerAlsaInputImpl::DidQueueData(bool end_of_stream) {
  RUN_ON_MIXER_THREAD(DidQueueData, end_of_stream);
  DCHECK(!IsDeleting());
  if (end_of_stream) {
    state_ = kStateGotEos;
  } else if (state_ == kStateUninitialized) {
    state_ = kStateNormalPlayback;
  }
  mixer_->OnFramesQueued();
}

void StreamMixerAlsaInputImpl::AfterWriteFrames(
    const MediaPipelineBackendAlsa::RenderingDelay& mixer_rendering_delay) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  double resampler_queued_frames =
      (resampler_ ? resampler_->BufferedFrames() : 0);

  bool queued_more_data = false;
  MediaPipelineBackendAlsa::RenderingDelay total_delay;
  {
    base::AutoLock lock(queue_lock_);
    mixer_rendering_delay_ = mixer_rendering_delay;
    queued_frames_ = 0;
    for (const auto& data : queue_)
      queued_frames_ +=
          data->data_size() / (kNumOutputChannels * sizeof(float));
    queued_frames_ -= current_buffer_offset_;
    DCHECK_GE(queued_frames_, 0);
    queued_frames_including_resampler_ =
        queued_frames_ + resampler_queued_frames;

    if (pending_data_ && queued_frames_ < max_queued_frames_) {
      scoped_refptr<DecoderBufferBase> data = pending_data_;
      pending_data_ = nullptr;
      total_delay = QueueData(data);
      queued_more_data = true;
      if (data->end_of_stream())
        state_ = kStateGotEos;
    }
  }

  if (queued_more_data)
    PostPcmCallback(total_delay);
}

int StreamMixerAlsaInputImpl::MaxReadSize() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  if (state_ == kStatePaused || state_ == kStateDeleted)
    return kPausedReadSamples;
  if (state_ == kStateFinalFade)
    return fade_frames_remaining_;

  int queued_frames;
  {
    base::AutoLock lock(queue_lock_);
    if (state_ == kStateGotEos)
      return std::max(static_cast<int>(queued_frames_including_resampler_),
                      kDefaultReadSize);
    queued_frames = queued_frames_;
  }

  int available_frames = 0;
  if (resampler_) {
    int num_chunks = queued_frames / kDefaultReadSize;
    available_frames = resampler_->ChunkSize() * num_chunks;
  } else {
    available_frames = queued_frames;
  }

  if (state_ == kStateFadingOut)
    return std::min(available_frames, fade_frames_remaining_);
  return std::max(0, available_frames - NormalFadeFrames());
}

void StreamMixerAlsaInputImpl::GetResampledData(::media::AudioBus* dest,
                                                int frames) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(dest);
  DCHECK_EQ(kNumOutputChannels, dest->channels());
  DCHECK_GE(dest->frames(), frames);

  if (state_ == kStatePaused || state_ == kStateDeleted) {
    dest->ZeroFramesPartial(0, frames);
    return;
  }

  if (resampler_) {
    resampler_->Resample(frames, dest);
  } else {
    FillFrames(0, dest, frames);
  }

  if (state_ == kStateFadingOut || state_ == kStateFinalFade) {
    FadeOut(dest, frames);
  } else if (fade_frames_remaining_) {
    FadeIn(dest, frames);
  }
}

// This is the signature expected by the resampler. |output| should be filled
// completely by FillFrames.
void StreamMixerAlsaInputImpl::ReadCB(int frame_delay,
                                      ::media::AudioBus* output) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(output);
  FillFrames(frame_delay, output, output->frames());
}

void StreamMixerAlsaInputImpl::FillFrames(int frame_delay,
                                          ::media::AudioBus* output,
                                          int frames) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(output);
  DCHECK_GE(output->frames(), frames);
  int frames_left = frames;
  int frames_filled = 0;
  while (frames_left) {
    scoped_refptr<DecoderBufferBase> buffer;
    int buffer_frames;
    int frames_to_copy;
    int buffer_offset = current_buffer_offset_;

    {
      base::AutoLock lock(queue_lock_);
      if (!queue_.empty()) {
        buffer = queue_.front();
        buffer_frames =
            buffer->data_size() / (kNumOutputChannels * sizeof(float));
        frames_to_copy =
            std::min(frames_left, buffer_frames - current_buffer_offset_);
        // Note that queued_frames_ is not updated until AfterWriteFrames().
        // This is done so that the rendering delay is always correct from the
        // perspective of the calling thread.
        current_buffer_offset_ += frames_to_copy;
        if (current_buffer_offset_ == buffer_frames) {
          queue_.pop_front();
          current_buffer_offset_ = 0;
        }
      }
    }

    if (buffer) {
      const float* buffer_samples =
          reinterpret_cast<const float*>(buffer->data());
      for (int i = 0; i < kNumOutputChannels; ++i) {
        const float* buffer_channel = buffer_samples + (buffer_frames * i);
        memcpy(output->channel(i) + frames_filled,
               buffer_channel + buffer_offset, frames_to_copy * sizeof(float));
      }
      frames_left -= frames_to_copy;
      frames_filled += frames_to_copy;
      LOG_IF(WARNING, zeroed_frames_ > 0) << "Filled a total of "
                                          << zeroed_frames_ << " frames with 0";
      zeroed_frames_ = 0;
    } else {
      // No data left in queue; fill remaining frames with zeros.
      LOG_IF(WARNING, zeroed_frames_ == 0) << "Starting to fill frames with 0";
      zeroed_frames_ += frames_left;
      output->ZeroFramesPartial(frames_filled, frames_left);
      frames_filled += frames_left;
      frames_left = 0;
      break;
    }
  }

  DCHECK_EQ(0, frames_left);
  DCHECK_EQ(frames, frames_filled);
}

int StreamMixerAlsaInputImpl::NormalFadeFrames() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  int frames = (mixer_->output_samples_per_second() * kFadeMs /
                base::Time::kMillisecondsPerSecond) -
               1;
  return std::max(frames, 0);
}

void StreamMixerAlsaInputImpl::FadeIn(::media::AudioBus* dest, int frames) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << "Fading in, " << fade_frames_remaining_ << " frames remaining";
  float fade_in_frames = mixer_->output_samples_per_second() * kFadeMs /
                         base::Time::kMillisecondsPerSecond;
  for (int f = 0; f < frames && fade_frames_remaining_; ++f) {
    float fade_multiplier = 1.0 - fade_frames_remaining_ / fade_in_frames;
    for (int c = 0; c < kNumOutputChannels; ++c)
      dest->channel(c)[f] *= fade_multiplier;
    --fade_frames_remaining_;
  }
}

void StreamMixerAlsaInputImpl::FadeOut(::media::AudioBus* dest, int frames) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << "Fading out, " << fade_frames_remaining_ << " frames remaining";
  int f = 0;
  for (; f < frames && fade_frames_remaining_; ++f) {
    float fade_multiplier =
        fade_frames_remaining_ / static_cast<float>(fade_out_frames_total_);
    for (int c = 0; c < kNumOutputChannels; ++c)
      dest->channel(c)[f] *= fade_multiplier;
    --fade_frames_remaining_;
  }
  // Zero remaining frames
  for (; f < frames; ++f) {
    for (int c = 0; c < kNumOutputChannels; ++c)
      dest->channel(c)[f] = 0.0f;
  }

  if (fade_frames_remaining_ == 0) {
    if (state_ == kStateFinalFade) {
      DeleteThis();
    } else {
      state_ = kStatePaused;
    }
  }
}

void StreamMixerAlsaInputImpl::SignalError(
    StreamMixerAlsaInput::MixerError error) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  if (state_ == kStateFinalFade) {
    DeleteThis();
    return;
  }
  state_ = kStateError;
  PostError(error);
}

void StreamMixerAlsaInputImpl::PostError(
    StreamMixerAlsaInput::MixerError error) {
  RUN_ON_CALLER_THREAD(PostError, error);
  delegate_->OnMixerError(error);
}

void StreamMixerAlsaInputImpl::SetPaused(bool paused) {
  RUN_ON_MIXER_THREAD(SetPaused, paused);
  DCHECK(!IsDeleting());
  if (paused) {
    if (state_ == kStateUninitialized) {
      state_ = kStatePaused;
    } else if (state_ == kStateNormalPlayback) {
      fade_frames_remaining_ = NormalFadeFrames() - fade_frames_remaining_;
      state_ = (fade_frames_remaining_ ? kStateFadingOut : kStatePaused);
    } else {
      return;
    }
    LOG(INFO) << "Pausing " << this;
  } else {
    if (state_ == kStateFadingOut) {
      fade_frames_remaining_ = NormalFadeFrames() - fade_frames_remaining_;
    } else if (state_ == kStatePaused) {
      fade_frames_remaining_ = NormalFadeFrames();
    } else {
      return;
    }
    LOG(INFO) << "Unpausing " << this;
    state_ = kStateNormalPlayback;
  }
  DCHECK_GE(fade_frames_remaining_, 0);

  if (state_ == kStateFadingOut) {
    // Tell the mixer that some more data might be available (since when fading
    // out, we can drain the queue completely).
    mixer_->OnFramesQueued();
  }
}

void StreamMixerAlsaInputImpl::SetVolumeMultiplier(float multiplier) {
  RUN_ON_MIXER_THREAD(SetVolumeMultiplier, multiplier);
  DCHECK(!IsDeleting());
  if (multiplier > 1.0f)
    multiplier = 1.0f;
  if (multiplier < 0.0f)
    multiplier = 0.0f;
  volume_multiplier_ = multiplier;
}

}  // namespace media
}  // namespace chromecast
