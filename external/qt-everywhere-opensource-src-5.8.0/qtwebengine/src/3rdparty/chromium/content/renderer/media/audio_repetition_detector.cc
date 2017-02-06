// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_repetition_detector.h"

#include <string.h>

#include "base/logging.h"
#include "base/macros.h"

namespace {

const float EPSILON = 4.0f / 32768.0f;

}  // namespace

namespace content {

AudioRepetitionDetector::AudioRepetitionDetector(
    int min_length_ms, size_t max_frames,
    const std::vector<int>& look_back_times,
    const RepetitionCallback& repetition_callback)
    : max_look_back_ms_(0),
      min_length_ms_(min_length_ms),
      num_channels_(0),
      sample_rate_(0),
      buffer_size_frames_(0),
      buffer_end_index_(0),
      max_frames_(max_frames),
      repetition_callback_(repetition_callback) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  processing_thread_checker_.DetachFromThread();

  // Avoid duplications in |look_back_times| if any.
  std::vector<int> temp(look_back_times);
  std::sort(temp.begin(), temp.end());
  temp.erase(std::unique(temp.begin(), temp.end()), temp.end());

  max_look_back_ms_ = temp.back();
  for (int look_back : temp)
    states_.push_back(new State(look_back));
}

AudioRepetitionDetector::~AudioRepetitionDetector() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
}

void AudioRepetitionDetector::Detect(const float* data, size_t num_frames,
                                     size_t num_channels, int sample_rate) {
  DCHECK(processing_thread_checker_.CalledOnValidThread());
  DCHECK(!states_.empty());

  if (num_channels != num_channels_ || sample_rate != sample_rate_)
    Reset(num_channels, sample_rate);

  // The maximum number of frames |audio_buffer_| can take in is |max_frames_|.
  // Therefore, input data with larger frames needs be divided into chunks.
  const size_t chunk_size = max_frames_ * num_channels;
  while (num_frames > max_frames_) {
    Detect(data, max_frames_, num_channels, sample_rate);
    data += chunk_size;
    num_frames -= max_frames_;
  }

  if (num_frames == 0)
    return;

  AddFramesToBuffer(data, num_frames);

  for (size_t idx = num_frames; idx > 0; --idx, data += num_channels) {
    for (State* state : states_) {
      // Look back position depends on the sample rate. It is rounded down to
      // the closest integer.
      const size_t look_back_frames =
          state->look_back_ms() * sample_rate_ / 1000;
      // Equal(data, offset) checks if |data| equals the audio frame located
      // |offset| frames from the end of buffer. Now a full frame has been
      // inserted to the buffer, and thus |offset| should compensate for it.
      if (Equal(data, look_back_frames + idx)) {
        if (!state->reported()) {
          state->Increment(data, num_channels);
          if (HasValidReport(state)) {
            repetition_callback_.Run(state->look_back_ms());
            state->set_reported(true);
          }
        }
      } else {
        state->Reset();
      }
    }
  }
}

AudioRepetitionDetector::State::State(int look_back_ms)
    : look_back_ms_(look_back_ms) {
  Reset();
}

AudioRepetitionDetector::State::~State() = default;

void AudioRepetitionDetector::State::Increment(const float* frame,
                                               size_t num_channels) {
  if (count_frames_ == 0) {
    is_constant_ = true;
    constant_.resize(num_channels);
    memcpy(&constant_[0], frame, sizeof(float) * num_channels);
  } else if (is_constant_ && !EqualsConstant(frame, num_channels)) {
    is_constant_ = false;
  }
  ++count_frames_;
}

void AudioRepetitionDetector::State::Reset() {
  count_frames_ = 0;
  reported_ = false;
}

bool AudioRepetitionDetector::State::EqualsConstant(const float* frame,
                                                    size_t num_channels) const {
  DCHECK(is_constant_);
  for (size_t channel = 0; channel < num_channels; ++channel) {
    const float diff = frame[channel] - constant_[channel];
    if (diff < -EPSILON || diff > EPSILON)
      return false;
  }
  return true;
}

void AudioRepetitionDetector::Reset(size_t num_channels, int sample_rate) {
  DCHECK(processing_thread_checker_.CalledOnValidThread());
  num_channels_ = num_channels;
  sample_rate_ = sample_rate;

  // |(xxx + 999) / 1000| is an arithmetic way to round up |xxx / 1000|.
  buffer_size_frames_ =
      (max_look_back_ms_ * sample_rate_ + 999) / 1000 + max_frames_;

  audio_buffer_.resize(buffer_size_frames_ * num_channels_);
  for (State* state : states_)
    state->Reset();
}

void AudioRepetitionDetector::AddFramesToBuffer(const float* data,
                                                size_t num_frames) {
  DCHECK(processing_thread_checker_.CalledOnValidThread());
  DCHECK_LE(num_frames, buffer_size_frames_);
  const size_t margin = buffer_size_frames_ - buffer_end_index_;
  const auto it = audio_buffer_.begin() + buffer_end_index_ * num_channels_;
  if (num_frames <= margin) {
    std::copy(data, data + num_frames * num_channels_, it);
    buffer_end_index_ += num_frames;
  } else {
    std::copy(data, data + margin * num_channels_, it);
    std::copy(data + margin * num_channels_, data + num_frames * num_channels_,
              audio_buffer_.begin());
    buffer_end_index_ = num_frames - margin;
  }
}

bool AudioRepetitionDetector::Equal(const float* frame,
                                    int look_back_frames) const {
  DCHECK(processing_thread_checker_.CalledOnValidThread());
  const size_t look_back_index =
      (buffer_end_index_ + buffer_size_frames_ - look_back_frames) %
      buffer_size_frames_ ;
  auto it = audio_buffer_.begin() + look_back_index * num_channels_;
  for (size_t channel = 0; channel < num_channels_; ++channel, ++frame, ++it) {
    if (*frame != *it)
      return false;
  }
  return true;
}

bool AudioRepetitionDetector::HasValidReport(const State* state) const {
  return (!state->is_constant() && state->count_frames() >=
      static_cast<size_t>(min_length_ms_ * sample_rate_ / 1000));
}

}  // namespace content
