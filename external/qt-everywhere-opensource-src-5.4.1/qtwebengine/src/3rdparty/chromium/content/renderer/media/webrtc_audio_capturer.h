// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_

#include <list>
#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/tagged_list.h"
#include "media/audio/audio_input_device.h"
#include "media/audio/audio_power_monitor.h"
#include "media/base/audio_capturer_source.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"

namespace media {
class AudioBus;
}

namespace content {

class MediaStreamAudioProcessor;
class MediaStreamAudioSource;
class WebRtcAudioDeviceImpl;
class WebRtcLocalAudioRenderer;
class WebRtcLocalAudioTrack;

// This class manages the capture data flow by getting data from its
// |source_|, and passing it to its |tracks_|.
// The threading model for this class is rather complex since it will be
// created on the main render thread, captured data is provided on a dedicated
// AudioInputDevice thread, and methods can be called either on the Libjingle
// thread or on the main render thread but also other client threads
// if an alternative AudioCapturerSource has been set.
class CONTENT_EXPORT WebRtcAudioCapturer
    : public base::RefCountedThreadSafe<WebRtcAudioCapturer>,
      NON_EXPORTED_BASE(public media::AudioCapturerSource::CaptureCallback) {
 public:
  // Used to construct the audio capturer. |render_view_id| specifies the
  // render view consuming audio for capture, |render_view_id| as -1 is used
  // by the unittests to skip creating a source via
  // AudioDeviceFactory::NewInputDevice(), and allow injecting their own source
  // via SetCapturerSourceForTesting() at a later state.  |device_info|
  // contains all the device information that the capturer is created for.
  // |constraints| contains the settings for audio processing.
  // TODO(xians): Implement the interface for the audio source and move the
  // |constraints| to ApplyConstraints().
  // Called on the main render thread.
  static scoped_refptr<WebRtcAudioCapturer> CreateCapturer(
      int render_view_id,
      const StreamDeviceInfo& device_info,
      const blink::WebMediaConstraints& constraints,
      WebRtcAudioDeviceImpl* audio_device,
      MediaStreamAudioSource* audio_source);


  // Add a audio track to the sinks of the capturer.
  // WebRtcAudioDeviceImpl calls this method on the main render thread but
  // other clients may call it from other threads. The current implementation
  // does not support multi-thread calling.
  // The first AddTrack will implicitly trigger the Start() of this object.
  void AddTrack(WebRtcLocalAudioTrack* track);

  // Remove a audio track from the sinks of the capturer.
  // If the track has been added to the capturer, it  must call RemoveTrack()
  // before it goes away.
  // Called on the main render thread or libjingle working thread.
  void RemoveTrack(WebRtcLocalAudioTrack* track);

  // Called when a stream is connecting to a peer connection. This will set
  // up the native buffer size for the stream in order to optimize the
  // performance for peer connection.
  void EnablePeerConnectionMode();

  // Volume APIs used by WebRtcAudioDeviceImpl.
  // Called on the AudioInputDevice audio thread.
  void SetVolume(int volume);
  int Volume() const;
  int MaxVolume() const;

  // Audio parameters utilized by the source of the audio capturer.
  // TODO(phoglund): Think over the implications of this accessor and if we can
  // remove it.
  media::AudioParameters source_audio_parameters() const;

  // Gets information about the paired output device. Returns true if such a
  // device exists.
  bool GetPairedOutputParameters(int* session_id,
                                 int* output_sample_rate,
                                 int* output_frames_per_buffer) const;

  const std::string& device_id() const { return device_info_.device.id; }
  int session_id() const { return device_info_.session_id; }

  // Stops recording audio. This method will empty its track lists since
  // stopping the capturer will implicitly invalidate all its tracks.
  // This method is exposed to the public because the MediaStreamAudioSource can
  // call Stop()
  void Stop();

  // Called by the WebAudioCapturerSource to get the audio processing params.
  // This function is triggered by provideInput() on the WebAudio audio thread,
  // TODO(xians): Remove after moving APM from WebRtc to Chrome.
  void GetAudioProcessingParams(base::TimeDelta* delay, int* volume,
                                bool* key_pressed);

  // Used by the unittests to inject their own source to the capturer.
  void SetCapturerSourceForTesting(
      const scoped_refptr<media::AudioCapturerSource>& source,
      media::AudioParameters params);

 protected:
  friend class base::RefCountedThreadSafe<WebRtcAudioCapturer>;
  virtual ~WebRtcAudioCapturer();

 private:
  class TrackOwner;
  typedef TaggedList<TrackOwner> TrackList;

  WebRtcAudioCapturer(int render_view_id,
                      const StreamDeviceInfo& device_info,
                      const blink::WebMediaConstraints& constraints,
                      WebRtcAudioDeviceImpl* audio_device,
                      MediaStreamAudioSource* audio_source);

  // AudioCapturerSource::CaptureCallback implementation.
  // Called on the AudioInputDevice audio thread.
  virtual void Capture(const media::AudioBus* audio_source,
                       int audio_delay_milliseconds,
                       double volume,
                       bool key_pressed) OVERRIDE;
  virtual void OnCaptureError() OVERRIDE;

  // Initializes the default audio capturing source using the provided render
  // view id and device information. Return true if success, otherwise false.
  bool Initialize();

  // SetCapturerSource() is called if the client on the source side desires to
  // provide their own captured audio data. Client is responsible for calling
  // Start() on its own source to have the ball rolling.
  // Called on the main render thread.
  void SetCapturerSource(
      const scoped_refptr<media::AudioCapturerSource>& source,
      media::ChannelLayout channel_layout,
      float sample_rate);

  // Starts recording audio.
  // Triggered by AddSink() on the main render thread or a Libjingle working
  // thread. It should NOT be called under |lock_|.
  void Start();

  // Helper function to get the buffer size based on |peer_connection_mode_|
  // and sample rate;
  int GetBufferSize(int sample_rate) const;

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // Protects |source_|, |audio_tracks_|, |running_|, |loopback_fifo_|,
  // |params_| and |buffering_|.
  mutable base::Lock lock_;

  // A tagged list of audio tracks that the audio data is fed
  // to. Tagged items need to be notified that the audio format has
  // changed.
  TrackList tracks_;

  // The audio data source from the browser process.
  scoped_refptr<media::AudioCapturerSource> source_;

  // Cached audio constraints for the capturer.
  blink::WebMediaConstraints constraints_;

  // Audio processor doing processing like FIFO, AGC, AEC and NS. Its output
  // data is in a unit of 10 ms data chunk.
  scoped_refptr<MediaStreamAudioProcessor> audio_processor_;

  bool running_;

  int render_view_id_;

  // Cached information of the device used by the capturer.
  const StreamDeviceInfo device_info_;

  // Stores latest microphone volume received in a CaptureData() callback.
  // Range is [0, 255].
  int volume_;

  // Flag which affects the buffer size used by the capturer.
  bool peer_connection_mode_;

  // Cache value for the audio processing params.
  base::TimeDelta audio_delay_;
  bool key_pressed_;

  // Flag to help deciding if the data needs audio processing.
  bool need_audio_processing_;

  // Raw pointer to the WebRtcAudioDeviceImpl, which is valid for the lifetime
  // of RenderThread.
  WebRtcAudioDeviceImpl* audio_device_;

  // Raw pointer to the MediaStreamAudioSource object that holds a reference
  // to this WebRtcAudioCapturer.
  // Since |audio_source_| is owned by a blink::WebMediaStreamSource object and
  // blink guarantees that the blink::WebMediaStreamSource outlives any
  // blink::WebMediaStreamTrack connected to the source, |audio_source_| is
  // guaranteed to exist as long as a WebRtcLocalAudioTrack is connected to this
  // WebRtcAudioCapturer.
  MediaStreamAudioSource* const audio_source_;

    // Audio power monitor for logging audio power level.
  media::AudioPowerMonitor audio_power_monitor_;

  // Records when the last time audio power level is logged.
  base::TimeTicks last_audio_level_log_time_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioCapturer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_
