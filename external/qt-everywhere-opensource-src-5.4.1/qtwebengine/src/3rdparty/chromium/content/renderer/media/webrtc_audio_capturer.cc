// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_capturer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/child/child_process.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "content/renderer/media/webrtc_logging.h"
#include "media/audio/sample_rates.h"

namespace content {

namespace {

// Supported hardware sample rates for input and output sides.
#if defined(OS_WIN) || defined(OS_MACOSX)
// media::GetAudioInputHardwareSampleRate() asks the audio layer
// for its current sample rate (set by the user) on Windows and Mac OS X.
// The listed rates below adds restrictions and WebRtcAudioDeviceImpl::Init()
// will fail if the user selects any rate outside these ranges.
const int kValidInputRates[] =
    {192000, 96000, 48000, 44100, 32000, 16000, 8000};
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
const int kValidInputRates[] = {48000, 44100};
#elif defined(OS_ANDROID)
const int kValidInputRates[] = {48000, 44100};
#else
const int kValidInputRates[] = {44100};
#endif

// Time constant for AudioPowerMonitor.  See AudioPowerMonitor ctor comments
// for semantics.  This value was arbitrarily chosen, but seems to work well.
const int kPowerMonitorTimeConstantMs = 10;

// The time between two audio power level samples.
const int kPowerMonitorLogIntervalSeconds = 10;

}  // namespace

// Reference counted container of WebRtcLocalAudioTrack delegate.
// TODO(xians): Switch to MediaStreamAudioSinkOwner.
class WebRtcAudioCapturer::TrackOwner
    : public base::RefCountedThreadSafe<WebRtcAudioCapturer::TrackOwner> {
 public:
  explicit TrackOwner(WebRtcLocalAudioTrack* track)
      : delegate_(track) {}

  void Capture(const int16* audio_data,
               base::TimeDelta delay,
               double volume,
               bool key_pressed,
               bool need_audio_processing) {
    base::AutoLock lock(lock_);
    if (delegate_) {
      delegate_->Capture(audio_data,
                         delay,
                         volume,
                         key_pressed,
                         need_audio_processing);
    }
  }

  void OnSetFormat(const media::AudioParameters& params) {
    base::AutoLock lock(lock_);
    if (delegate_)
      delegate_->OnSetFormat(params);
  }

  void SetAudioProcessor(
      const scoped_refptr<MediaStreamAudioProcessor>& processor) {
    base::AutoLock lock(lock_);
    if (delegate_)
      delegate_->SetAudioProcessor(processor);
  }

  void Reset() {
    base::AutoLock lock(lock_);
    delegate_ = NULL;
  }

  void Stop() {
    base::AutoLock lock(lock_);
    DCHECK(delegate_);

    // This can be reentrant so reset |delegate_| before calling out.
    WebRtcLocalAudioTrack* temp = delegate_;
    delegate_ = NULL;
    temp->Stop();
  }

  // Wrapper which allows to use std::find_if() when adding and removing
  // sinks to/from the list.
  struct TrackWrapper {
    TrackWrapper(WebRtcLocalAudioTrack* track) : track_(track) {}
    bool operator()(
        const scoped_refptr<WebRtcAudioCapturer::TrackOwner>& owner) const {
      return owner->IsEqual(track_);
    }
    WebRtcLocalAudioTrack* track_;
  };

 protected:
  virtual ~TrackOwner() {}

 private:
  friend class base::RefCountedThreadSafe<WebRtcAudioCapturer::TrackOwner>;

  bool IsEqual(const WebRtcLocalAudioTrack* other) const {
    base::AutoLock lock(lock_);
    return (other == delegate_);
  }

  // Do NOT reference count the |delegate_| to avoid cyclic reference counting.
  WebRtcLocalAudioTrack* delegate_;
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(TrackOwner);
};

// static
scoped_refptr<WebRtcAudioCapturer> WebRtcAudioCapturer::CreateCapturer(
    int render_view_id, const StreamDeviceInfo& device_info,
    const blink::WebMediaConstraints& constraints,
    WebRtcAudioDeviceImpl* audio_device,
    MediaStreamAudioSource* audio_source) {
  scoped_refptr<WebRtcAudioCapturer> capturer = new WebRtcAudioCapturer(
      render_view_id, device_info, constraints, audio_device, audio_source);
  if (capturer->Initialize())
    return capturer;

  return NULL;
}

bool WebRtcAudioCapturer::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::Initialize()";
  WebRtcLogMessage(base::StringPrintf(
      "WAC::Initialize. render_view_id=%d"
      ", channel_layout=%d, sample_rate=%d, buffer_size=%d"
      ", session_id=%d, paired_output_sample_rate=%d"
      ", paired_output_frames_per_buffer=%d, effects=%d. ",
      render_view_id_,
      device_info_.device.input.channel_layout,
      device_info_.device.input.sample_rate,
      device_info_.device.input.frames_per_buffer,
      device_info_.session_id,
      device_info_.device.matched_output.sample_rate,
      device_info_.device.matched_output.frames_per_buffer,
      device_info_.device.input.effects));

  if (render_view_id_ == -1) {
    // Return true here to allow injecting a new source via
    // SetCapturerSourceForTesting() at a later state.
    return true;
  }

  MediaAudioConstraints audio_constraints(constraints_,
                                          device_info_.device.input.effects);
  if (!audio_constraints.IsValid())
    return false;

  media::ChannelLayout channel_layout = static_cast<media::ChannelLayout>(
      device_info_.device.input.channel_layout);
  DVLOG(1) << "Audio input hardware channel layout: " << channel_layout;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout",
                            channel_layout, media::CHANNEL_LAYOUT_MAX + 1);

  // Verify that the reported input channel configuration is supported.
  if (channel_layout != media::CHANNEL_LAYOUT_MONO &&
      channel_layout != media::CHANNEL_LAYOUT_STEREO &&
      channel_layout != media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC) {
    DLOG(ERROR) << channel_layout
                << " is not a supported input channel configuration.";
    return false;
  }

  DVLOG(1) << "Audio input hardware sample rate: "
           << device_info_.device.input.sample_rate;
  media::AudioSampleRate asr;
  if (media::ToAudioSampleRate(device_info_.device.input.sample_rate, &asr)) {
    UMA_HISTOGRAM_ENUMERATION(
        "WebRTC.AudioInputSampleRate", asr, media::kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS("WebRTC.AudioInputSampleRateUnexpected",
                         device_info_.device.input.sample_rate);
  }

  // Verify that the reported input hardware sample rate is supported
  // on the current platform.
  if (std::find(&kValidInputRates[0],
                &kValidInputRates[0] + arraysize(kValidInputRates),
                device_info_.device.input.sample_rate) ==
          &kValidInputRates[arraysize(kValidInputRates)]) {
    DLOG(ERROR) << device_info_.device.input.sample_rate
                << " is not a supported input rate.";
    return false;
  }

  // Create and configure the default audio capturing source.
  SetCapturerSource(AudioDeviceFactory::NewInputDevice(render_view_id_),
                    channel_layout,
                    static_cast<float>(device_info_.device.input.sample_rate));

  // Add the capturer to the WebRtcAudioDeviceImpl since it needs some hardware
  // information from the capturer.
  if (audio_device_)
    audio_device_->AddAudioCapturer(this);

  return true;
}

WebRtcAudioCapturer::WebRtcAudioCapturer(
    int render_view_id,
    const StreamDeviceInfo& device_info,
    const blink::WebMediaConstraints& constraints,
    WebRtcAudioDeviceImpl* audio_device,
    MediaStreamAudioSource* audio_source)
    : constraints_(constraints),
      audio_processor_(
          new talk_base::RefCountedObject<MediaStreamAudioProcessor>(
              constraints, device_info.device.input.effects, audio_device)),
      running_(false),
      render_view_id_(render_view_id),
      device_info_(device_info),
      volume_(0),
      peer_connection_mode_(false),
      key_pressed_(false),
      need_audio_processing_(false),
      audio_device_(audio_device),
      audio_source_(audio_source),
      audio_power_monitor_(
          device_info_.device.input.sample_rate,
          base::TimeDelta::FromMilliseconds(kPowerMonitorTimeConstantMs)) {
  DVLOG(1) << "WebRtcAudioCapturer::WebRtcAudioCapturer()";
}

WebRtcAudioCapturer::~WebRtcAudioCapturer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(tracks_.IsEmpty());
  DVLOG(1) << "WebRtcAudioCapturer::~WebRtcAudioCapturer()";
  Stop();
}

void WebRtcAudioCapturer::AddTrack(WebRtcLocalAudioTrack* track) {
  DCHECK(track);
  DVLOG(1) << "WebRtcAudioCapturer::AddTrack()";

  {
    base::AutoLock auto_lock(lock_);
    // Verify that |track| is not already added to the list.
    DCHECK(!tracks_.Contains(TrackOwner::TrackWrapper(track)));

    // Add with a tag, so we remember to call OnSetFormat() on the new
    // track.
    scoped_refptr<TrackOwner> track_owner(new TrackOwner(track));
    tracks_.AddAndTag(track_owner);
  }
}

void WebRtcAudioCapturer::RemoveTrack(WebRtcLocalAudioTrack* track) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::RemoveTrack()";
  bool stop_source = false;
  {
    base::AutoLock auto_lock(lock_);

    scoped_refptr<TrackOwner> removed_item =
        tracks_.Remove(TrackOwner::TrackWrapper(track));

    // Clear the delegate to ensure that no more capture callbacks will
    // be sent to this sink. Also avoids a possible crash which can happen
    // if this method is called while capturing is active.
    if (removed_item.get()) {
      removed_item->Reset();
      stop_source = tracks_.IsEmpty();
    }
  }
  if (stop_source) {
    // Since WebRtcAudioCapturer does not inherit MediaStreamAudioSource,
    // and instead MediaStreamAudioSource is composed of a WebRtcAudioCapturer,
    // we have to call StopSource on the MediaStreamSource. This will call
    // MediaStreamAudioSource::DoStopSource which in turn call
    // WebRtcAudioCapturerer::Stop();
    audio_source_->StopSource();
  }
}

void WebRtcAudioCapturer::SetCapturerSource(
    const scoped_refptr<media::AudioCapturerSource>& source,
    media::ChannelLayout channel_layout,
    float sample_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "SetCapturerSource(channel_layout=" << channel_layout << ","
           << "sample_rate=" << sample_rate << ")";
  scoped_refptr<media::AudioCapturerSource> old_source;
  {
    base::AutoLock auto_lock(lock_);
    if (source_.get() == source.get())
      return;

    source_.swap(old_source);
    source_ = source;

    // Reset the flag to allow starting the new source.
    running_ = false;
  }

  DVLOG(1) << "Switching to a new capture source.";
  if (old_source.get())
    old_source->Stop();

  // Dispatch the new parameters both to the sink(s) and to the new source,
  // also apply the new |constraints|.
  // The idea is to get rid of any dependency of the microphone parameters
  // which would normally be used by default.
  // bits_per_sample is always 16 for now.
  int buffer_size = GetBufferSize(sample_rate);
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                channel_layout, 0, sample_rate,
                                16, buffer_size,
                                device_info_.device.input.effects);

  {
    base::AutoLock auto_lock(lock_);
    // Notify the |audio_processor_| of the new format.
    audio_processor_->OnCaptureFormatChanged(params);

    MediaAudioConstraints audio_constraints(constraints_,
                                            device_info_.device.input.effects);
    need_audio_processing_ = audio_constraints.NeedsAudioProcessing();
    // Notify all tracks about the new format.
    tracks_.TagAll();
  }

  if (source.get())
    source->Initialize(params, this, session_id());

  Start();
}

void WebRtcAudioCapturer::EnablePeerConnectionMode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "EnablePeerConnectionMode";
  // Do nothing if the peer connection mode has been enabled.
  if (peer_connection_mode_)
    return;

  peer_connection_mode_ = true;
  int render_view_id = -1;
  media::AudioParameters input_params;
  {
    base::AutoLock auto_lock(lock_);
    // Simply return if there is no existing source or the |render_view_id_| is
    // not valid.
    if (!source_.get() || render_view_id_== -1)
      return;

    render_view_id = render_view_id_;
    input_params = audio_processor_->InputFormat();
  }

  // Do nothing if the current buffer size is the WebRtc native buffer size.
  if (GetBufferSize(input_params.sample_rate()) ==
          input_params.frames_per_buffer()) {
    return;
  }

  // Create a new audio stream as source which will open the hardware using
  // WebRtc native buffer size.
  SetCapturerSource(AudioDeviceFactory::NewInputDevice(render_view_id),
                    input_params.channel_layout(),
                    static_cast<float>(input_params.sample_rate()));
}

void WebRtcAudioCapturer::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::Start()";
  base::AutoLock auto_lock(lock_);
  if (running_ || !source_)
    return;

  // Start the data source, i.e., start capturing data from the current source.
  // We need to set the AGC control before starting the stream.
  source_->SetAutomaticGainControl(true);
  source_->Start();
  running_ = true;
}

void WebRtcAudioCapturer::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::Stop()";
  scoped_refptr<media::AudioCapturerSource> source;
  TrackList::ItemList tracks;
  {
    base::AutoLock auto_lock(lock_);
    if (!running_)
      return;

    source = source_;
    tracks = tracks_.Items();
    tracks_.Clear();
    running_ = false;
  }

  // Remove the capturer object from the WebRtcAudioDeviceImpl.
  if (audio_device_)
    audio_device_->RemoveAudioCapturer(this);

  for (TrackList::ItemList::const_iterator it = tracks.begin();
       it != tracks.end();
       ++it) {
    (*it)->Stop();
  }

  if (source.get())
    source->Stop();

  // Stop the audio processor to avoid feeding render data into the processor.
  audio_processor_->Stop();
}

void WebRtcAudioCapturer::SetVolume(int volume) {
  DVLOG(1) << "WebRtcAudioCapturer::SetVolume()";
  DCHECK_LE(volume, MaxVolume());
  double normalized_volume = static_cast<double>(volume) / MaxVolume();
  base::AutoLock auto_lock(lock_);
  if (source_.get())
    source_->SetVolume(normalized_volume);
}

int WebRtcAudioCapturer::Volume() const {
  base::AutoLock auto_lock(lock_);
  return volume_;
}

int WebRtcAudioCapturer::MaxVolume() const {
  return WebRtcAudioDeviceImpl::kMaxVolumeLevel;
}

void WebRtcAudioCapturer::Capture(const media::AudioBus* audio_source,
                                  int audio_delay_milliseconds,
                                  double volume,
                                  bool key_pressed) {
// This callback is driven by AudioInputDevice::AudioThreadCallback if
// |source_| is AudioInputDevice, otherwise it is driven by client's
// CaptureCallback.
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

  TrackList::ItemList tracks;
  TrackList::ItemList tracks_to_notify_format;
  int current_volume = 0;
  base::TimeDelta audio_delay;
  bool need_audio_processing = true;
  {
    base::AutoLock auto_lock(lock_);
    if (!running_)
      return;

    // Map internal volume range of [0.0, 1.0] into [0, 255] used by AGC.
    // The volume can be higher than 255 on Linux, and it will be cropped to
    // 255 since AGC does not allow values out of range.
    volume_ = static_cast<int>((volume * MaxVolume()) + 0.5);
    current_volume = volume_ > MaxVolume() ? MaxVolume() : volume_;
    audio_delay = base::TimeDelta::FromMilliseconds(audio_delay_milliseconds);
    audio_delay_ = audio_delay;
    key_pressed_ = key_pressed;
    tracks = tracks_.Items();
    tracks_.RetrieveAndClearTags(&tracks_to_notify_format);

    // Set the flag to turn on the audio processing in PeerConnection level.
    // Note that, we turn off the audio processing in PeerConnection if the
    // processor has already processed the data.
    need_audio_processing = need_audio_processing_ ?
        !MediaStreamAudioProcessor::IsAudioTrackProcessingEnabled() : false;
  }

  DCHECK(audio_processor_->InputFormat().IsValid());
  DCHECK_EQ(audio_source->channels(),
            audio_processor_->InputFormat().channels());
  DCHECK_EQ(audio_source->frames(),
            audio_processor_->InputFormat().frames_per_buffer());

  // Notify the tracks on when the format changes. This will do nothing if
  // |tracks_to_notify_format| is empty.
  media::AudioParameters output_params = audio_processor_->OutputFormat();
  for (TrackList::ItemList::const_iterator it = tracks_to_notify_format.begin();
       it != tracks_to_notify_format.end(); ++it) {
    (*it)->OnSetFormat(output_params);
    (*it)->SetAudioProcessor(audio_processor_);
  }

  if ((base::TimeTicks::Now() - last_audio_level_log_time_).InSeconds() >
          kPowerMonitorLogIntervalSeconds) {
    audio_power_monitor_.Scan(*audio_source, audio_source->frames());

    last_audio_level_log_time_ = base::TimeTicks::Now();

    std::pair<float, bool> result =
        audio_power_monitor_.ReadCurrentPowerAndClip();
    WebRtcLogMessage(base::StringPrintf(
        "WAC::Capture: current_audio_power=%.2fdBFS.", result.first));

    audio_power_monitor_.Reset();
  }

  // Push the data to the processor for processing.
  audio_processor_->PushCaptureData(audio_source);

  // Process and consume the data in the processor until there is not enough
  // data in the processor.
  int16* output = NULL;
  int new_volume = 0;
  while (audio_processor_->ProcessAndConsumeData(
      audio_delay, current_volume, key_pressed, &new_volume, &output)) {
    // Feed the post-processed data to the tracks.
    for (TrackList::ItemList::const_iterator it = tracks.begin();
         it != tracks.end(); ++it) {
      (*it)->Capture(output, audio_delay, current_volume, key_pressed,
                     need_audio_processing);
    }

    if (new_volume) {
      SetVolume(new_volume);

      // Update the |current_volume| to avoid passing the old volume to AGC.
      current_volume = new_volume;
    }
  }
}

void WebRtcAudioCapturer::OnCaptureError() {
  NOTIMPLEMENTED();
}

media::AudioParameters WebRtcAudioCapturer::source_audio_parameters() const {
  base::AutoLock auto_lock(lock_);
  return audio_processor_ ?
      audio_processor_->InputFormat() : media::AudioParameters();
}

bool WebRtcAudioCapturer::GetPairedOutputParameters(
    int* session_id,
    int* output_sample_rate,
    int* output_frames_per_buffer) const {
  // Don't set output parameters unless all of them are valid.
  if (device_info_.session_id <= 0 ||
      !device_info_.device.matched_output.sample_rate ||
      !device_info_.device.matched_output.frames_per_buffer)
    return false;

  *session_id = device_info_.session_id;
  *output_sample_rate = device_info_.device.matched_output.sample_rate;
  *output_frames_per_buffer =
      device_info_.device.matched_output.frames_per_buffer;

  return true;
}

int WebRtcAudioCapturer::GetBufferSize(int sample_rate) const {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(OS_ANDROID)
  // TODO(henrika): Tune and adjust buffer size on Android.
  return (2 * sample_rate / 100);
#endif

  // PeerConnection is running at a buffer size of 10ms data. A multiple of
  // 10ms as the buffer size can give the best performance to PeerConnection.
  int peer_connection_buffer_size = sample_rate / 100;

  // Use the native hardware buffer size in non peer connection mode when the
  // platform is using a native buffer size smaller than the PeerConnection
  // buffer size.
  int hardware_buffer_size = device_info_.device.input.frames_per_buffer;
  if (!peer_connection_mode_ && hardware_buffer_size &&
      hardware_buffer_size <= peer_connection_buffer_size) {
    return hardware_buffer_size;
  }

  return (sample_rate / 100);
}

void WebRtcAudioCapturer::GetAudioProcessingParams(
    base::TimeDelta* delay, int* volume, bool* key_pressed) {
  base::AutoLock auto_lock(lock_);
  *delay = audio_delay_;
  *volume = volume_;
  *key_pressed = key_pressed_;
}

void WebRtcAudioCapturer::SetCapturerSourceForTesting(
    const scoped_refptr<media::AudioCapturerSource>& source,
    media::AudioParameters params) {
  // Create a new audio stream as source which uses the new source.
  SetCapturerSource(source, params.channel_layout(),
                    static_cast<float>(params.sample_rate()));
}

}  // namespace content
