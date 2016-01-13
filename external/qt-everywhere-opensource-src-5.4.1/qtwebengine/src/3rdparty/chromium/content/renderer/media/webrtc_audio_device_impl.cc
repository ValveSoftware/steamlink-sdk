// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_device_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/win/windows_version.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/sample_rates.h"

using media::AudioParameters;
using media::ChannelLayout;

namespace content {

WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()
    : ref_count_(0),
      audio_transport_callback_(NULL),
      input_delay_ms_(0),
      output_delay_ms_(0),
      initialized_(false),
      playing_(false),
      recording_(false),
      microphone_volume_(0),
      is_audio_track_processing_enabled_(
          MediaStreamAudioProcessor::IsAudioTrackProcessingEnabled()) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()";
}

WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl()";
  DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
}

int32_t WebRtcAudioDeviceImpl::AddRef() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::subtle::Barrier_AtomicIncrement(&ref_count_, 1);
}

int32_t WebRtcAudioDeviceImpl::Release() {
  DCHECK(thread_checker_.CalledOnValidThread());
  int ret = base::subtle::Barrier_AtomicIncrement(&ref_count_, -1);
  if (ret == 0) {
    delete this;
  }
  return ret;
}
int WebRtcAudioDeviceImpl::OnData(const int16* audio_data,
                                  int sample_rate,
                                  int number_of_channels,
                                  int number_of_frames,
                                  const std::vector<int>& channels,
                                  int audio_delay_milliseconds,
                                  int current_volume,
                                  bool need_audio_processing,
                                  bool key_pressed) {
  int total_delay_ms = 0;
  {
    base::AutoLock auto_lock(lock_);
    // Return immediately when not recording or |channels| is empty.
    // See crbug.com/274017: renderer crash dereferencing invalid channels[0].
    if (!recording_ || channels.empty())
      return 0;

    // Store the reported audio delay locally.
    input_delay_ms_ = audio_delay_milliseconds;
    total_delay_ms = input_delay_ms_ + output_delay_ms_;
    DVLOG(2) << "total delay: " << input_delay_ms_ + output_delay_ms_;
  }

  // Write audio frames in blocks of 10 milliseconds to the registered
  // webrtc::AudioTransport sink. Keep writing until our internal byte
  // buffer is empty.
  const int16* audio_buffer = audio_data;
  const int frames_per_10_ms = (sample_rate / 100);
  CHECK_EQ(number_of_frames % frames_per_10_ms, 0);
  int accumulated_audio_frames = 0;
  uint32_t new_volume = 0;

  // The lock here is to protect a race in the resampler inside webrtc when
  // there are more than one input stream calling OnData(), which can happen
  // when the users setup two getUserMedia, one for the microphone, another
  // for WebAudio. Currently we don't have a better way to fix it except for
  // adding a lock here to sequence the call.
  // TODO(xians): Remove this workaround after we move the
  // webrtc::AudioProcessing module to Chrome. See http://crbug/264611 for
  // details.
  base::AutoLock auto_lock(capture_callback_lock_);
  while (accumulated_audio_frames < number_of_frames) {
    // Deliver 10ms of recorded 16-bit linear PCM audio.
    int new_mic_level = audio_transport_callback_->OnDataAvailable(
        &channels[0],
        channels.size(),
        audio_buffer,
        sample_rate,
        number_of_channels,
        frames_per_10_ms,
        total_delay_ms,
        current_volume,
        key_pressed,
        need_audio_processing);

    accumulated_audio_frames += frames_per_10_ms;
    audio_buffer += frames_per_10_ms * number_of_channels;

    // The latest non-zero new microphone level will be returned.
    if (new_mic_level)
      new_volume = new_mic_level;
  }

  return new_volume;
}

void WebRtcAudioDeviceImpl::OnSetFormat(
    const media::AudioParameters& params) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::OnSetFormat()";
}

void WebRtcAudioDeviceImpl::RenderData(media::AudioBus* audio_bus,
                                       int sample_rate,
                                       int audio_delay_milliseconds,
                                       base::TimeDelta* current_time) {
  render_buffer_.resize(audio_bus->frames() * audio_bus->channels());

  {
    base::AutoLock auto_lock(lock_);
    DCHECK(audio_transport_callback_);
    // Store the reported audio delay locally.
    output_delay_ms_ = audio_delay_milliseconds;
  }

  int frames_per_10_ms = (sample_rate / 100);
  int bytes_per_sample = sizeof(render_buffer_[0]);
  const int bytes_per_10_ms =
      audio_bus->channels() * frames_per_10_ms * bytes_per_sample;
  DCHECK_EQ(audio_bus->frames() % frames_per_10_ms, 0);

  // Get audio frames in blocks of 10 milliseconds from the registered
  // webrtc::AudioTransport source. Keep reading until our internal buffer
  // is full.
  uint32_t num_audio_frames = 0;
  int accumulated_audio_frames = 0;
  int16* audio_data = &render_buffer_[0];
  while (accumulated_audio_frames < audio_bus->frames()) {
    // Get 10ms and append output to temporary byte buffer.
    int64_t elapsed_time_ms = -1;
    int64_t ntp_time_ms = -1;
    if (is_audio_track_processing_enabled_) {
      // When audio processing is enabled in the audio track, we use
      // PullRenderData() instead of NeedMorePlayData() to avoid passing the
      // render data to the APM in WebRTC as reference signal for echo
      // cancellation.
      static const int kBitsPerByte = 8;
      audio_transport_callback_->PullRenderData(bytes_per_sample * kBitsPerByte,
                                                sample_rate,
                                                audio_bus->channels(),
                                                frames_per_10_ms,
                                                audio_data,
                                                &elapsed_time_ms,
                                                &ntp_time_ms);
      accumulated_audio_frames += frames_per_10_ms;
    } else {
      // TODO(xians): Remove the following code after the APM in WebRTC is
      // deprecated.
      audio_transport_callback_->NeedMorePlayData(frames_per_10_ms,
                                                  bytes_per_sample,
                                                  audio_bus->channels(),
                                                  sample_rate,
                                                  audio_data,
                                                  num_audio_frames,
                                                  &elapsed_time_ms,
                                                  &ntp_time_ms);
      accumulated_audio_frames += num_audio_frames;
    }
    if (elapsed_time_ms >= 0) {
      *current_time = base::TimeDelta::FromMilliseconds(elapsed_time_ms);
    }
    audio_data += bytes_per_10_ms;
  }

  // De-interleave each channel and convert to 32-bit floating-point
  // with nominal range -1.0 -> +1.0 to match the callback format.
  audio_bus->FromInterleaved(&render_buffer_[0],
                             audio_bus->frames(),
                             bytes_per_sample);

  // Pass the render data to the playout sinks.
  base::AutoLock auto_lock(lock_);
  for (PlayoutDataSinkList::const_iterator it = playout_sinks_.begin();
       it != playout_sinks_.end(); ++it) {
    (*it)->OnPlayoutData(audio_bus, sample_rate, audio_delay_milliseconds);
  }
}

void WebRtcAudioDeviceImpl::RemoveAudioRenderer(WebRtcAudioRenderer* renderer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(renderer, renderer_);
  base::AutoLock auto_lock(lock_);
  // Notify the playout sink of the change.
  for (PlayoutDataSinkList::const_iterator it = playout_sinks_.begin();
       it != playout_sinks_.end(); ++it) {
    (*it)->OnPlayoutDataSourceChanged();
  }

  renderer_ = NULL;
  playing_ = false;
}

int32_t WebRtcAudioDeviceImpl::RegisterAudioCallback(
    webrtc::AudioTransport* audio_callback) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::RegisterAudioCallback()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(audio_transport_callback_ == NULL, audio_callback != NULL);
  audio_transport_callback_ = audio_callback;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::Init() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::Init()";
  DCHECK(thread_checker_.CalledOnValidThread());

  // We need to return a success to continue the initialization of WebRtc VoE
  // because failure on the capturer_ initialization should not prevent WebRTC
  // from working. See issue http://crbug.com/144421 for details.
  initialized_ = true;

  return 0;
}

int32_t WebRtcAudioDeviceImpl::Terminate() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::Terminate()";
  DCHECK(thread_checker_.CalledOnValidThread());

  // Calling Terminate() multiple times in a row is OK.
  if (!initialized_)
    return 0;

  StopRecording();
  StopPlayout();

  DCHECK(!renderer_.get() || !renderer_->IsStarted())
      << "The shared audio renderer shouldn't be running";

  // Stop all the capturers to ensure no further OnData() and
  // RemoveAudioCapturer() callback.
  // Cache the capturers in a local list since WebRtcAudioCapturer::Stop()
  // will trigger RemoveAudioCapturer() callback.
  CapturerList capturers;
  capturers.swap(capturers_);
  for (CapturerList::const_iterator iter = capturers.begin();
       iter != capturers.end(); ++iter) {
    (*iter)->Stop();
  }

  initialized_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Initialized() const {
  return initialized_;
}

int32_t WebRtcAudioDeviceImpl::PlayoutIsAvailable(bool* available) {
  *available = initialized_;
  return 0;
}

bool WebRtcAudioDeviceImpl::PlayoutIsInitialized() const {
  return initialized_;
}

int32_t WebRtcAudioDeviceImpl::RecordingIsAvailable(bool* available) {
  *available = (!capturers_.empty());
  return 0;
}

bool WebRtcAudioDeviceImpl::RecordingIsInitialized() const {
  DVLOG(1) << "WebRtcAudioDeviceImpl::RecordingIsInitialized()";
  DCHECK(thread_checker_.CalledOnValidThread());
  return (!capturers_.empty());
}

int32_t WebRtcAudioDeviceImpl::StartPlayout() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StartPlayout()";
  LOG_IF(ERROR, !audio_transport_callback_) << "Audio transport is missing";
  {
    base::AutoLock auto_lock(lock_);
    if (!audio_transport_callback_)
      return 0;
  }

  if (playing_) {
    // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
    // that the call is ignored the second time.
    return 0;
  }

  playing_ = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopPlayout() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StopPlayout()";
  if (!playing_) {
    // webrtc::VoiceEngine assumes that it is OK to call Stop() just in case.
    return 0;
  }

  playing_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Playing() const {
  return playing_;
}

int32_t WebRtcAudioDeviceImpl::StartRecording() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StartRecording()";
  DCHECK(initialized_);
  LOG_IF(ERROR, !audio_transport_callback_) << "Audio transport is missing";
  if (!audio_transport_callback_) {
    return -1;
  }

  {
    base::AutoLock auto_lock(lock_);
    if (recording_)
      return 0;

    recording_ = true;
  }

  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopRecording() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StopRecording()";
  {
    base::AutoLock auto_lock(lock_);
    if (!recording_)
      return 0;

    recording_ = false;
  }

  return 0;
}

bool WebRtcAudioDeviceImpl::Recording() const {
  base::AutoLock auto_lock(lock_);
  return recording_;
}

int32_t WebRtcAudioDeviceImpl::SetMicrophoneVolume(uint32_t volume) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::SetMicrophoneVolume(" << volume << ")";
  DCHECK(initialized_);

  // Only one microphone is supported at the moment, which is represented by
  // the default capturer.
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  capturer->SetVolume(volume);
  return 0;
}

// TODO(henrika): sort out calling thread once we start using this API.
int32_t WebRtcAudioDeviceImpl::MicrophoneVolume(uint32_t* volume) const {
  DVLOG(1) << "WebRtcAudioDeviceImpl::MicrophoneVolume()";
  // We only support one microphone now, which is accessed via the default
  // capturer.
  DCHECK(initialized_);
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  *volume = static_cast<uint32_t>(capturer->Volume());

  return 0;
}

int32_t WebRtcAudioDeviceImpl::MaxMicrophoneVolume(uint32_t* max_volume) const {
  DCHECK(initialized_);
  *max_volume = kMaxVolumeLevel;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::MinMicrophoneVolume(uint32_t* min_volume) const {
  *min_volume = 0;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoPlayoutIsAvailable(bool* available) const {
  DCHECK(initialized_);
  *available = renderer_ && renderer_->channels() == 2;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StereoRecordingIsAvailable(
    bool* available) const {
  DCHECK(initialized_);
  // TODO(xians): These kind of hardware methods do not make much sense since we
  // support multiple sources. Remove or figure out new APIs for such methods.
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  *available = (capturer->source_audio_parameters().channels() == 2);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::PlayoutDelay(uint16_t* delay_ms) const {
  base::AutoLock auto_lock(lock_);
  *delay_ms = static_cast<uint16_t>(output_delay_ms_);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::RecordingDelay(uint16_t* delay_ms) const {
  base::AutoLock auto_lock(lock_);
  *delay_ms = static_cast<uint16_t>(input_delay_ms_);
  return 0;
}

int32_t WebRtcAudioDeviceImpl::RecordingSampleRate(
    uint32_t* sample_rate) const {
  // We use the default capturer as the recording sample rate.
  scoped_refptr<WebRtcAudioCapturer> capturer(GetDefaultCapturer());
  if (!capturer.get())
    return -1;

  *sample_rate = static_cast<uint32_t>(
      capturer->source_audio_parameters().sample_rate());
  return 0;
}

int32_t WebRtcAudioDeviceImpl::PlayoutSampleRate(
    uint32_t* sample_rate) const {
  *sample_rate = renderer_ ? renderer_->sample_rate() : 0;
  return 0;
}

bool WebRtcAudioDeviceImpl::SetAudioRenderer(WebRtcAudioRenderer* renderer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(renderer);

  base::AutoLock auto_lock(lock_);
  if (renderer_.get())
    return false;

  if (!renderer->Initialize(this))
    return false;

  renderer_ = renderer;
  return true;
}

void WebRtcAudioDeviceImpl::AddAudioCapturer(
    const scoped_refptr<WebRtcAudioCapturer>& capturer) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::AddAudioCapturer()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(capturer.get());
  DCHECK(!capturer->device_id().empty());
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(std::find(capturers_.begin(), capturers_.end(), capturer) ==
        capturers_.end());
    capturers_.push_back(capturer);
  }
}

void WebRtcAudioDeviceImpl::RemoveAudioCapturer(
    const scoped_refptr<WebRtcAudioCapturer>& capturer) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::AddAudioCapturer()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(capturer.get());
  base::AutoLock auto_lock(lock_);
  capturers_.remove(capturer);
}

scoped_refptr<WebRtcAudioCapturer>
WebRtcAudioDeviceImpl::GetDefaultCapturer() const {
  base::AutoLock auto_lock(lock_);
  // Use the last |capturer| which is from the latest getUserMedia call as
  // the default capture device.
  return capturers_.empty() ? NULL : capturers_.back();
}

void WebRtcAudioDeviceImpl::AddPlayoutSink(
    WebRtcPlayoutDataSource::Sink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(sink);
  base::AutoLock auto_lock(lock_);
  DCHECK(std::find(playout_sinks_.begin(), playout_sinks_.end(), sink) ==
      playout_sinks_.end());
  playout_sinks_.push_back(sink);
}

void WebRtcAudioDeviceImpl::RemovePlayoutSink(
    WebRtcPlayoutDataSource::Sink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(sink);
  base::AutoLock auto_lock(lock_);
  playout_sinks_.remove(sink);
}

bool WebRtcAudioDeviceImpl::GetAuthorizedDeviceInfoForAudioRenderer(
    int* session_id,
    int* output_sample_rate,
    int* output_frames_per_buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If there is no capturer or there are more than one open capture devices,
  // return false.
  if (capturers_.empty() || capturers_.size() > 1)
    return false;

  return GetDefaultCapturer()->GetPairedOutputParameters(
      session_id, output_sample_rate, output_frames_per_buffer);
}

}  // namespace content
