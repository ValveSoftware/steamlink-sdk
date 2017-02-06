// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/audio_decoder_alsa.h"

#include <time.h>

#include <algorithm>
#include <limits>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/backend/alsa/media_pipeline_backend_alsa.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

#define TRACE_FUNCTION_ENTRY0() TRACE_EVENT0("cma", __FUNCTION__)

#define TRACE_FUNCTION_ENTRY1(arg1) \
  TRACE_EVENT1("cma", __FUNCTION__, #arg1, arg1)

#define TRACE_FUNCTION_ENTRY2(arg1, arg2) \
  TRACE_EVENT2("cma", __FUNCTION__, #arg1, arg1, #arg2, arg2)

namespace chromecast {
namespace media {

namespace {

const CastAudioDecoder::OutputFormat kDecoderSampleFormat =
    CastAudioDecoder::kOutputPlanarFloat;

const int64_t kInvalidDelayTimestamp = std::numeric_limits<int64_t>::min();

AudioDecoderAlsa::RenderingDelay kInvalidRenderingDelay() {
  AudioDecoderAlsa::RenderingDelay delay;
  delay.timestamp_microseconds = kInvalidDelayTimestamp;
  delay.delay_microseconds = 0;
  return delay;
}

}  // namespace

AudioDecoderAlsa::AudioDecoderAlsa(MediaPipelineBackendAlsa* backend)
    : backend_(backend),
      task_runner_(backend->GetTaskRunner()),
      delegate_(nullptr),
      is_eos_(false),
      error_(false),
      volume_multiplier_(1.0f),
      weak_factory_(this) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(backend_);
  DCHECK(task_runner_.get());
  DCHECK(task_runner_->BelongsToCurrentThread());
}

AudioDecoderAlsa::~AudioDecoderAlsa() {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void AudioDecoderAlsa::SetDelegate(
    MediaPipelineBackend::Decoder::Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

bool AudioDecoderAlsa::Initialize() {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(delegate_);
  stats_ = Statistics();
  is_eos_ = false;
  last_buffer_pts_ = std::numeric_limits<int64_t>::min();

  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
    last_known_delay_.timestamp_microseconds =
        static_cast<int64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
  } else {
    LOG(ERROR) << "Failed to get current timestamp";
    last_known_delay_.timestamp_microseconds = kInvalidDelayTimestamp;
    return false;
  }
  last_known_delay_.delay_microseconds = 0;
  return true;
}

bool AudioDecoderAlsa::Start(int64_t start_pts) {
  TRACE_FUNCTION_ENTRY0();
  current_pts_ = start_pts;
  DCHECK(IsValidConfig(config_));
  mixer_input_.reset(new StreamMixerAlsaInput(
      this, config_.samples_per_second, backend_->Primary()));
  mixer_input_->SetVolumeMultiplier(volume_multiplier_);
  // Create decoder_ if necessary. This can happen if Stop() was called, and
  // SetConfig() was not called since then.
  if (!decoder_)
    CreateDecoder();
  return true;
}

bool AudioDecoderAlsa::Stop() {
  TRACE_FUNCTION_ENTRY0();
  decoder_.reset();
  mixer_input_.reset();
  return Initialize();
}

bool AudioDecoderAlsa::Pause() {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(mixer_input_);
  mixer_input_->SetPaused(true);
  return true;
}

bool AudioDecoderAlsa::Resume() {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(mixer_input_);
  mixer_input_->SetPaused(false);
  return true;
}

AudioDecoderAlsa::BufferStatus AudioDecoderAlsa::PushBuffer(
    CastDecoderBuffer* buffer) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(buffer);
  DCHECK(!is_eos_);
  DCHECK(!error_);

  uint64_t input_bytes = buffer->end_of_stream() ? 0 : buffer->data_size();
  scoped_refptr<DecoderBufferBase> buffer_base(
      static_cast<DecoderBufferBase*>(buffer));
  if (!buffer->end_of_stream()) {
    last_buffer_pts_ = buffer->timestamp();
    current_pts_ = std::min(current_pts_, last_buffer_pts_);
  }

  // If the buffer is already decoded, do not attempt to decode. Call
  // OnBufferDecoded asynchronously on the main thread.
  if (BypassDecoder()) {
    DCHECK(!decoder_);
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&AudioDecoderAlsa::OnBufferDecoded,
                   weak_factory_.GetWeakPtr(), input_bytes,
                   CastAudioDecoder::Status::kDecodeOk, buffer_base));
    return MediaPipelineBackendAlsa::kBufferPending;
  }

  DCHECK(decoder_);
  // Decode the buffer.
  decoder_->Decode(buffer_base,
                   base::Bind(&AudioDecoderAlsa::OnBufferDecoded,
                              weak_factory_.GetWeakPtr(),
                              input_bytes));
  return MediaPipelineBackendAlsa::kBufferPending;
}

void AudioDecoderAlsa::UpdateStatistics(Statistics delta) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  stats_.decoded_bytes += delta.decoded_bytes;
}

void AudioDecoderAlsa::GetStatistics(Statistics* stats) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(stats);
  DCHECK(task_runner_->BelongsToCurrentThread());
  *stats = stats_;
}

bool AudioDecoderAlsa::SetConfig(const AudioConfig& config) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!IsValidConfig(config)) {
    LOG(ERROR) << "Invalid audio config passed to SetConfig";
    return false;
  }

  if (mixer_input_ && config.samples_per_second != config_.samples_per_second) {
    // Destroy the old input first to ensure that the mixer output sample rate
    // is updated.
    mixer_input_.reset();
    mixer_input_.reset(new StreamMixerAlsaInput(
        this, config.samples_per_second, backend_->Primary()));
    mixer_input_->SetVolumeMultiplier(volume_multiplier_);
  }

  config_ = config;
  decoder_.reset();
  CreateDecoder();
  return true;
}

void AudioDecoderAlsa::CreateDecoder() {
  DCHECK(!decoder_);
  DCHECK(IsValidConfig(config_));

  // No need to create a decoder if the samples are already decoded.
  if (BypassDecoder()) {
    LOG(INFO) << "Data is not coded. Decoder will not be used.";
    return;
  }

  // Create a decoder.
  decoder_ = CastAudioDecoder::Create(
      task_runner_,
      config_,
      kDecoderSampleFormat,
      base::Bind(&AudioDecoderAlsa::OnDecoderInitialized,
                 weak_factory_.GetWeakPtr()));
}

bool AudioDecoderAlsa::SetVolume(float multiplier) {
  TRACE_FUNCTION_ENTRY1(multiplier);
  DCHECK(task_runner_->BelongsToCurrentThread());
  volume_multiplier_ = multiplier;
  if (mixer_input_)
    mixer_input_->SetVolumeMultiplier(volume_multiplier_);
  return true;
}

AudioDecoderAlsa::RenderingDelay AudioDecoderAlsa::GetRenderingDelay() {
  TRACE_FUNCTION_ENTRY0();
  return last_known_delay_;
}

void AudioDecoderAlsa::OnDecoderInitialized(bool success) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(task_runner_->BelongsToCurrentThread());
  LOG(INFO) << "Decoder initialization was "
            << (success ? "successful" : "unsuccessful");
  if (!success)
    delegate_->OnDecoderError();
}

void AudioDecoderAlsa::OnBufferDecoded(
    uint64_t input_bytes,
    CastAudioDecoder::Status status,
    const scoped_refptr<DecoderBufferBase>& decoded) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!is_eos_);

  Statistics delta = Statistics();

  if (status == CastAudioDecoder::Status::kDecodeError) {
    LOG(ERROR) << "Decode error";
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&AudioDecoderAlsa::OnWritePcmCompletion,
                                      weak_factory_.GetWeakPtr(),
                                      MediaPipelineBackendAlsa::kBufferFailed,
                                      kInvalidRenderingDelay()));
    UpdateStatistics(delta);
    return;
  }

  delta.decoded_bytes = input_bytes;
  UpdateStatistics(delta);

  if (decoded->end_of_stream())
    is_eos_ = true;

  DCHECK(mixer_input_);
  mixer_input_->WritePcm(decoded);
}

bool AudioDecoderAlsa::BypassDecoder() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // The mixer input requires planar float PCM data.
  return (config_.codec == kCodecPCM &&
          config_.sample_format == kSampleFormatPlanarF32);
}

void AudioDecoderAlsa::OnWritePcmCompletion(BufferStatus status,
                                            const RenderingDelay& delay) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (status == MediaPipelineBackendAlsa::kBufferSuccess && !is_eos_)
    current_pts_ = last_buffer_pts_;
  if (delay.timestamp_microseconds != kInvalidDelayTimestamp)
    last_known_delay_ = delay;
  delegate_->OnPushBufferComplete(status);
  if (is_eos_)
    delegate_->OnEndOfStream();
}

void AudioDecoderAlsa::OnMixerError(MixerError error) {
  TRACE_FUNCTION_ENTRY0();
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (error != MixerError::kInputIgnored)
    LOG(ERROR) << "Mixer error occurred.";
  error_ = true;
  delegate_->OnDecoderError();
}

}  // namespace media
}  // namespace chromecast
