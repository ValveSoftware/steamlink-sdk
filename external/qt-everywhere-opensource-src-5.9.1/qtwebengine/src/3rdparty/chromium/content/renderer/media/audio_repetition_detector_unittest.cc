// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_repetition_detector.h"

#include <stddef.h>

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
const int kDefaultMinLengthMs = 1;
const size_t kDefaultMaxFrames = 480;  // 10 ms * 48 kHz

// Sample rate used in many tests. We choose a special sample rate in order to
// make the test signal obvious.
const int kSampleRateHz = 1000;

}

class AudioRepetitionDetectorForTest : public AudioRepetitionDetector {
 public:
  AudioRepetitionDetectorForTest(int min_length_ms, size_t max_frames,
                                 const int* look_back_times,
                                 size_t num_look_back)
      : AudioRepetitionDetector(
            min_length_ms, max_frames,
            std::vector<int>(look_back_times, look_back_times + num_look_back),
            base::Bind(&AudioRepetitionDetectorForTest::OnRepetitionDetected,
                       base::Unretained(this))) {
  }

  int GetCount(int look_back_ms) const {
    auto it = counters_.find(look_back_ms);
    return it == counters_.end() ? 0 : it->second;
  }

  void ResetCounters() {
    counters_.clear();
  }

 private:
  void OnRepetitionDetected(int look_back_ms) {
    auto it = counters_.find(look_back_ms);
    if (it == counters_.end()) {
      counters_.insert(std::pair<int, size_t>(look_back_ms, 1));
      return;
    }
    it->second++;
  }

  std::map<int, size_t> counters_;
};

class AudioRepetitionDetectorTest : public ::testing::Test {
 public:
  AudioRepetitionDetectorTest()
      : detector_(nullptr) {
  }

 protected:
  struct ExpectedCount {
    int look_back_ms;
    int count;
  };

  // Verify if the counts on the repetition patterns match expectation after
  // injecting a signal. No reset on the counters
  void Verify(const ExpectedCount* expected_counts, size_t num_patterns,
              const float* tester, size_t num_frames,
              int sample_rate_hz, size_t channels = 1) {
    detector_->Detect(tester, num_frames, channels, sample_rate_hz);
    for (size_t idx = 0; idx < num_patterns; ++idx) {
      const int look_back_ms = expected_counts[idx].look_back_ms;
      EXPECT_EQ(expected_counts[idx].count, detector_->GetCount(look_back_ms))
          << "Repetition with look back "
          << look_back_ms
          << " ms counted wrong.";
    }
  }

  void VerifyStereo(const ExpectedCount* expected_counts, size_t num_patterns,
                    const float* tester, size_t num_frames,
                    int sample_rate_hz) {
    static const size_t kNumChannels = 2;

    // Get memory to store interleaved stereo.
    std::unique_ptr<float[]> tester_stereo(
        new float[num_frames * kNumChannels]);

    for (size_t idx = 0; idx < num_frames; ++idx, ++tester) {
      for (size_t channel = 0; channel < kNumChannels; ++channel)
        tester_stereo[idx * kNumChannels + channel] = *tester;
    }

    Verify(expected_counts, num_patterns, tester_stereo.get(),
           num_frames, sample_rate_hz, kNumChannels);
  }

  void SetDetector(int min_length_ms, size_t max_frames,
                   const int* look_back_times, size_t num_look_back) {
    detector_.reset(new AudioRepetitionDetectorForTest(min_length_ms,
                                                       max_frames,
                                                       look_back_times,
                                                       num_look_back));
  }

  void ResetCounters() {
    detector_->ResetCounters();
  }

 private:
  std::unique_ptr<AudioRepetitionDetectorForTest> detector_;
};

TEST_F(AudioRepetitionDetectorTest, Basic) {
  // Check that one look back time will registered only once.
  const int kLookbackTimes[] = {3, 3, 3, 3};

  const float kTestSignal[] = {1, 2, 3, 1, 2, 3};
  const ExpectedCount kExpectedCounts_1[] = {
      {3, 1}
  };
  const ExpectedCount kExpectedCounts_2[] = {
      {3, 1}
  };


  SetDetector(kDefaultMinLengthMs, kDefaultMaxFrames, kLookbackTimes,
              arraysize(kLookbackTimes));
  Verify(kExpectedCounts_1, arraysize(kExpectedCounts_1), kTestSignal,
         arraysize(kTestSignal), kSampleRateHz);
  Verify(kExpectedCounts_2, arraysize(kExpectedCounts_2), kTestSignal,
         arraysize(kTestSignal), kSampleRateHz);
  ResetCounters();

  VerifyStereo(kExpectedCounts_1, arraysize(kExpectedCounts_1), kTestSignal,
               arraysize(kTestSignal), kSampleRateHz);
  VerifyStereo(kExpectedCounts_2, arraysize(kExpectedCounts_2), kTestSignal,
               arraysize(kTestSignal), kSampleRateHz);
}

TEST_F(AudioRepetitionDetectorTest, StereoOutOfSync) {
  const int kLookbackTimes[] = {3};
  const float kTestSignal[] = {
      1, 1,
      2, 2,
      3, 3,
      1, 1,
      2, 2,
      3, 1};
  const ExpectedCount kExpectedCounts[] = {
      {3, 0}
  };

  // By default, any repetition longer than 1 ms (1 sample at 1000 Hz) will be
  // counted as repetition. This test needs to make it longer.
  SetDetector(3, kDefaultMaxFrames, kLookbackTimes, arraysize(kLookbackTimes));
  Verify(kExpectedCounts, arraysize(kExpectedCounts), kTestSignal,
         arraysize(kTestSignal) / 2, kSampleRateHz, 2);
}

TEST_F(AudioRepetitionDetectorTest, IncompletePattern) {
  const int kLookbackTimes[] = {3};
  const float kTestSignal[] = {1, 2, 1, 2, 3, 1, 2, 3};
  const ExpectedCount kExpectedCounts[] = {
      {3, 1},
  };

  SetDetector(kDefaultMinLengthMs, kDefaultMaxFrames, kLookbackTimes,
              arraysize(kLookbackTimes));
  Verify(kExpectedCounts, arraysize(kExpectedCounts), kTestSignal,
         arraysize(kTestSignal), kSampleRateHz);
  ResetCounters();
  VerifyStereo(kExpectedCounts, arraysize(kExpectedCounts), kTestSignal,
               arraysize(kTestSignal), kSampleRateHz);
}

TEST_F(AudioRepetitionDetectorTest, PatternLongerThanFrame) {
  // To make the test signal most obvious, we choose a special sample rate.
  const int kSampleRateHz = 1000;

  const int kLookbackTimes[] = {6};
  const float kTestSignal_1[] = {1, 2, 3, 4, 5};
  const float kTestSignal_2[] = {6, 1, 2, 3, 4, 5, 6};
  const ExpectedCount kExpectedCounts_1[] = {
      {6, 0},
  };
  const ExpectedCount kExpectedCounts_2[] = {
      {6, 1},
  };

  SetDetector(kDefaultMinLengthMs, kDefaultMaxFrames, kLookbackTimes,
              arraysize(kLookbackTimes));
 Verify(kExpectedCounts_1, arraysize(kExpectedCounts_1), kTestSignal_1,
         arraysize(kTestSignal_1), kSampleRateHz);
  Verify(kExpectedCounts_2, arraysize(kExpectedCounts_2), kTestSignal_2,
         arraysize(kTestSignal_2), kSampleRateHz);
  ResetCounters();
  VerifyStereo(kExpectedCounts_1, arraysize(kExpectedCounts_1), kTestSignal_1,
               arraysize(kTestSignal_1), kSampleRateHz);
  VerifyStereo(kExpectedCounts_2, arraysize(kExpectedCounts_2), kTestSignal_2,
               arraysize(kTestSignal_2), kSampleRateHz);
}

TEST_F(AudioRepetitionDetectorTest, TwoPatterns) {
  const int kLookbackTimes[] = {3, 4};
  const float kTestSignal[] = {1, 2, 3, 1, 2, 3, 4, 1, 2, 3, 4};
  const ExpectedCount kExpectedCounts[] = {
      // 1,2,3 belongs to both patterns.
      {3, 1},
      {4, 1}
  };

  SetDetector(kDefaultMinLengthMs, kDefaultMaxFrames, kLookbackTimes,
              arraysize(kLookbackTimes));
  Verify(kExpectedCounts, arraysize(kExpectedCounts), kTestSignal,
         arraysize(kTestSignal), kSampleRateHz);
  ResetCounters();
  VerifyStereo(kExpectedCounts, arraysize(kExpectedCounts), kTestSignal,
               arraysize(kTestSignal), kSampleRateHz);
}

TEST_F(AudioRepetitionDetectorTest, MaxFramesShorterThanInput) {
  // To make the test signal most obvious, we choose a special sample rate.
  const int kSampleRateHz = 1000;

  const int kLookbackTimes[] = {3, 4};
  const float kTestSignal[] = {1, 2, 3, 1, 2, 3, 4, 1, 2, 3, 4};
  const ExpectedCount kExpectedCounts[] = {
      // 1,2,3 belongs to both patterns.
      {3, 1},
      {4, 1}
  };

  // length of kTestSignal is 11 but I set maximum frames to be 2. The detection
  // should still work.
  SetDetector(kDefaultMinLengthMs, 2, kLookbackTimes,arraysize(kLookbackTimes));
  Verify(kExpectedCounts, arraysize(kExpectedCounts), kTestSignal,
         arraysize(kTestSignal), kSampleRateHz);
  ResetCounters();
  VerifyStereo(kExpectedCounts, arraysize(kExpectedCounts), kTestSignal,
               arraysize(kTestSignal), kSampleRateHz);
}

TEST_F(AudioRepetitionDetectorTest, NestedPatterns) {
  const int kLookbackTimes[] = {6, 3};
  const float kTestSignal[] = {1, 2, 3, 1, 2, 3};
  const ExpectedCount kExpectedCounts_1[] = {
      {3, 1},
      {6, 0}
  };
  const ExpectedCount kExpectedCounts_2[] = {
      {3, 1},
      {6, 1}
  };

  SetDetector(kDefaultMinLengthMs, kDefaultMaxFrames, kLookbackTimes,
              arraysize(kLookbackTimes));
  Verify(kExpectedCounts_1, arraysize(kExpectedCounts_1), kTestSignal,
         arraysize(kTestSignal), kSampleRateHz);
  Verify(kExpectedCounts_2, arraysize(kExpectedCounts_2), kTestSignal,
         arraysize(kTestSignal), kSampleRateHz);
  ResetCounters();
  VerifyStereo(kExpectedCounts_1, arraysize(kExpectedCounts_1), kTestSignal,
               arraysize(kTestSignal), kSampleRateHz);
  VerifyStereo(kExpectedCounts_2, arraysize(kExpectedCounts_2), kTestSignal,
               arraysize(kTestSignal), kSampleRateHz);
}

TEST_F(AudioRepetitionDetectorTest, NotFullLengthPattern) {
  const int kLookbackTimes[] = {4};
  const float kTestSignal[] = {1, 2, 3, -1, 1, 2, 3, -2};
  const ExpectedCount kExpectedCounts[] = {
      {4, 1},
  };

  SetDetector(kDefaultMinLengthMs, kDefaultMaxFrames, kLookbackTimes,
              arraysize(kLookbackTimes));
  Verify(kExpectedCounts, arraysize(kExpectedCounts), kTestSignal,
         arraysize(kTestSignal), kSampleRateHz);
  ResetCounters();
  VerifyStereo(kExpectedCounts, arraysize(kExpectedCounts), kTestSignal,
               arraysize(kTestSignal), kSampleRateHz);
}

TEST_F(AudioRepetitionDetectorTest, DcCountOrNot) {
  const int kLookbackTimes[] = {3};
  const float kDc = 1.2345f;
  const float kTestSignal_1[] = {kDc, kDc, kDc, kDc, kDc, kDc};
  const float kTestSignal_2[] = {kDc, 1, 2, kDc, 1, 2};
  const ExpectedCount kExpectedCounts_1[] = {
      // Full zeros won't count.
      {3, 0},
  };
  const ExpectedCount kExpectedCounts_2[] = {
      // Partial zero will count.
      {3, 1},
  };

  SetDetector(kDefaultMinLengthMs, kDefaultMaxFrames, kLookbackTimes,
              arraysize(kLookbackTimes));
  Verify(kExpectedCounts_1, arraysize(kExpectedCounts_1), kTestSignal_1,
         arraysize(kTestSignal_1), kSampleRateHz);
  Verify(kExpectedCounts_2, arraysize(kExpectedCounts_2), kTestSignal_2,
         arraysize(kTestSignal_2), kSampleRateHz);
  ResetCounters();
  VerifyStereo(kExpectedCounts_1, arraysize(kExpectedCounts_1), kTestSignal_1,
               arraysize(kTestSignal_1), kSampleRateHz);
  VerifyStereo(kExpectedCounts_2, arraysize(kExpectedCounts_2), kTestSignal_2,
               arraysize(kTestSignal_2), kSampleRateHz);
}

// Previous tests use short signal to test the detection algorithm, this one
// tests a normal frame size
TEST_F(AudioRepetitionDetectorTest, NormalSignal) {
  const int kNormalSampleRateHz = 44100;
  // Let the signal be "*(4ms)-A(13ms)-*(100ms)-A", where * denotes random
  // samples.
  const size_t kPreSamples = kNormalSampleRateHz * 4 / 1000;
  const size_t kRepSamples = kNormalSampleRateHz * 13 / 1000;
  const size_t kSkipSamples = kNormalSampleRateHz * 100 / 1000;
  const size_t kSamples = kPreSamples + kRepSamples * 2 + kSkipSamples;

  const int kLookbackTimes[] = {80, 90, 100, 110, 120};

  float test_signal[kSamples];
  size_t idx = 0;
  for (; idx < kPreSamples + kRepSamples + kSkipSamples; ++idx)
    test_signal[idx] = static_cast<float>(base::RandDouble());

  for (; idx < kSamples; ++idx)
    test_signal[idx] = test_signal[idx - kSkipSamples];

  ExpectedCount expect_counts[arraysize(kLookbackTimes)];
  for (size_t i = 0; i < arraysize(kLookbackTimes); ++i) {
    expect_counts[i].look_back_ms = kLookbackTimes[i];
    expect_counts[i].count = 0;
  }

  // We only expect a repetition with 100 ms look back time.
  expect_counts[2].count = 1;

  SetDetector(kDefaultMinLengthMs, kDefaultMaxFrames, kLookbackTimes,
              arraysize(kLookbackTimes));
  Verify(expect_counts, arraysize(expect_counts), test_signal, kSamples,
         kNormalSampleRateHz);
}

}  // namespace content
