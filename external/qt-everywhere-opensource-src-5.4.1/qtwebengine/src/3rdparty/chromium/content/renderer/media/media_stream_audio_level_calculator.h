// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_LEVEL_CALCULATOR_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_LEVEL_CALCULATOR_H_

#include "base/threading/thread_checker.h"

namespace content {

// This class is used by the WebRtcLocalAudioTrack to calculate the  level of
// the audio signal. And the audio level will be eventually used by the volume
// animation UI.
// The algorithm used by this class is the same as how it is done in
// third_party/webrtc/voice_engine/level_indicator.cc.
class MediaStreamAudioLevelCalculator {
 public:
  MediaStreamAudioLevelCalculator();
  ~MediaStreamAudioLevelCalculator();

  // Calculates the signal level of the audio data.
  // Returns the absolute value of the amplitude of the signal.
  int Calculate(const int16* audio_data, int number_of_channels,
                int number_of_frames);

 private:
  // Used to DCHECK that the constructor and Calculate() are always called on
  // the same audio thread. Note that the destructor will be called on a
  // different thread, which can be either the main render thread or a new
  // audio thread where WebRtcLocalAudioTrack::OnSetFormat() is called.
  base::ThreadChecker thread_checker_;

  int counter_;
  int max_amplitude_;
  int level_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_LEVEL_CALCULATOR_H_
