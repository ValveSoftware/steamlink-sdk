// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBAUDIO_CAPTURER_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_WEBAUDIO_CAPTURER_SOURCE_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_fifo.h"
#include "third_party/WebKit/public/platform/WebAudioDestinationConsumer.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

class WebRtcAudioCapturer;
class WebRtcLocalAudioTrack;

// WebAudioCapturerSource is the missing link between
// WebAudio's MediaStreamAudioDestinationNode and WebRtcLocalAudioTrack.
//
// 1. WebKit calls the setFormat() method setting up the basic stream format
//    (channels, and sample-rate).
// 2. consumeAudio() is called periodically by WebKit which dispatches the
//    audio stream to the WebRtcLocalAudioTrack::Capture() method.
class WebAudioCapturerSource
    :  public base::RefCountedThreadSafe<WebAudioCapturerSource>,
       public blink::WebAudioDestinationConsumer {
 public:
  WebAudioCapturerSource();

  // WebAudioDestinationConsumer implementation.
  // setFormat() is called early on, so that we can configure the audio track.
  virtual void setFormat(size_t number_of_channels, float sample_rate) OVERRIDE;
  // MediaStreamAudioDestinationNode periodically calls consumeAudio().
  // Called on the WebAudio audio thread.
  virtual void consumeAudio(const blink::WebVector<const float*>& audio_data,
      size_t number_of_frames) OVERRIDE;

  // Called when the WebAudioCapturerSource is hooking to a media audio track.
  // |track| is the sink of the data flow. |source_provider| is the source of
  // the data flow where stream information like delay, volume, key_pressed,
  // is stored.
  void Start(WebRtcLocalAudioTrack* track, WebRtcAudioCapturer* capturer);

  // Called when the media audio track is stopping.
  void Stop();

 protected:
  friend class base::RefCountedThreadSafe<WebAudioCapturerSource>;
  virtual ~WebAudioCapturerSource();

 private:
  // Used to DCHECK that some methods are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // The audio track this WebAudioCapturerSource is feeding data to.
  // WebRtcLocalAudioTrack is reference counted, and owning this object.
  // To avoid circular reference, a raw pointer is kept here.
  WebRtcLocalAudioTrack* track_;

  // A raw pointer to the capturer to get audio processing params like
  // delay, volume, key_pressed information.
  // This |capturer_| is guaranteed to outlive this object.
  WebRtcAudioCapturer* capturer_;

  media::AudioParameters params_;

  // Flag to help notify the |track_| when the audio format has changed.
  bool audio_format_changed_;

  // Wraps data coming from HandleCapture().
  scoped_ptr<media::AudioBus> wrapper_bus_;

  // Bus for reading from FIFO and calling the CaptureCallback.
  scoped_ptr<media::AudioBus> capture_bus_;

  // Handles mismatch between WebAudio buffer size and WebRTC.
  scoped_ptr<media::AudioFifo> fifo_;

  // Buffer to pass audio data to WebRtc.
  scoped_ptr<int16[]> audio_data_;

  // Synchronizes HandleCapture() with AudioCapturerSource calls.
  base::Lock lock_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(WebAudioCapturerSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBAUDIO_CAPTURER_SOURCE_H_
