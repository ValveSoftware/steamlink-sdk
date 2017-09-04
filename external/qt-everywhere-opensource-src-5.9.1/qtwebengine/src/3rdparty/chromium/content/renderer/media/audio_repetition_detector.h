// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_REPETITION_DETECTOR_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_REPETITION_DETECTOR_H_

#include <stddef.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"

namespace content {

// AudioRepetitionDetector detects bit-exact audio repetitions of registered
// patterns. A repetition pattern is defined by a look back time. The detector
// buffers the audio signal and checks equality of each input sample against the
// samples at the look back positions of all registered patterns, and counts the
// duration of any consecutive equality.
// All methods should be called from the same thread. However, we allow the
// construction and destruction be made from a separate thread.

class CONTENT_EXPORT AudioRepetitionDetector {
 public:
  // Callback that defines the action upon a repetition is detected. One int
  // parameter to the callback is the look back time (in milliseconds) of the
  // detected repetition.
  typedef base::Callback<void(int)> RepetitionCallback;

  // |min_length_ms| is the minimum duration (in milliseconds) of repetitions
  // that count.
  // |max_frames| is the maximum number of audio frames that will be provided to
  // |Detect()| each time. Input longer than |max_frames| won't cause any
  // problem, and will only affect computational efficiency.
  // |look_back_times| is a vector of look back times (in milliseconds) for the
  // detector to keep track.
  AudioRepetitionDetector(int min_length_ms, size_t max_frames,
                          const std::vector<int>& look_back_times,
                          const RepetitionCallback& repetition_callback);

  virtual ~AudioRepetitionDetector();

  // Detect repetition in |data|. |sample_rate| is measured in Hz.
  void Detect(const float* data, size_t num_frames, size_t num_channels,
              int sample_rate);

 private:
  friend class AudioRepetitionDetectorForTest;

  // A state is used by the detector to keep track of a consecutive repetition,
  // whether the samples in a repetition are constant, and whether a repetition
  // has been reported.
  class State {
   public:
    explicit State(int look_back_ms);
    ~State();

    int look_back_ms() const { return look_back_ms_; };
    size_t count_frames() const { return count_frames_; }
    bool is_constant() const { return is_constant_; }
    bool reported() const { return reported_; }
    void set_reported(bool reported) { reported_ = reported; }

    // Increase |count_frames_| by 1. The method also determines if the frames
    // in current repetition are constant.
    void Increment(const float* frame, size_t num_channels);

    void Reset();

   private:
    // Determine if an audio frame (samples interleaved if stereo) is identical
    // to |constant_|.
    bool EqualsConstant(const float* frame, size_t num_channels) const;

    // Look back time of the repetition pattern this state keeps track of.
    const int look_back_ms_;

    // Counter of frames in a consecutive repetition.
    size_t count_frames_;

    // |is_constant_| indicates whether frames in a repetition are constant.
    // When |is_constant_| is true, |constant_| stores that constant frame.
    bool is_constant_;
    std::vector<float> constant_;

    // |reported_| tells whether a repetition has been reported. This is to make
    // sure that a repetition with a long duration will be reported as early as
    // being detected but no more than one time.
    bool reported_;

    DISALLOW_COPY_AND_ASSIGN(State);
  };

  // Reset |audio_buffer_| when number of channels or sample rate (Hz) changes.
  void Reset(size_t num_channels, int sample_rate);

  // Add frames (interleaved if stereo) to |audio_buffer_|.
  void AddFramesToBuffer(const float* data, size_t num_frames);

  // Determine if an audio frame (samples interleaved if stereo) is identical to
  // |audio_buffer_| at a look back position.
  bool Equal(const float* frame, int look_back_samples) const;

  // Check whether the state contains a valid repetition report.
  bool HasValidReport(const State* state) const;

  // Used to DCHECK that we are called on the correct thread. Ctor/dtor
  // should be called on one thread. The rest can be called on another.
  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker processing_thread_checker_;

  ScopedVector<State> states_;

  // Ring buffer to store input audio.
  std::vector<float> audio_buffer_;

  // Maximum look back time of all registered repetitions. This defines the size
  // of |audio_buffer_|
  int max_look_back_ms_;

  // The shortest length for repetitions.
  const int min_length_ms_;

  // Number of audio channels in buffer.
  size_t num_channels_;

  // Sample rate in Hz.
  int sample_rate_;

  // Number of frames in |audio_buffer|.
  size_t buffer_size_frames_;

  // The index of the last frame in |audio_buffer|.
  size_t buffer_end_index_;

  // The maximum frames |audio_buffer_| can take in each time.
  const size_t max_frames_;

  // Action when a repetition is found. |look_back_ms| provides the look back
  // time of the detected repetition.
  RepetitionCallback repetition_callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioRepetitionDetector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_REPETITION_DETECTOR_H_
