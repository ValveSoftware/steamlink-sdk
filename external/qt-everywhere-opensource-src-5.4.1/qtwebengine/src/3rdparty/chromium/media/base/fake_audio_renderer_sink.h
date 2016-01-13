// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FAKE_AUDIO_RENDERER_SINK_H_
#define MEDIA_BASE_FAKE_AUDIO_RENDERER_SINK_H_

#include "media/audio/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"

namespace media {

class FakeAudioRendererSink : public AudioRendererSink {
 public:
  enum State {
    kUninitialized,
    kInitialized,
    kStarted,
    kPaused,
    kPlaying,
    kStopped
  };

  FakeAudioRendererSink();

  virtual void Initialize(const AudioParameters& params,
                          RenderCallback* callback) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Pause() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual bool SetVolume(double volume) OVERRIDE;

  // Attempts to call Render() on the callback provided to
  // Initialize() with |dest| and |audio_delay_milliseconds|.
  // Returns true and sets |frames_written| to the return value of the
  // Render() call.
  // Returns false if this object is in a state where calling Render()
  // should not occur. (i.e., in the kPaused or kStopped state.) The
  // value of |frames_written| is undefined if false is returned.
  bool Render(AudioBus* dest, int audio_delay_milliseconds,
              int* frames_written);
  void OnRenderError();

  State state() const { return state_; }

 protected:
  virtual ~FakeAudioRendererSink();

 private:
  void ChangeState(State new_state);

  State state_;
  RenderCallback* callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioRendererSink);
};

}  // namespace media

#endif  // MEDIA_BASE_FAKE_AUDIO_RENDERER_SINK_H_
