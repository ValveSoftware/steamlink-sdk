// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_TRACK_RECORDER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_TRACK_RECORDER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace media {
class AudioBus;
class AudioParameters;
}  // namespace media

namespace content {

// AudioTrackRecorder is a MediaStreamAudioSink that encodes the audio buses
// received from a Stream Audio Track. The class is constructed on a
// single thread (the main Render thread) but can recieve MediaStreamAudioSink-
// related calls on a different "live audio" thread (referred to internally as
// the "capture thread"). It owns an internal thread to use for encoding, on
// which lives an AudioEncoder (a private nested class of ATR) with its own
// threading subtleties, see the implementation file.
class CONTENT_EXPORT AudioTrackRecorder
    : NON_EXPORTED_BASE(public MediaStreamAudioSink) {
 public:
  using OnEncodedAudioCB =
      base::Callback<void(const media::AudioParameters& params,
                          std::unique_ptr<std::string> encoded_data,
                          base::TimeTicks capture_time)>;

  AudioTrackRecorder(const blink::WebMediaStreamTrack& track,
                     const OnEncodedAudioCB& on_encoded_audio_cb,
                     int32_t bits_per_second);
  ~AudioTrackRecorder() override;

  // Implement MediaStreamAudioSink.
  void OnSetFormat(const media::AudioParameters& params) override;
  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks capture_time) override;

  void Pause();
  void Resume();

 private:
  // Forward declaration of nested class for handling encoding.
  // See the implementation file for details.
  class AudioEncoder;

  // Used to check that we are destroyed on the same thread we were created on.
  base::ThreadChecker main_render_thread_checker_;

  // Used to check that MediaStreamAudioSink's methods are called on the
  // capture audio thread.
  base::ThreadChecker capture_thread_checker_;

  // We need to hold on to the Blink track to remove ourselves on destruction.
  const blink::WebMediaStreamTrack track_;

  // Thin wrapper around OpusEncoder.
  // |encoder_| should be initialized before |encoder_thread_| such that
  // |encoder_thread_| is destructed first. This, combined with all
  // AudioEncoder work (aside from construction and destruction) happening on
  // |encoder_thread_|, should allow us to be sure that all AudioEncoder work is
  // done by the time we destroy it on ATR's thread.
  const scoped_refptr<AudioEncoder> encoder_;
  // The thread on which |encoder_| works.
  base::Thread encoder_thread_;

  DISALLOW_COPY_AND_ASSIGN(AudioTrackRecorder);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_TRACK_RECORDER_H_
