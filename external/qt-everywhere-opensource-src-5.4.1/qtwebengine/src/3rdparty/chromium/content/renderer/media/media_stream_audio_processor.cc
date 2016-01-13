// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_processor.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_fifo.h"
#include "media/base/channel_layout.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"
#include "third_party/webrtc/modules/audio_processing/typing_detection.h"

namespace content {

namespace {

using webrtc::AudioProcessing;

#if defined(OS_ANDROID)
const int kAudioProcessingSampleRate = 16000;
#else
const int kAudioProcessingSampleRate = 32000;
#endif
const int kAudioProcessingNumberOfChannels = 1;
const AudioProcessing::ChannelLayout kAudioProcessingChannelLayout =
    AudioProcessing::kMono;

const int kMaxNumberOfBuffersInFifo = 2;

// Used by UMA histograms and entries shouldn't be re-ordered or removed.
enum AudioTrackProcessingStates {
  AUDIO_PROCESSING_ENABLED = 0,
  AUDIO_PROCESSING_DISABLED,
  AUDIO_PROCESSING_IN_WEBRTC,
  AUDIO_PROCESSING_MAX
};

void RecordProcessingState(AudioTrackProcessingStates state) {
  UMA_HISTOGRAM_ENUMERATION("Media.AudioTrackProcessingStates",
                            state, AUDIO_PROCESSING_MAX);
}

}  // namespace

class MediaStreamAudioProcessor::MediaStreamAudioConverter
    : public media::AudioConverter::InputCallback {
 public:
  MediaStreamAudioConverter(const media::AudioParameters& source_params,
                            const media::AudioParameters& sink_params)
     : source_params_(source_params),
       sink_params_(sink_params),
       audio_converter_(source_params, sink_params_, false) {
    // An instance of MediaStreamAudioConverter may be created in the main
    // render thread and used in the audio thread, for example, the
    // |MediaStreamAudioProcessor::capture_converter_|.
    thread_checker_.DetachFromThread();
    audio_converter_.AddInput(this);

    // Create and initialize audio fifo and audio bus wrapper.
    // The size of the FIFO should be at least twice of the source buffer size
    // or twice of the sink buffer size. Also, FIFO needs to have enough space
    // to store pre-processed data before passing the data to
    // webrtc::AudioProcessing, which requires 10ms as packet size.
    int max_frame_size = std::max(source_params_.frames_per_buffer(),
                                  sink_params_.frames_per_buffer());
    int buffer_size = std::max(
        kMaxNumberOfBuffersInFifo * max_frame_size,
        kMaxNumberOfBuffersInFifo * source_params_.sample_rate() / 100);
    fifo_.reset(new media::AudioFifo(source_params_.channels(), buffer_size));

    // TODO(xians): Use CreateWrapper to save one memcpy.
    audio_wrapper_ = media::AudioBus::Create(sink_params_.channels(),
                                             sink_params_.frames_per_buffer());
  }

  virtual ~MediaStreamAudioConverter() {
    audio_converter_.RemoveInput(this);
  }

  void Push(const media::AudioBus* audio_source) {
    // Called on the audio thread, which is the capture audio thread for
    // |MediaStreamAudioProcessor::capture_converter_|, and render audio thread
    // for |MediaStreamAudioProcessor::render_converter_|.
    // And it must be the same thread as calling Convert().
    DCHECK(thread_checker_.CalledOnValidThread());
    fifo_->Push(audio_source);
  }

  bool Convert(webrtc::AudioFrame* out, bool audio_mirroring) {
    // Called on the audio thread, which is the capture audio thread for
    // |MediaStreamAudioProcessor::capture_converter_|, and render audio thread
    // for |MediaStreamAudioProcessor::render_converter_|.
    DCHECK(thread_checker_.CalledOnValidThread());
    // Return false if there is not enough data in the FIFO, this happens when
    // fifo_->frames() / source_params_.sample_rate() is less than
    // sink_params.frames_per_buffer() / sink_params.sample_rate().
    if (fifo_->frames() * sink_params_.sample_rate() <
        sink_params_.frames_per_buffer() * source_params_.sample_rate()) {
      return false;
    }

    // Convert data to the output format, this will trigger ProvideInput().
    audio_converter_.Convert(audio_wrapper_.get());
    DCHECK_EQ(audio_wrapper_->frames(), sink_params_.frames_per_buffer());

    // Swap channels before interleaving the data if |audio_mirroring| is
    // set to true.
    if (audio_mirroring &&
        sink_params_.channel_layout() == media::CHANNEL_LAYOUT_STEREO) {
      // Swap the first and second channels.
      audio_wrapper_->SwapChannels(0, 1);
    }

    // TODO(xians): Figure out a better way to handle the interleaved and
    // deinterleaved format switching.
    audio_wrapper_->ToInterleaved(audio_wrapper_->frames(),
                                  sink_params_.bits_per_sample() / 8,
                                  out->data_);

    out->samples_per_channel_ = sink_params_.frames_per_buffer();
    out->sample_rate_hz_ = sink_params_.sample_rate();
    out->speech_type_ = webrtc::AudioFrame::kNormalSpeech;
    out->vad_activity_ = webrtc::AudioFrame::kVadUnknown;
    out->num_channels_ = sink_params_.channels();

    return true;
  }

  const media::AudioParameters& source_parameters() const {
    return source_params_;
  }
  const media::AudioParameters& sink_parameters() const {
    return sink_params_;
  }

 private:
  // AudioConverter::InputCallback implementation.
  virtual double ProvideInput(media::AudioBus* audio_bus,
                              base::TimeDelta buffer_delay) OVERRIDE {
    // Called on realtime audio thread.
    // TODO(xians): Figure out why the first Convert() triggers ProvideInput
    // two times.
    if (fifo_->frames() < audio_bus->frames())
      return 0;

    fifo_->Consume(audio_bus, 0, audio_bus->frames());

    // Return 1.0 to indicate no volume scaling on the data.
    return 1.0;
  }

  base::ThreadChecker thread_checker_;
  const media::AudioParameters source_params_;
  const media::AudioParameters sink_params_;

  // TODO(xians): consider using SincResampler to save some memcpy.
  // Handles mixing and resampling between input and output parameters.
  media::AudioConverter audio_converter_;
  scoped_ptr<media::AudioBus> audio_wrapper_;
  scoped_ptr<media::AudioFifo> fifo_;
};

bool MediaStreamAudioProcessor::IsAudioTrackProcessingEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableAudioTrackProcessing);
}

MediaStreamAudioProcessor::MediaStreamAudioProcessor(
    const blink::WebMediaConstraints& constraints,
    int effects,
    WebRtcPlayoutDataSource* playout_data_source)
    : render_delay_ms_(0),
      playout_data_source_(playout_data_source),
      audio_mirroring_(false),
      typing_detected_(false),
      stopped_(false) {
  capture_thread_checker_.DetachFromThread();
  render_thread_checker_.DetachFromThread();
  InitializeAudioProcessingModule(constraints, effects);
  if (IsAudioTrackProcessingEnabled()) {
    aec_dump_message_filter_ = AecDumpMessageFilter::Get();
    // In unit tests not creating a message filter, |aec_dump_message_filter_|
    // will be NULL. We can just ignore that. Other unit tests and browser tests
    // ensure that we do get the filter when we should.
    if (aec_dump_message_filter_)
      aec_dump_message_filter_->AddDelegate(this);
  }
}

MediaStreamAudioProcessor::~MediaStreamAudioProcessor() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  Stop();
}

void MediaStreamAudioProcessor::OnCaptureFormatChanged(
    const media::AudioParameters& source_params) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // There is no need to hold a lock here since the caller guarantees that
  // there is no more PushCaptureData() and ProcessAndConsumeData() callbacks
  // on the capture thread.
  InitializeCaptureConverter(source_params);

  // Reset the |capture_thread_checker_| since the capture data will come from
  // a new capture thread.
  capture_thread_checker_.DetachFromThread();
}

void MediaStreamAudioProcessor::PushCaptureData(
    const media::AudioBus* audio_source) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(audio_source->channels(),
            capture_converter_->source_parameters().channels());
  DCHECK_EQ(audio_source->frames(),
            capture_converter_->source_parameters().frames_per_buffer());

  capture_converter_->Push(audio_source);
}

bool MediaStreamAudioProcessor::ProcessAndConsumeData(
    base::TimeDelta capture_delay, int volume, bool key_pressed,
    int* new_volume, int16** out) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::ProcessAndConsumeData");

  if (!capture_converter_->Convert(&capture_frame_, audio_mirroring_))
    return false;

  *new_volume = ProcessData(&capture_frame_, capture_delay, volume,
                            key_pressed);
  *out = capture_frame_.data_;

  return true;
}

void MediaStreamAudioProcessor::Stop() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (stopped_)
    return;

  stopped_ = true;

  if (aec_dump_message_filter_) {
    aec_dump_message_filter_->RemoveDelegate(this);
    aec_dump_message_filter_ = NULL;
  }

  if (!audio_processing_.get())
    return;

  StopEchoCancellationDump(audio_processing_.get());

  if (playout_data_source_) {
    playout_data_source_->RemovePlayoutSink(this);
    playout_data_source_ = NULL;
  }
}

const media::AudioParameters& MediaStreamAudioProcessor::InputFormat() const {
  return capture_converter_->source_parameters();
}

const media::AudioParameters& MediaStreamAudioProcessor::OutputFormat() const {
  return capture_converter_->sink_parameters();
}

void MediaStreamAudioProcessor::OnAecDumpFile(
    const IPC::PlatformFileForTransit& file_handle) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::File file = IPC::PlatformFileForTransitToFile(file_handle);
  DCHECK(file.IsValid());

  if (audio_processing_)
    StartEchoCancellationDump(audio_processing_.get(), file.Pass());
  else
    file.Close();
}

void MediaStreamAudioProcessor::OnDisableAecDump() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (audio_processing_)
    StopEchoCancellationDump(audio_processing_.get());
}

void MediaStreamAudioProcessor::OnIpcClosing() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  aec_dump_message_filter_ = NULL;
}

void MediaStreamAudioProcessor::OnPlayoutData(media::AudioBus* audio_bus,
                                              int sample_rate,
                                              int audio_delay_milliseconds) {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  DCHECK(audio_processing_->echo_control_mobile()->is_enabled() ^
         audio_processing_->echo_cancellation()->is_enabled());

  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::OnPlayoutData");
  DCHECK_LT(audio_delay_milliseconds,
            std::numeric_limits<base::subtle::Atomic32>::max());
  base::subtle::Release_Store(&render_delay_ms_, audio_delay_milliseconds);

  InitializeRenderConverterIfNeeded(sample_rate, audio_bus->channels(),
                                    audio_bus->frames());

  render_converter_->Push(audio_bus);
  while (render_converter_->Convert(&render_frame_, false))
    audio_processing_->AnalyzeReverseStream(&render_frame_);
}

void MediaStreamAudioProcessor::OnPlayoutDataSourceChanged() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // There is no need to hold a lock here since the caller guarantees that
  // there is no more OnPlayoutData() callback on the render thread.
  render_thread_checker_.DetachFromThread();
  render_converter_.reset();
}

void MediaStreamAudioProcessor::GetStats(AudioProcessorStats* stats) {
  stats->typing_noise_detected =
      (base::subtle::Acquire_Load(&typing_detected_) != false);
  GetAecStats(audio_processing_.get(), stats);
}

void MediaStreamAudioProcessor::InitializeAudioProcessingModule(
    const blink::WebMediaConstraints& constraints, int effects) {
  DCHECK(!audio_processing_);

  MediaAudioConstraints audio_constraints(constraints, effects);

  // Audio mirroring can be enabled even though audio processing is otherwise
  // disabled.
  audio_mirroring_ = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogAudioMirroring);

  if (!IsAudioTrackProcessingEnabled()) {
    RecordProcessingState(AUDIO_PROCESSING_IN_WEBRTC);
    return;
  }

#if defined(OS_IOS)
  // On iOS, VPIO provides built-in AGC and AEC.
  const bool echo_cancellation = false;
  const bool goog_agc = false;
#else
  const bool echo_cancellation =
      audio_constraints.GetEchoCancellationProperty();
  const bool goog_agc = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogAutoGainControl);
#endif

#if defined(OS_IOS) || defined(OS_ANDROID)
  const bool goog_experimental_aec = false;
  const bool goog_typing_detection = false;
#else
  const bool goog_experimental_aec = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogExperimentalEchoCancellation);
  const bool goog_typing_detection = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogTypingNoiseDetection);
#endif

  const bool goog_ns = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogNoiseSuppression);
  const bool goog_experimental_ns = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogExperimentalNoiseSuppression);
 const bool goog_high_pass_filter = audio_constraints.GetProperty(
     MediaAudioConstraints::kGoogHighpassFilter);

  // Return immediately if no goog constraint is enabled.
  if (!echo_cancellation && !goog_experimental_aec && !goog_ns &&
      !goog_high_pass_filter && !goog_typing_detection &&
      !goog_agc && !goog_experimental_ns) {
    RecordProcessingState(AUDIO_PROCESSING_DISABLED);
    return;
  }

  // Create and configure the webrtc::AudioProcessing.
  audio_processing_.reset(webrtc::AudioProcessing::Create());
  CHECK_EQ(0, audio_processing_->Initialize(kAudioProcessingSampleRate,
                                            kAudioProcessingSampleRate,
                                            kAudioProcessingSampleRate,
                                            kAudioProcessingChannelLayout,
                                            kAudioProcessingChannelLayout,
                                            kAudioProcessingChannelLayout));

  // Enable the audio processing components.
  if (echo_cancellation) {
    EnableEchoCancellation(audio_processing_.get());

    if (goog_experimental_aec)
      EnableExperimentalEchoCancellation(audio_processing_.get());

    if (playout_data_source_)
      playout_data_source_->AddPlayoutSink(this);
  }

  if (goog_ns)
    EnableNoiseSuppression(audio_processing_.get());

  if (goog_experimental_ns)
    EnableExperimentalNoiseSuppression(audio_processing_.get());

  if (goog_high_pass_filter)
    EnableHighPassFilter(audio_processing_.get());

  if (goog_typing_detection) {
    // TODO(xians): Remove this |typing_detector_| after the typing suppression
    // is enabled by default.
    typing_detector_.reset(new webrtc::TypingDetection());
    EnableTypingDetection(audio_processing_.get(), typing_detector_.get());
  }

  if (goog_agc)
    EnableAutomaticGainControl(audio_processing_.get());

  RecordProcessingState(AUDIO_PROCESSING_ENABLED);
}

void MediaStreamAudioProcessor::InitializeCaptureConverter(
    const media::AudioParameters& source_params) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(source_params.IsValid());

  // Create and initialize audio converter for the source data.
  // When the webrtc AudioProcessing is enabled, the sink format of the
  // converter will be the same as the post-processed data format, which is
  // 32k mono for desktops and 16k mono for Android. When the AudioProcessing
  // is disabled, the sink format will be the same as the source format.
  const int sink_sample_rate = audio_processing_ ?
      kAudioProcessingSampleRate : source_params.sample_rate();
  const media::ChannelLayout sink_channel_layout = audio_processing_ ?
      media::GuessChannelLayout(kAudioProcessingNumberOfChannels) :
      source_params.channel_layout();

  // WebRtc AudioProcessing requires 10ms as its packet size. We use this
  // native size when processing is enabled. While processing is disabled, and
  // the source is running with a buffer size smaller than 10ms buffer, we use
  // same buffer size as the incoming format to avoid extra FIFO for WebAudio.
  int sink_buffer_size =  sink_sample_rate / 100;
  if (!audio_processing_ &&
      source_params.frames_per_buffer() < sink_buffer_size) {
    sink_buffer_size = source_params.frames_per_buffer();
  }

  media::AudioParameters sink_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, sink_channel_layout,
      sink_sample_rate, 16, sink_buffer_size);
  capture_converter_.reset(
      new MediaStreamAudioConverter(source_params, sink_params));
}

void MediaStreamAudioProcessor::InitializeRenderConverterIfNeeded(
    int sample_rate, int number_of_channels, int frames_per_buffer) {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  // TODO(xians): Figure out if we need to handle the buffer size change.
  if (render_converter_.get() &&
      render_converter_->source_parameters().sample_rate() == sample_rate &&
      render_converter_->source_parameters().channels() == number_of_channels) {
    // Do nothing if the |render_converter_| has been setup properly.
    return;
  }

  // Create and initialize audio converter for the render data.
  // webrtc::AudioProcessing accepts the same format as what it uses to process
  // capture data, which is 32k mono for desktops and 16k mono for Android.
  media::AudioParameters source_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::GuessChannelLayout(number_of_channels), sample_rate, 16,
      frames_per_buffer);
  media::AudioParameters sink_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_MONO, kAudioProcessingSampleRate, 16,
      kAudioProcessingSampleRate / 100);
  render_converter_.reset(
      new MediaStreamAudioConverter(source_params, sink_params));
  render_data_bus_ = media::AudioBus::Create(number_of_channels,
                                             frames_per_buffer);
}

int MediaStreamAudioProcessor::ProcessData(webrtc::AudioFrame* audio_frame,
                                           base::TimeDelta capture_delay,
                                           int volume,
                                           bool key_pressed) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  if (!audio_processing_)
    return 0;

  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::ProcessData");
  DCHECK_EQ(audio_processing_->input_sample_rate_hz(),
            capture_converter_->sink_parameters().sample_rate());
  DCHECK_EQ(audio_processing_->num_input_channels(),
            capture_converter_->sink_parameters().channels());
  DCHECK_EQ(audio_processing_->num_output_channels(),
            capture_converter_->sink_parameters().channels());

  base::subtle::Atomic32 render_delay_ms =
      base::subtle::Acquire_Load(&render_delay_ms_);
  int64 capture_delay_ms = capture_delay.InMilliseconds();
  DCHECK_LT(capture_delay_ms,
            std::numeric_limits<base::subtle::Atomic32>::max());
  int total_delay_ms =  capture_delay_ms + render_delay_ms;
  if (total_delay_ms > 300) {
    LOG(WARNING) << "Large audio delay, capture delay: " << capture_delay_ms
                 << "ms; render delay: " << render_delay_ms << "ms";
  }

  audio_processing_->set_stream_delay_ms(total_delay_ms);

  DCHECK_LE(volume, WebRtcAudioDeviceImpl::kMaxVolumeLevel);
  webrtc::GainControl* agc = audio_processing_->gain_control();
  int err = agc->set_stream_analog_level(volume);
  DCHECK_EQ(err, 0) << "set_stream_analog_level() error: " << err;

  audio_processing_->set_stream_key_pressed(key_pressed);

  err = audio_processing_->ProcessStream(audio_frame);
  DCHECK_EQ(err, 0) << "ProcessStream() error: " << err;

  if (typing_detector_ &&
      audio_frame->vad_activity_ != webrtc::AudioFrame::kVadUnknown) {
    bool vad_active =
        (audio_frame->vad_activity_ == webrtc::AudioFrame::kVadActive);
    bool typing_detected = typing_detector_->Process(key_pressed, vad_active);
    base::subtle::Release_Store(&typing_detected_, typing_detected);
  }

  // Return 0 if the volume has not been changed, otherwise return the new
  // volume.
  return (agc->stream_analog_level() == volume) ?
      0 : agc->stream_analog_level();
}

}  // namespace content
