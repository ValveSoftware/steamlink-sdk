// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_resampler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/sample_rates.h"
#include "media/base/audio_converter.h"
#include "media/base/limits.h"

namespace media {

class OnMoreDataConverter
    : public AudioOutputStream::AudioSourceCallback,
      public AudioConverter::InputCallback {
 public:
  OnMoreDataConverter(const AudioParameters& input_params,
                      const AudioParameters& output_params);
  virtual ~OnMoreDataConverter();

  // AudioSourceCallback interface.
  virtual int OnMoreData(AudioBus* dest,
                         AudioBuffersState buffers_state) OVERRIDE;
  virtual void OnError(AudioOutputStream* stream) OVERRIDE;

  // Sets |source_callback_|.  If this is not a new object, then Stop() must be
  // called before Start().
  void Start(AudioOutputStream::AudioSourceCallback* callback);

  // Clears |source_callback_| and flushes the resampler.
  void Stop();

  bool started() { return source_callback_ != NULL; }

 private:
  // AudioConverter::InputCallback implementation.
  virtual double ProvideInput(AudioBus* audio_bus,
                              base::TimeDelta buffer_delay) OVERRIDE;

  // Ratio of input bytes to output bytes used to correct playback delay with
  // regard to buffering and resampling.
  const double io_ratio_;

  // Source callback.
  AudioOutputStream::AudioSourceCallback* source_callback_;

  // Last AudioBuffersState object received via OnMoreData(), used to correct
  // playback delay by ProvideInput() and passed on to |source_callback_|.
  AudioBuffersState current_buffers_state_;

  const int input_bytes_per_second_;

  // Handles resampling, buffering, and channel mixing between input and output
  // parameters.
  AudioConverter audio_converter_;

  DISALLOW_COPY_AND_ASSIGN(OnMoreDataConverter);
};

// Record UMA statistics for hardware output configuration.
static void RecordStats(const AudioParameters& output_params) {
  // Note the 'PRESUBMIT_IGNORE_UMA_MAX's below, these silence the PRESUBMIT.py
  // check for uma enum max usage, since we're abusing UMA_HISTOGRAM_ENUMERATION
  // to report a discrete value.
  UMA_HISTOGRAM_ENUMERATION(
      "Media.HardwareAudioBitsPerChannel",
      output_params.bits_per_sample(),
      limits::kMaxBitsPerSample);  // PRESUBMIT_IGNORE_UMA_MAX
  UMA_HISTOGRAM_ENUMERATION(
      "Media.HardwareAudioChannelLayout", output_params.channel_layout(),
      CHANNEL_LAYOUT_MAX + 1);
  UMA_HISTOGRAM_ENUMERATION(
      "Media.HardwareAudioChannelCount", output_params.channels(),
      limits::kMaxChannels);  // PRESUBMIT_IGNORE_UMA_MAX

  AudioSampleRate asr;
  if (ToAudioSampleRate(output_params.sample_rate(), &asr)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.HardwareAudioSamplesPerSecond", asr, kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS(
        "Media.HardwareAudioSamplesPerSecondUnexpected",
        output_params.sample_rate());
  }
}

// Record UMA statistics for hardware output configuration after fallback.
static void RecordFallbackStats(const AudioParameters& output_params) {
  UMA_HISTOGRAM_BOOLEAN("Media.FallbackToHighLatencyAudioPath", true);
  // Note the 'PRESUBMIT_IGNORE_UMA_MAX's below, these silence the PRESUBMIT.py
  // check for uma enum max usage, since we're abusing UMA_HISTOGRAM_ENUMERATION
  // to report a discrete value.
  UMA_HISTOGRAM_ENUMERATION(
      "Media.FallbackHardwareAudioBitsPerChannel",
      output_params.bits_per_sample(),
      limits::kMaxBitsPerSample);  // PRESUBMIT_IGNORE_UMA_MAX
  UMA_HISTOGRAM_ENUMERATION(
      "Media.FallbackHardwareAudioChannelLayout",
      output_params.channel_layout(), CHANNEL_LAYOUT_MAX + 1);
  UMA_HISTOGRAM_ENUMERATION(
      "Media.FallbackHardwareAudioChannelCount", output_params.channels(),
      limits::kMaxChannels);  // PRESUBMIT_IGNORE_UMA_MAX

  AudioSampleRate asr;
  if (ToAudioSampleRate(output_params.sample_rate(), &asr)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.FallbackHardwareAudioSamplesPerSecond",
        asr, kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS(
        "Media.FallbackHardwareAudioSamplesPerSecondUnexpected",
        output_params.sample_rate());
  }
}

// Converts low latency based |output_params| into high latency appropriate
// output parameters in error situations.
void AudioOutputResampler::SetupFallbackParams() {
// Only Windows has a high latency output driver that is not the same as the low
// latency path.
#if defined(OS_WIN)
  // Choose AudioParameters appropriate for opening the device in high latency
  // mode.  |kMinLowLatencyFrameSize| is arbitrarily based on Pepper Flash's
  // MAXIMUM frame size for low latency.
  static const int kMinLowLatencyFrameSize = 2048;
  const int frames_per_buffer =
      std::max(params_.frames_per_buffer(), kMinLowLatencyFrameSize);

  output_params_ = AudioParameters(
      AudioParameters::AUDIO_PCM_LINEAR, params_.channel_layout(),
      params_.sample_rate(), params_.bits_per_sample(),
      frames_per_buffer);
  device_id_ = "";
  Initialize();
#endif
}

AudioOutputResampler::AudioOutputResampler(AudioManager* audio_manager,
                                           const AudioParameters& input_params,
                                           const AudioParameters& output_params,
                                           const std::string& output_device_id,
                                           const base::TimeDelta& close_delay)
    : AudioOutputDispatcher(audio_manager, input_params, output_device_id),
      close_delay_(close_delay),
      output_params_(output_params),
      streams_opened_(false) {
  DCHECK(input_params.IsValid());
  DCHECK(output_params.IsValid());
  DCHECK_EQ(output_params_.format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);

  // Record UMA statistics for the hardware configuration.
  RecordStats(output_params);

  Initialize();
}

AudioOutputResampler::~AudioOutputResampler() {
  DCHECK(callbacks_.empty());
}

void AudioOutputResampler::Initialize() {
  DCHECK(!streams_opened_);
  DCHECK(callbacks_.empty());
  dispatcher_ = new AudioOutputDispatcherImpl(
      audio_manager_, output_params_, device_id_, close_delay_);
}

bool AudioOutputResampler::OpenStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (dispatcher_->OpenStream()) {
    // Only record the UMA statistic if we didn't fallback during construction
    // and only for the first stream we open.
    if (!streams_opened_ &&
        output_params_.format() == AudioParameters::AUDIO_PCM_LOW_LATENCY) {
      UMA_HISTOGRAM_BOOLEAN("Media.FallbackToHighLatencyAudioPath", false);
    }
    streams_opened_ = true;
    return true;
  }

  // If we've already tried to open the stream in high latency mode or we've
  // successfully opened a stream previously, there's nothing more to be done.
  if (output_params_.format() != AudioParameters::AUDIO_PCM_LOW_LATENCY ||
      streams_opened_ || !callbacks_.empty()) {
    return false;
  }

  DCHECK_EQ(output_params_.format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);

  // Record UMA statistics about the hardware which triggered the failure so
  // we can debug and triage later.
  RecordFallbackStats(output_params_);

  // Only Windows has a high latency output driver that is not the same as the
  // low latency path.
#if defined(OS_WIN)
  DLOG(ERROR) << "Unable to open audio device in low latency mode.  Falling "
              << "back to high latency audio output.";

  SetupFallbackParams();
  if (dispatcher_->OpenStream()) {
    streams_opened_ = true;
    return true;
  }
#endif

  DLOG(ERROR) << "Unable to open audio device in high latency mode.  Falling "
              << "back to fake audio output.";

  // Finally fall back to a fake audio output device.
  output_params_.Reset(
      AudioParameters::AUDIO_FAKE, params_.channel_layout(),
      params_.channels(), params_.input_channels(), params_.sample_rate(),
      params_.bits_per_sample(), params_.frames_per_buffer());
  Initialize();
  if (dispatcher_->OpenStream()) {
    streams_opened_ = true;
    return true;
  }

  return false;
}

bool AudioOutputResampler::StartStream(
    AudioOutputStream::AudioSourceCallback* callback,
    AudioOutputProxy* stream_proxy) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  OnMoreDataConverter* resampler_callback = NULL;
  CallbackMap::iterator it = callbacks_.find(stream_proxy);
  if (it == callbacks_.end()) {
    resampler_callback = new OnMoreDataConverter(params_, output_params_);
    callbacks_[stream_proxy] = resampler_callback;
  } else {
    resampler_callback = it->second;
  }

  resampler_callback->Start(callback);
  bool result = dispatcher_->StartStream(resampler_callback, stream_proxy);
  if (!result)
    resampler_callback->Stop();
  return result;
}

void AudioOutputResampler::StreamVolumeSet(AudioOutputProxy* stream_proxy,
                                           double volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  dispatcher_->StreamVolumeSet(stream_proxy, volume);
}

void AudioOutputResampler::StopStream(AudioOutputProxy* stream_proxy) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  dispatcher_->StopStream(stream_proxy);

  // Now that StopStream() has completed the underlying physical stream should
  // be stopped and no longer calling OnMoreData(), making it safe to Stop() the
  // OnMoreDataConverter.
  CallbackMap::iterator it = callbacks_.find(stream_proxy);
  if (it != callbacks_.end())
    it->second->Stop();
}

void AudioOutputResampler::CloseStream(AudioOutputProxy* stream_proxy) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  dispatcher_->CloseStream(stream_proxy);

  // We assume that StopStream() is always called prior to CloseStream(), so
  // that it is safe to delete the OnMoreDataConverter here.
  CallbackMap::iterator it = callbacks_.find(stream_proxy);
  if (it != callbacks_.end()) {
    delete it->second;
    callbacks_.erase(it);
  }
}

void AudioOutputResampler::Shutdown() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // No AudioOutputProxy objects should hold a reference to us when we get
  // to this stage.
  DCHECK(HasOneRef()) << "Only the AudioManager should hold a reference";

  dispatcher_->Shutdown();
  DCHECK(callbacks_.empty());
}

OnMoreDataConverter::OnMoreDataConverter(const AudioParameters& input_params,
                                         const AudioParameters& output_params)
    : io_ratio_(static_cast<double>(input_params.GetBytesPerSecond()) /
                output_params.GetBytesPerSecond()),
      source_callback_(NULL),
      input_bytes_per_second_(input_params.GetBytesPerSecond()),
      audio_converter_(input_params, output_params, false) {}

OnMoreDataConverter::~OnMoreDataConverter() {
  // Ensure Stop() has been called so we don't end up with an AudioOutputStream
  // calling back into OnMoreData() after destruction.
  CHECK(!source_callback_);
}

void OnMoreDataConverter::Start(
    AudioOutputStream::AudioSourceCallback* callback) {
  CHECK(!source_callback_);
  source_callback_ = callback;

  // While AudioConverter can handle multiple inputs, we're using it only with
  // a single input currently.  Eventually this may be the basis for a browser
  // side mixer.
  audio_converter_.AddInput(this);
}

void OnMoreDataConverter::Stop() {
  CHECK(source_callback_);
  source_callback_ = NULL;
  audio_converter_.RemoveInput(this);
}

int OnMoreDataConverter::OnMoreData(AudioBus* dest,
                                    AudioBuffersState buffers_state) {
  current_buffers_state_ = buffers_state;
  audio_converter_.Convert(dest);

  // Always return the full number of frames requested, ProvideInput()
  // will pad with silence if it wasn't able to acquire enough data.
  return dest->frames();
}

double OnMoreDataConverter::ProvideInput(AudioBus* dest,
                                         base::TimeDelta buffer_delay) {
  // Adjust playback delay to include |buffer_delay|.
  // TODO(dalecurtis): Stop passing bytes around, it doesn't make sense since
  // AudioBus is just float data.  Use TimeDelta instead.
  AudioBuffersState new_buffers_state;
  new_buffers_state.pending_bytes =
      io_ratio_ * (current_buffers_state_.total_bytes() +
                   buffer_delay.InSecondsF() * input_bytes_per_second_);

  // Retrieve data from the original callback.
  const int frames = source_callback_->OnMoreData(dest, new_buffers_state);

  // Zero any unfilled frames if anything was filled, otherwise we'll just
  // return a volume of zero and let AudioConverter drop the output.
  if (frames > 0 && frames < dest->frames())
    dest->ZeroFramesPartial(frames, dest->frames() - frames);
  return frames > 0 ? 1 : 0;
}

void OnMoreDataConverter::OnError(AudioOutputStream* stream) {
  source_callback_->OnError(stream);
}

}  // namespace media
