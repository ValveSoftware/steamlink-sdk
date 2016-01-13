// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/audio_buffer.h"
#include "content/browser/speech/endpointer/endpointer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kFrameRate = 50;  // 20 ms long frames for AMR encoding.
const int kSampleRate = 8000;  // 8 k samples per second for AMR encoding.

// At 8 sample per second a 20 ms frame is 160 samples, which corrsponds
// to the AMR codec.
const int kFrameSize = kSampleRate / kFrameRate;  // 160 samples.
COMPILE_ASSERT(kFrameSize == 160, invalid_frame_size);
}

namespace content {

class FrameProcessor {
 public:
  // Process a single frame of test audio samples.
  virtual EpStatus ProcessFrame(int64 time, int16* samples, int frame_size) = 0;
};

void RunEndpointerEventsTest(FrameProcessor* processor) {
  int16 samples[kFrameSize];

  // We will create a white noise signal of 150 frames. The frames from 50 to
  // 100 will have more power, and the endpointer should fire on those frames.
  const int kNumFrames = 150;

  // Create a random sequence of samples.
  srand(1);
  float gain = 0.0;
  int64 time = 0;
  for (int frame_count = 0; frame_count < kNumFrames; ++frame_count) {
    // The frames from 50 to 100 will have more power, and the endpointer
    // should detect those frames as speech.
    if ((frame_count >= 50) && (frame_count < 100)) {
      gain = 2000.0;
    } else {
      gain = 1.0;
    }
    // Create random samples.
    for (int i = 0; i < kFrameSize; ++i) {
      float randNum = static_cast<float>(rand() - (RAND_MAX / 2)) /
          static_cast<float>(RAND_MAX);
      samples[i] = static_cast<int16>(gain * randNum);
    }

    EpStatus ep_status = processor->ProcessFrame(time, samples, kFrameSize);
    time += static_cast<int64>(kFrameSize * (1e6 / kSampleRate));

    // Log the status.
    if (20 == frame_count)
      EXPECT_EQ(EP_PRE_SPEECH, ep_status);
    if (70 == frame_count)
      EXPECT_EQ(EP_SPEECH_PRESENT, ep_status);
    if (120 == frame_count)
      EXPECT_EQ(EP_PRE_SPEECH, ep_status);
  }
}

// This test instantiates and initializes a stand alone endpointer module.
// The test creates FrameData objects with random noise and send them
// to the endointer module. The energy of the first 50 frames is low,
// followed by 500 high energy frames, and another 50 low energy frames.
// We test that the correct start and end frames were detected.
class EnergyEndpointerFrameProcessor : public FrameProcessor {
 public:
  explicit EnergyEndpointerFrameProcessor(EnergyEndpointer* endpointer)
      : endpointer_(endpointer) {}

  virtual EpStatus ProcessFrame(int64 time,
                                int16* samples,
                                int frame_size) OVERRIDE {
    endpointer_->ProcessAudioFrame(time, samples, kFrameSize, NULL);
    int64 ep_time;
    return endpointer_->Status(&ep_time);
  }

 private:
  EnergyEndpointer* endpointer_;
};

TEST(EndpointerTest, TestEnergyEndpointerEvents) {
  // Initialize endpointer and configure it. We specify the parameters
  // here for a 20ms window, and a 20ms step size, which corrsponds to
  // the narrow band AMR codec.
  EnergyEndpointerParams ep_config;
  ep_config.set_frame_period(1.0f / static_cast<float>(kFrameRate));
  ep_config.set_frame_duration(1.0f / static_cast<float>(kFrameRate));
  ep_config.set_endpoint_margin(0.2f);
  ep_config.set_onset_window(0.15f);
  ep_config.set_speech_on_window(0.4f);
  ep_config.set_offset_window(0.15f);
  ep_config.set_onset_detect_dur(0.09f);
  ep_config.set_onset_confirm_dur(0.075f);
  ep_config.set_on_maintain_dur(0.10f);
  ep_config.set_offset_confirm_dur(0.12f);
  ep_config.set_decision_threshold(100.0f);
  EnergyEndpointer endpointer;
  endpointer.Init(ep_config);

  endpointer.StartSession();

  EnergyEndpointerFrameProcessor frame_processor(&endpointer);
  RunEndpointerEventsTest(&frame_processor);

  endpointer.EndSession();
};

// Test endpointer wrapper class.
class EndpointerFrameProcessor : public FrameProcessor {
 public:
  explicit EndpointerFrameProcessor(Endpointer* endpointer)
      : endpointer_(endpointer) {}

  virtual EpStatus ProcessFrame(int64 time,
                                int16* samples,
                                int frame_size) OVERRIDE {
    scoped_refptr<AudioChunk> frame(
        new AudioChunk(reinterpret_cast<uint8*>(samples), kFrameSize * 2, 2));
    endpointer_->ProcessAudio(*frame.get(), NULL);
    int64 ep_time;
    return endpointer_->Status(&ep_time);
  }

 private:
  Endpointer* endpointer_;
};

TEST(EndpointerTest, TestEmbeddedEndpointerEvents) {
  const int kSampleRate = 8000;  // 8 k samples per second for AMR encoding.

  Endpointer endpointer(kSampleRate);
  const int64 kMillisecondsPerMicrosecond = 1000;
  const int64 short_timeout = 300 * kMillisecondsPerMicrosecond;
  endpointer.set_speech_input_possibly_complete_silence_length(short_timeout);
  const int64 long_timeout = 500 * kMillisecondsPerMicrosecond;
  endpointer.set_speech_input_complete_silence_length(long_timeout);
  endpointer.StartSession();

  EndpointerFrameProcessor frame_processor(&endpointer);
  RunEndpointerEventsTest(&frame_processor);

  endpointer.EndSession();
}

}  // namespace content
