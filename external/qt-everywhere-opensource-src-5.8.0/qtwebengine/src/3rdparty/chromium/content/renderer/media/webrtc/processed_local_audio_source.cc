// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/processed_local_audio_source.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/render_frame_impl.h"
#include "media/audio/sample_rates.h"
#include "media/base/channel_layout.h"
#include "third_party/webrtc/api/mediaconstraintsinterface.h"
#include "third_party/webrtc/media/base/mediachannel.h"

namespace content {

namespace {
// Used as an identifier for ProcessedLocalAudioSource::From().
void* const kClassIdentifier = const_cast<void**>(&kClassIdentifier);
}  // namespace

ProcessedLocalAudioSource::ProcessedLocalAudioSource(
    int consumer_render_frame_id,
    const StreamDeviceInfo& device_info,
    PeerConnectionDependencyFactory* factory)
    : MediaStreamAudioSource(true /* is_local_source */),
      consumer_render_frame_id_(consumer_render_frame_id),
      pc_factory_(factory),
      volume_(0),
      allow_invalid_render_frame_id_for_testing_(false) {
  DCHECK(pc_factory_);
  DVLOG(1) << "ProcessedLocalAudioSource::ProcessedLocalAudioSource()";
  MediaStreamSource::SetDeviceInfo(device_info);
}

ProcessedLocalAudioSource::~ProcessedLocalAudioSource() {
  DVLOG(1) << "ProcessedLocalAudioSource::~ProcessedLocalAudioSource()";
  EnsureSourceIsStopped();
}

// static
ProcessedLocalAudioSource* ProcessedLocalAudioSource::From(
    MediaStreamAudioSource* source) {
  if (source && source->GetClassIdentifier() == kClassIdentifier)
    return static_cast<ProcessedLocalAudioSource*>(source);
  return nullptr;
}

void ProcessedLocalAudioSource::SetSourceConstraints(
    const blink::WebMediaConstraints& constraints) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!constraints.isNull());
  DCHECK(!source_);
  constraints_ = constraints;
}

void* ProcessedLocalAudioSource::GetClassIdentifier() const {
  return kClassIdentifier;
}

bool ProcessedLocalAudioSource::EnsureSourceIsStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());

  {
    base::AutoLock auto_lock(source_lock_);
    if (source_)
      return true;
  }

  // Sanity-check that the consuming RenderFrame still exists. This is required
  // to initialize the audio source.
  if (!allow_invalid_render_frame_id_for_testing_ &&
      !RenderFrameImpl::FromRoutingID(consumer_render_frame_id_)) {
    WebRtcLogMessage("ProcessedLocalAudioSource::EnsureSourceIsStarted() fails "
                     " because the render frame does not exist.");
    return false;
  }

  WebRtcLogMessage(base::StringPrintf(
      "ProcessedLocalAudioSource::EnsureSourceIsStarted. render_frame_id=%d"
      ", channel_layout=%d, sample_rate=%d, buffer_size=%d"
      ", session_id=%d, paired_output_sample_rate=%d"
      ", paired_output_frames_per_buffer=%d, effects=%d. ",
      consumer_render_frame_id_, device_info().device.input.channel_layout,
      device_info().device.input.sample_rate,
      device_info().device.input.frames_per_buffer, device_info().session_id,
      device_info().device.matched_output.sample_rate,
      device_info().device.matched_output.frames_per_buffer,
      device_info().device.input.effects));

  // Sanity-check that the constraints, plus the additional input effects are
  // valid when combined.
  const MediaAudioConstraints audio_constraints(
      constraints_, device_info().device.input.effects);
  if (!audio_constraints.IsValid()) {
    WebRtcLogMessage("ProcessedLocalAudioSource::EnsureSourceIsStarted() fails "
                     " because MediaAudioConstraints are not valid.");
    return false;
  }

  if (device_info().device.input.effects &
      media::AudioParameters::ECHO_CANCELLER) {
    // TODO(hta): Figure out if we should be looking at echoCancellation.
    // Previous code had googEchoCancellation only.
    const blink::BooleanConstraint& echoCancellation =
        constraints_.basic().googEchoCancellation;
    if (echoCancellation.hasExact() && !echoCancellation.exact()) {
      StreamDeviceInfo modified_device_info(device_info());
      modified_device_info.device.input.effects &=
          ~media::AudioParameters::ECHO_CANCELLER;
      SetDeviceInfo(modified_device_info);
    }
  }

  // Create the MediaStreamAudioProcessor, bound to the WebRTC audio device
  // module.
  WebRtcAudioDeviceImpl* const rtc_audio_device =
      pc_factory_->GetWebRtcAudioDevice();
  if (!rtc_audio_device) {
    WebRtcLogMessage("ProcessedLocalAudioSource::EnsureSourceIsStarted() fails "
                     " because there is no WebRtcAudioDeviceImpl instance.");
    return false;
  }
  audio_processor_ = new rtc::RefCountedObject<MediaStreamAudioProcessor>(
      constraints_, device_info().device.input, rtc_audio_device);

  // If KEYBOARD_MIC effect is set, change the layout to the corresponding
  // layout that includes the keyboard mic.
  media::ChannelLayout channel_layout = static_cast<media::ChannelLayout>(
      device_info().device.input.channel_layout);
  if ((device_info().device.input.effects &
       media::AudioParameters::KEYBOARD_MIC) &&
      audio_constraints.GetGoogExperimentalNoiseSuppression()) {
    if (channel_layout == media::CHANNEL_LAYOUT_STEREO) {
      channel_layout = media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC;
      DVLOG(1) << "Changed stereo layout to stereo + keyboard mic layout due "
               << "to KEYBOARD_MIC effect.";
    } else {
      DVLOG(1) << "KEYBOARD_MIC effect ignored, not compatible with layout "
               << channel_layout;
    }
  }

  DVLOG(1) << "Audio input hardware channel layout: " << channel_layout;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout",
                            channel_layout, media::CHANNEL_LAYOUT_MAX + 1);

  // Verify that the reported input channel configuration is supported.
  if (channel_layout != media::CHANNEL_LAYOUT_MONO &&
      channel_layout != media::CHANNEL_LAYOUT_STEREO &&
      channel_layout != media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC) {
    WebRtcLogMessage(base::StringPrintf(
        "ProcessedLocalAudioSource::EnsureSourceIsStarted() fails "
        " because the input channel layout (%d) is not supported.",
        static_cast<int>(channel_layout)));
    return false;
  }

  DVLOG(1) << "Audio input hardware sample rate: "
           << device_info().device.input.sample_rate;
  media::AudioSampleRate asr;
  if (media::ToAudioSampleRate(device_info().device.input.sample_rate, &asr)) {
    UMA_HISTOGRAM_ENUMERATION(
        "WebRTC.AudioInputSampleRate", asr, media::kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS("WebRTC.AudioInputSampleRateUnexpected",
                         device_info().device.input.sample_rate);
  }

  // Determine the audio format required of the AudioCapturerSource. Then, pass
  // that to the |audio_processor_| and set the output format of this
  // ProcessedLocalAudioSource to the processor's output format.
  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
      device_info().device.input.sample_rate, 16,
      GetBufferSize(device_info().device.input.sample_rate));
  params.set_effects(device_info().device.input.effects);
  DCHECK(params.IsValid());
  audio_processor_->OnCaptureFormatChanged(params);
  MediaStreamAudioSource::SetFormat(audio_processor_->OutputFormat());

  // Start the source.
  VLOG(1) << "Starting WebRTC audio source for consumption by render frame "
          << consumer_render_frame_id_ << " with input parameters={"
          << params.AsHumanReadableString() << "} and output parameters={"
          << GetAudioParameters().AsHumanReadableString() << '}';
  scoped_refptr<media::AudioCapturerSource> new_source =
      AudioDeviceFactory::NewAudioCapturerSource(consumer_render_frame_id_);
  new_source->Initialize(params, this, device_info().session_id);
  // We need to set the AGC control before starting the stream.
  new_source->SetAutomaticGainControl(true);
  {
    base::AutoLock auto_lock(source_lock_);
    source_ = std::move(new_source);
  }
  source_->Start();

  // Register this source with the WebRtcAudioDeviceImpl.
  rtc_audio_device->AddAudioCapturer(this);

  return true;
}

void ProcessedLocalAudioSource::EnsureSourceIsStopped() {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<media::AudioCapturerSource> source_to_stop;
  {
    base::AutoLock auto_lock(source_lock_);
    if (!source_)
      return;
    source_to_stop = std::move(source_);
  }

  if (WebRtcAudioDeviceImpl* rtc_audio_device =
      pc_factory_->GetWebRtcAudioDevice()) {
    rtc_audio_device->RemoveAudioCapturer(this);
  }

  source_to_stop->Stop();

  // Stop the audio processor to avoid feeding render data into the processor.
  audio_processor_->Stop();

  VLOG(1) << "Stopped WebRTC audio pipeline for consumption by render frame "
          << consumer_render_frame_id_ << '.';
}

void ProcessedLocalAudioSource::SetVolume(int volume) {
  DVLOG(1) << "ProcessedLocalAudioSource::SetVolume()";
  DCHECK_LE(volume, MaxVolume());

  const double normalized_volume = static_cast<double>(volume) / MaxVolume();

  // Hold a strong reference to |source_| while its SetVolume() method is
  // called. This will prevent the object from being destroyed on another thread
  // in the meantime. It's possible the |source_| will be stopped on another
  // thread while calling SetVolume() here; but this is safe: The operation will
  // simply be ignored.
  scoped_refptr<media::AudioCapturerSource> maybe_source;
  {
    base::AutoLock auto_lock(source_lock_);
    maybe_source = source_;
  }
  if (maybe_source)
    maybe_source->SetVolume(normalized_volume);
}

int ProcessedLocalAudioSource::Volume() const {
  // Note: Using NoBarrier_Load() because the timing of visibility of the
  // updated volume information on other threads can be relaxed.
  return base::subtle::NoBarrier_Load(&volume_);
}

int ProcessedLocalAudioSource::MaxVolume() const {
  return WebRtcAudioDeviceImpl::kMaxVolumeLevel;
}

void ProcessedLocalAudioSource::Capture(const media::AudioBus* audio_bus,
                                        int audio_delay_milliseconds,
                                        double volume,
                                        bool key_pressed) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  DCHECK_LE(volume, 1.0);
#elif (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_OPENBSD)
  // We have a special situation on Linux where the microphone volume can be
  // "higher than maximum". The input volume slider in the sound preference
  // allows the user to set a scaling that is higher than 100%. It means that
  // even if the reported maximum levels is N, the actual microphone level can
  // go up to 1.5x*N and that corresponds to a normalized |volume| of 1.5x.
  DCHECK_LE(volume, 1.6);
#endif

  // TODO(miu): Plumbing is needed to determine the actual capture timestamp
  // of the audio, instead of just snapshotting TimeTicks::Now(), for proper
  // audio/video sync.  http://crbug.com/335335
  const base::TimeTicks reference_clock_snapshot = base::TimeTicks::Now();

  // Map internal volume range of [0.0, 1.0] into [0, 255] used by AGC.
  // The volume can be higher than 255 on Linux, and it will be cropped to
  // 255 since AGC does not allow values out of range.
  int current_volume = static_cast<int>((volume * MaxVolume()) + 0.5);
  // Note: Using NoBarrier_Store() because the timing of visibility of the
  // updated volume information on other threads can be relaxed.
  base::subtle::NoBarrier_Store(&volume_, current_volume);
  current_volume = std::min(current_volume, MaxVolume());

  // Sanity-check the input audio format in debug builds.  Then, notify the
  // tracks if the format has changed.
  //
  // Locking is not needed here to read the audio input/output parameters
  // because the audio processor format changes only occur while audio capture
  // is stopped.
  DCHECK(audio_processor_->InputFormat().IsValid());
  DCHECK_EQ(audio_bus->channels(), audio_processor_->InputFormat().channels());
  DCHECK_EQ(audio_bus->frames(),
            audio_processor_->InputFormat().frames_per_buffer());

  // Figure out if the pre-processed data has any energy or not. This
  // information will be passed to the level calculator to force it to report
  // energy in case the post-processed data is zeroed by the audio processing.
  const bool force_report_nonzero_energy = !audio_bus->AreFramesZero();

  // Push the data to the processor for processing.
  audio_processor_->PushCaptureData(
      *audio_bus,
      base::TimeDelta::FromMilliseconds(audio_delay_milliseconds));

  // Process and consume the data in the processor until there is not enough
  // data in the processor.
  media::AudioBus* processed_data = nullptr;
  base::TimeDelta processed_data_audio_delay;
  int new_volume = 0;
  while (audio_processor_->ProcessAndConsumeData(
             current_volume, key_pressed,
             &processed_data, &processed_data_audio_delay, &new_volume)) {
    DCHECK(processed_data);

    level_calculator_.Calculate(*processed_data, force_report_nonzero_energy);

    MediaStreamAudioSource::DeliverDataToTracks(
        *processed_data, reference_clock_snapshot - processed_data_audio_delay);

    if (new_volume) {
      SetVolume(new_volume);

      // Update the |current_volume| to avoid passing the old volume to AGC.
      current_volume = new_volume;
    }
  }
}

void ProcessedLocalAudioSource::OnCaptureError(const std::string& message) {
  WebRtcLogMessage("ProcessedLocalAudioSource::OnCaptureError: " + message);
}

media::AudioParameters ProcessedLocalAudioSource::GetInputFormat() const {
  return audio_processor_ ? audio_processor_->InputFormat()
                          : media::AudioParameters();
}

int ProcessedLocalAudioSource::GetBufferSize(int sample_rate) const {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(OS_ANDROID)
  // TODO(henrika): Re-evaluate whether to use same logic as other platforms.
  return (2 * sample_rate / 100);
#endif

  // If audio processing is turned on, require 10ms buffers.
  if (audio_processor_->has_audio_processing())
    return (sample_rate / 100);

  // If audio processing is off and the native hardware buffer size was
  // provided, use it. It can be harmful, in terms of CPU/power consumption, to
  // use smaller buffer sizes than the native size (http://crbug.com/362261).
  if (int hardware_buffer_size = device_info().device.input.frames_per_buffer)
    return hardware_buffer_size;

  // If the buffer size is missing from the StreamDeviceInfo, provide 10ms as a
  // fall-back.
  //
  // TODO(miu): Identify where/why the buffer size might be missing, fix the
  // code, and then require it here.
  return (sample_rate / 100);
}

}  // namespace content
