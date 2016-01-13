// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_

#include <list>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/tagged_list.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"

namespace content {

class MediaStreamAudioLevelCalculator;
class MediaStreamAudioProcessor;
class MediaStreamAudioSink;
class MediaStreamAudioSinkOwner;
class MediaStreamAudioTrackSink;
class PeerConnectionAudioSink;
class WebAudioCapturerSource;
class WebRtcAudioCapturer;
class WebRtcLocalAudioTrackAdapter;

// A WebRtcLocalAudioTrack instance contains the implementations of
// MediaStreamTrackExtraData.
// When an instance is created, it will register itself as a track to the
// WebRtcAudioCapturer to get the captured data, and forward the data to
// its |sinks_|. The data flow can be stopped by disabling the audio track.
class CONTENT_EXPORT WebRtcLocalAudioTrack
    : NON_EXPORTED_BASE(public MediaStreamTrack) {
 public:
  WebRtcLocalAudioTrack(WebRtcLocalAudioTrackAdapter* adapter,
                        const scoped_refptr<WebRtcAudioCapturer>& capturer,
                        WebAudioCapturerSource* webaudio_source);

  virtual ~WebRtcLocalAudioTrack();

  // Add a sink to the track. This function will trigger a OnSetFormat()
  // call on the |sink|.
  // Called on the main render thread.
  void AddSink(MediaStreamAudioSink* sink);

  // Remove a sink from the track.
  // Called on the main render thread.
  void RemoveSink(MediaStreamAudioSink* sink);

  // Add/remove PeerConnection sink to/from the track.
  // TODO(xians): Remove these two methods after PeerConnection can use the
  // same sink interface as MediaStreamAudioSink.
  void AddSink(PeerConnectionAudioSink* sink);
  void RemoveSink(PeerConnectionAudioSink* sink);

  // Starts the local audio track. Called on the main render thread and
  // should be called only once when audio track is created.
  void Start();

  // Stops the local audio track. Called on the main render thread and
  // should be called only once when audio track going away.
  virtual void Stop() OVERRIDE;

  // Method called by the capturer to deliver the capture data.
  // Called on the capture audio thread.
  void Capture(const int16* audio_data,
               base::TimeDelta delay,
               int volume,
               bool key_pressed,
               bool need_audio_processing);

  // Method called by the capturer to set the audio parameters used by source
  // of the capture data..
  // Called on the capture audio thread.
  void OnSetFormat(const media::AudioParameters& params);

  // Method called by the capturer to set the processor that applies signal
  // processing on the data of the track.
  // Called on the capture audio thread.
  void SetAudioProcessor(
      const scoped_refptr<MediaStreamAudioProcessor>& processor);

 private:
  typedef TaggedList<MediaStreamAudioTrackSink> SinkList;

  // All usage of libjingle is through this adapter. The adapter holds
  // a reference on this object, but not vice versa.
  WebRtcLocalAudioTrackAdapter* adapter_;

  // The provider of captured data to render.
  scoped_refptr<WebRtcAudioCapturer> capturer_;

  // The source of the audio track which is used by WebAudio, which provides
  // data to the audio track when hooking up with WebAudio.
  scoped_refptr<WebAudioCapturerSource> webaudio_source_;

  // A tagged list of sinks that the audio data is fed to. Tags
  // indicate tracks that need to be notified that the audio format
  // has changed.
  SinkList sinks_;

  // Used to DCHECK that some methods are called on the main render thread.
  base::ThreadChecker main_render_thread_checker_;

  // Used to DCHECK that some methods are called on the capture audio thread.
  base::ThreadChecker capture_thread_checker_;

  // Protects |params_| and |sinks_|.
  mutable base::Lock lock_;

  // Audio parameters of the audio capture stream.
  // Accessed on only the audio capture thread.
  media::AudioParameters audio_parameters_;

  // Used to calculate the signal level that shows in the UI.
  // Accessed on only the audio thread.
  scoped_ptr<MediaStreamAudioLevelCalculator> level_calculator_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLocalAudioTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_TRACK_H_
