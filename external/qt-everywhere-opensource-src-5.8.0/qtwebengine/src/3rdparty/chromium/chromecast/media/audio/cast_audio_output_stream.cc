// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_output_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "media/base/decoder_buffer.h"

namespace {
const int kMaxQueuedDataMs = 1000;
}  // namespace

namespace chromecast {
namespace media {

// Backend represents a MediaPipelineBackend adapter.
// It can be created and destroyed on any thread,
// but all other member functions must be called on a single thread.
class CastAudioOutputStream::Backend
    : public MediaPipelineBackend::Decoder::Delegate {
 public:
  using PushBufferCompletionCallback = base::Callback<void(bool)>;

  Backend() : decoder_(nullptr), first_start_(true), error_(false) {
    thread_checker_.DetachFromThread();
  }
  ~Backend() override {}

  bool Open(const ::media::AudioParameters& audio_params,
            CastAudioManager* audio_manager) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(audio_manager);
    DCHECK(backend_ == nullptr);

    backend_task_runner_.reset(new TaskRunnerImpl());
    MediaPipelineDeviceParams device_params(
        MediaPipelineDeviceParams::kModeIgnorePts,
        MediaPipelineDeviceParams::kAudioStreamSoundEffects,
        backend_task_runner_.get());
    backend_ = audio_manager->CreateMediaPipelineBackend(device_params);
    if (!backend_)
      return false;

    decoder_ = backend_->CreateAudioDecoder();
    if (!decoder_)
      return false;
    decoder_->SetDelegate(this);

    AudioConfig audio_config;
    audio_config.codec = kCodecPCM;
    audio_config.sample_format = kSampleFormatS16;
    audio_config.bytes_per_channel = audio_params.bits_per_sample() / 8;
    audio_config.channel_number = audio_params.channels();
    audio_config.samples_per_second = audio_params.sample_rate();
    if (!decoder_->SetConfig(audio_config))
      return false;

    return backend_->Initialize();
  }

  void Close() {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (backend_ && !first_start_)  // Only stop the backend if it was started.
      backend_->Stop();
    backend_.reset();
    backend_task_runner_.reset();
  }

  void Start() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(backend_);

    if (first_start_) {
      first_start_ = false;
      backend_->Start(0);
    } else {
      backend_->Resume();
    }
  }

  void Stop() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(backend_);

    backend_->Pause();
  }

  void PushBuffer(scoped_refptr<media::DecoderBufferBase> decoder_buffer,
                  const PushBufferCompletionCallback& completion_cb) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(decoder_);
    DCHECK(!completion_cb.is_null());
    DCHECK(completion_cb_.is_null());
    if (error_) {
      completion_cb.Run(false);
      return;
    }

    backend_buffer_ = decoder_buffer;
    completion_cb_ = completion_cb;
    BufferStatus status = decoder_->PushBuffer(backend_buffer_.get());
    if (status != MediaPipelineBackend::kBufferPending)
      OnPushBufferComplete(status);
  }

  void SetVolume(double volume) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(decoder_);
    decoder_->SetVolume(volume);
  }

 private:
  // MediaPipelineBackend::Decoder::Delegate implementation
  void OnPushBufferComplete(BufferStatus status) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_NE(status, MediaPipelineBackend::kBufferPending);

    // |completion_cb_| may be null if OnDecoderError was called.
    if (completion_cb_.is_null())
      return;

    base::ResetAndReturn(&completion_cb_)
        .Run(status == MediaPipelineBackend::kBufferSuccess);
  }

  void OnEndOfStream() override {}

  void OnDecoderError() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    error_ = true;
    if (!completion_cb_.is_null())
      OnPushBufferComplete(MediaPipelineBackend::kBufferFailed);
  }

  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override {}

  void OnVideoResolutionChanged(const Size& size) override {}

  std::unique_ptr<MediaPipelineBackend> backend_;
  std::unique_ptr<TaskRunnerImpl> backend_task_runner_;
  MediaPipelineBackend::AudioDecoder* decoder_;
  PushBufferCompletionCallback completion_cb_;
  bool first_start_;
  bool error_;
  scoped_refptr<DecoderBufferBase> backend_buffer_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

// CastAudioOutputStream runs on audio thread (AudioManager::GetTaskRunner).
CastAudioOutputStream::CastAudioOutputStream(
    const ::media::AudioParameters& audio_params,
    CastAudioManager* audio_manager)
    : audio_params_(audio_params),
      audio_manager_(audio_manager),
      volume_(1.0),
      source_callback_(nullptr),
      timestamp_helper_(audio_params_.sample_rate()),
      backend_(new Backend()),
      buffer_duration_(audio_params.GetBufferDuration()),
      push_in_progress_(false),
      weak_factory_(this) {
  VLOG(1) << "CastAudioOutputStream " << this << " created with "
          << audio_params_.AsHumanReadableString();
}

CastAudioOutputStream::~CastAudioOutputStream() {
}

bool CastAudioOutputStream::Open() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  ::media::AudioParameters::Format format = audio_params_.format();
  DCHECK((format == ::media::AudioParameters::AUDIO_PCM_LINEAR) ||
         (format == ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY));

  ::media::ChannelLayout channel_layout = audio_params_.channel_layout();
  if ((channel_layout != ::media::CHANNEL_LAYOUT_MONO) &&
      (channel_layout != ::media::CHANNEL_LAYOUT_STEREO)) {
    LOG(WARNING) << "Unsupported channel layout: " << channel_layout;
    return false;
  }
  DCHECK_GE(audio_params_.channels(), 1);
  DCHECK_LE(audio_params_.channels(), 2);

  if (!backend_->Open(audio_params_, audio_manager_)) {
    LOG(WARNING) << "Failed to create media pipeline backend.";
    return false;
  }

  audio_bus_ = ::media::AudioBus::Create(audio_params_);
  decoder_buffer_ = new DecoderBufferAdapter(
      new ::media::DecoderBuffer(audio_params_.GetBytesPerBuffer()));
  timestamp_helper_.SetBaseTimestamp(base::TimeDelta());

  VLOG(1) << __FUNCTION__ << " : " << this;
  return true;
}

void CastAudioOutputStream::Close() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  VLOG(1) << __FUNCTION__ << " : " << this;

  backend_->Close();
  // Signal to the manager that we're closed and can be removed.
  // This should be the last call in the function as it deletes "this".
  audio_manager_->ReleaseOutputStream(this);
}

void CastAudioOutputStream::Start(AudioSourceCallback* source_callback) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(source_callback);

  source_callback_ = source_callback;
  backend_->Start();

  next_push_time_ = base::TimeTicks::Now();
  if (!push_in_progress_) {
    audio_manager_->GetTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&CastAudioOutputStream::PushBuffer,
                              weak_factory_.GetWeakPtr()));
    push_in_progress_ = true;
  }

  metrics::CastMetricsHelper::GetInstance()->LogTimeToFirstAudio();
}

void CastAudioOutputStream::Stop() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  source_callback_ = nullptr;
  backend_->Stop();
}

void CastAudioOutputStream::SetVolume(double volume) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  volume_ = volume;
  backend_->SetVolume(volume);
}

void CastAudioOutputStream::GetVolume(double* volume) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  *volume = volume_;
}

void CastAudioOutputStream::PushBuffer() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(push_in_progress_);

  if (!source_callback_) {
    push_in_progress_ = false;
    return;
  }

  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta queue_delay =
      std::max(base::TimeDelta(), next_push_time_ - now);
  uint32_t bytes_delay = queue_delay.InMicroseconds() *
                         audio_params_.GetBytesPerSecond() / 1000000;
  int frame_count =
      source_callback_->OnMoreData(audio_bus_.get(), bytes_delay, 0);
  VLOG(3) << "frames_filled=" << frame_count << " with latency=" << bytes_delay;

  DCHECK_EQ(frame_count, audio_bus_->frames());
  DCHECK_EQ(static_cast<int>(decoder_buffer_->data_size()),
            frame_count * audio_params_.GetBytesPerFrame());
  audio_bus_->ToInterleaved(frame_count, audio_params_.bits_per_sample() / 8,
                            decoder_buffer_->writable_data());
  decoder_buffer_->set_timestamp(timestamp_helper_.GetTimestamp());
  timestamp_helper_.AddFrames(frame_count);

  auto completion_cb = base::Bind(&CastAudioOutputStream::OnPushBufferComplete,
                                  weak_factory_.GetWeakPtr());
  backend_->PushBuffer(decoder_buffer_, completion_cb);
}

void CastAudioOutputStream::OnPushBufferComplete(bool success) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(push_in_progress_);

  push_in_progress_ = false;

  if (!source_callback_)
    return;
  if (!success) {
    source_callback_->OnError(this);
    return;
  }

  // Schedule next push buffer. We don't want to allow more than
  // kMaxQueuedDataMs of queued audio.
  const base::TimeTicks now = base::TimeTicks::Now();
  next_push_time_ = std::max(now, next_push_time_ + buffer_duration_);

  base::TimeDelta delay = (next_push_time_ - now) -
                          base::TimeDelta::FromMilliseconds(kMaxQueuedDataMs);
  delay = std::max(delay, base::TimeDelta());

  audio_manager_->GetTaskRunner()->PostDelayedTask(
      FROM_HERE, base::Bind(&CastAudioOutputStream::PushBuffer,
                            weak_factory_.GetWeakPtr()),
      delay);
  push_in_progress_ = true;
}

}  // namespace media
}  // namespace chromecast
