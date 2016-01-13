// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/channel_layout.h"
#include "media/base/fake_audio_render_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kChannels = 6;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_5_1;
// Use a buffer size which is intentionally not a multiple of kChannelAlignment.
static const int kFrameCount = media::AudioBus::kChannelAlignment * 32 - 1;
static const int kSampleRate = 48000;

class AudioBusTest : public testing::Test {
 public:
  AudioBusTest() {}
  virtual ~AudioBusTest() {
    for (size_t i = 0; i < data_.size(); ++i)
      base::AlignedFree(data_[i]);
  }

  // Validate parameters returned by AudioBus v.s. the constructed parameters.
  void VerifyParams(AudioBus* bus) {
    EXPECT_EQ(kChannels, bus->channels());
    EXPECT_EQ(kFrameCount, bus->frames());
  }

  void VerifyValue(const float data[], int size, float value) {
    for (int i = 0; i < size; ++i)
      ASSERT_FLOAT_EQ(value, data[i]) << "i=" << i;
  }

  // Verify values for each channel in |result| are within |epsilon| of
  // |expected|.  If |epsilon| exactly equals 0, uses FLOAT_EQ macro.
  void VerifyBusWithEpsilon(const AudioBus* result, const AudioBus* expected,
                            float epsilon) {
    ASSERT_EQ(expected->channels(), result->channels());
    ASSERT_EQ(expected->frames(), result->frames());
    for (int ch = 0; ch < result->channels(); ++ch) {
      for (int i = 0; i < result->frames(); ++i) {
        SCOPED_TRACE(base::StringPrintf("ch=%d, i=%d", ch, i));
        if (epsilon == 0) {
          ASSERT_FLOAT_EQ(expected->channel(ch)[i], result->channel(ch)[i]);
        } else {
          ASSERT_NEAR(expected->channel(ch)[i], result->channel(ch)[i],
                      epsilon);
        }
      }
    }
  }

  // Verify values for each channel in |result| against |expected|.
  void VerifyBus(const AudioBus* result, const AudioBus* expected) {
    VerifyBusWithEpsilon(result, expected, 0);
  }

  // Read and write to the full extent of the allocated channel data.  Also test
  // the Zero() method and verify it does as advertised.  Also test data if data
  // is 16-byte aligned as advertised (see kChannelAlignment in audio_bus.h).
  void VerifyChannelData(AudioBus* bus) {
    for (int i = 0; i < bus->channels(); ++i) {
      ASSERT_EQ(0U, reinterpret_cast<uintptr_t>(
          bus->channel(i)) & (AudioBus::kChannelAlignment - 1));
      std::fill(bus->channel(i), bus->channel(i) + bus->frames(), i);
    }

    for (int i = 0; i < bus->channels(); ++i)
      VerifyValue(bus->channel(i), bus->frames(), i);

    bus->Zero();
    for (int i = 0; i < bus->channels(); ++i)
      VerifyValue(bus->channel(i), bus->frames(), 0);
  }

  // Verify copying to and from |bus1| and |bus2|.
  void CopyTest(AudioBus* bus1, AudioBus* bus2) {
    // Fill |bus1| with dummy data.
    for (int i = 0; i < bus1->channels(); ++i)
      std::fill(bus1->channel(i), bus1->channel(i) + bus1->frames(), i);

    // Verify copy from |bus1| to |bus2|.
    bus2->Zero();
    bus1->CopyTo(bus2);
    VerifyBus(bus1, bus2);

    // Verify copy from |bus2| to |bus1|.
    bus1->Zero();
    bus2->CopyTo(bus1);
    VerifyBus(bus2, bus1);
  }

 protected:
  std::vector<float*> data_;

  DISALLOW_COPY_AND_ASSIGN(AudioBusTest);
};

// Verify basic Create(...) method works as advertised.
TEST_F(AudioBusTest, Create) {
  scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrameCount);
  VerifyParams(bus.get());
  VerifyChannelData(bus.get());
}

// Verify Create(...) using AudioParameters works as advertised.
TEST_F(AudioBusTest, CreateUsingAudioParameters) {
  scoped_ptr<AudioBus> bus = AudioBus::Create(AudioParameters(
      AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate, 32,
      kFrameCount));
  VerifyParams(bus.get());
  VerifyChannelData(bus.get());
}

// Verify an AudioBus created via wrapping a vector works as advertised.
TEST_F(AudioBusTest, WrapVector) {
  data_.reserve(kChannels);
  for (int i = 0; i < kChannels; ++i) {
    data_.push_back(static_cast<float*>(base::AlignedAlloc(
        sizeof(*data_[i]) * kFrameCount, AudioBus::kChannelAlignment)));
  }

  scoped_ptr<AudioBus> bus = AudioBus::WrapVector(kFrameCount, data_);
  VerifyParams(bus.get());
  VerifyChannelData(bus.get());
}

// Verify an AudioBus created via wrapping a memory block works as advertised.
TEST_F(AudioBusTest, WrapMemory) {
  AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate, 32,
      kFrameCount);
  int data_size = AudioBus::CalculateMemorySize(params);
  scoped_ptr<float, base::AlignedFreeDeleter> data(static_cast<float*>(
      base::AlignedAlloc(data_size, AudioBus::kChannelAlignment)));

  // Fill the memory with a test value we can check for after wrapping.
  static const float kTestValue = 3;
  std::fill(
      data.get(), data.get() + data_size / sizeof(*data.get()), kTestValue);

  scoped_ptr<AudioBus> bus = AudioBus::WrapMemory(params, data.get());
  // Verify the test value we filled prior to wrapping.
  for (int i = 0; i < bus->channels(); ++i)
    VerifyValue(bus->channel(i), bus->frames(), kTestValue);
  VerifyParams(bus.get());
  VerifyChannelData(bus.get());

  // Verify the channel vectors lie within the provided memory block.
  EXPECT_GE(bus->channel(0), data.get());
  EXPECT_LT(bus->channel(bus->channels() - 1) + bus->frames(),
            data.get() + data_size / sizeof(*data.get()));
}

// Simulate a shared memory transfer and verify results.
TEST_F(AudioBusTest, CopyTo) {
  // Create one bus with AudioParameters and the other through direct values to
  // test for parity between the Create() functions.
  AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate, 32,
      kFrameCount);
  scoped_ptr<AudioBus> bus1 = AudioBus::Create(kChannels, kFrameCount);
  scoped_ptr<AudioBus> bus2 = AudioBus::Create(params);

  {
    SCOPED_TRACE("Created");
    CopyTest(bus1.get(), bus2.get());
  }
  {
    SCOPED_TRACE("Wrapped Vector");
    // Try a copy to an AudioBus wrapping a vector.
    data_.reserve(kChannels);
    for (int i = 0; i < kChannels; ++i) {
      data_.push_back(static_cast<float*>(base::AlignedAlloc(
          sizeof(*data_[i]) * kFrameCount, AudioBus::kChannelAlignment)));
    }

    bus2 = AudioBus::WrapVector(kFrameCount, data_);
    CopyTest(bus1.get(), bus2.get());
  }
  {
    SCOPED_TRACE("Wrapped Memory");
    // Try a copy to an AudioBus wrapping a memory block.
    scoped_ptr<float, base::AlignedFreeDeleter> data(
        static_cast<float*>(base::AlignedAlloc(
            AudioBus::CalculateMemorySize(params),
            AudioBus::kChannelAlignment)));

    bus2 = AudioBus::WrapMemory(params, data.get());
    CopyTest(bus1.get(), bus2.get());
  }
}

// Verify Zero() and ZeroFrames(...) utility methods work as advertised.
TEST_F(AudioBusTest, Zero) {
  scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrameCount);

  // Fill the bus with dummy data.
  for (int i = 0; i < bus->channels(); ++i)
    std::fill(bus->channel(i), bus->channel(i) + bus->frames(), i + 1);

  // Zero first half the frames of each channel.
  bus->ZeroFrames(kFrameCount / 2);
  for (int i = 0; i < bus->channels(); ++i) {
    SCOPED_TRACE("First Half Zero");
    VerifyValue(bus->channel(i), kFrameCount / 2, 0);
    VerifyValue(bus->channel(i) + kFrameCount / 2,
                kFrameCount - kFrameCount / 2, i + 1);
  }

  // Fill the bus with dummy data.
  for (int i = 0; i < bus->channels(); ++i)
    std::fill(bus->channel(i), bus->channel(i) + bus->frames(), i + 1);

  // Zero the last half of the frames.
  bus->ZeroFramesPartial(kFrameCount / 2, kFrameCount - kFrameCount / 2);
  for (int i = 0; i < bus->channels(); ++i) {
    SCOPED_TRACE("Last Half Zero");
    VerifyValue(bus->channel(i) + kFrameCount / 2,
                kFrameCount - kFrameCount / 2, 0);
    VerifyValue(bus->channel(i), kFrameCount / 2, i + 1);
  }

  // Fill the bus with dummy data.
  for (int i = 0; i < bus->channels(); ++i)
    std::fill(bus->channel(i), bus->channel(i) + bus->frames(), i + 1);

  // Zero all the frames of each channel.
  bus->Zero();
  for (int i = 0; i < bus->channels(); ++i) {
    SCOPED_TRACE("All Zero");
    VerifyValue(bus->channel(i), bus->frames(), 0);
  }
}

// Each test vector represents two channels of data in the following arbitrary
// layout: <min, zero, max, min, max / 2, min / 2, zero, max, zero, zero>.
static const int kTestVectorSize = 10;
static const uint8 kTestVectorUint8[kTestVectorSize] = {
    0, -kint8min, kuint8max, 0, kint8max / 2 + 128, kint8min / 2 + 128,
    -kint8min, kuint8max, -kint8min, -kint8min };
static const int16 kTestVectorInt16[kTestVectorSize] = {
    kint16min, 0, kint16max, kint16min, kint16max / 2, kint16min / 2,
    0, kint16max, 0, 0 };
static const int32 kTestVectorInt32[kTestVectorSize] = {
    kint32min, 0, kint32max, kint32min, kint32max / 2, kint32min / 2,
    0, kint32max, 0, 0 };

// Expected results.
static const int kTestVectorFrames = kTestVectorSize / 2;
static const float kTestVectorResult[][kTestVectorFrames] = {
    { -1, 1, 0.5, 0, 0 }, { 0, -1, -0.5, 1, 0 }};
static const int kTestVectorChannels = arraysize(kTestVectorResult);

// Verify FromInterleaved() deinterleaves audio in supported formats correctly.
TEST_F(AudioBusTest, FromInterleaved) {
  scoped_ptr<AudioBus> bus = AudioBus::Create(
      kTestVectorChannels, kTestVectorFrames);
  scoped_ptr<AudioBus> expected = AudioBus::Create(
      kTestVectorChannels, kTestVectorFrames);
  for (int ch = 0; ch < kTestVectorChannels; ++ch) {
    memcpy(expected->channel(ch), kTestVectorResult[ch],
           kTestVectorFrames * sizeof(*expected->channel(ch)));
  }
  {
    SCOPED_TRACE("uint8");
    bus->Zero();
    bus->FromInterleaved(
        kTestVectorUint8, kTestVectorFrames, sizeof(*kTestVectorUint8));
    // Biased uint8 calculations have poor precision, so the epsilon here is
    // slightly more permissive than int16 and int32 calculations.
    VerifyBusWithEpsilon(bus.get(), expected.get(), 1.0f / (kuint8max - 1));
  }
  {
    SCOPED_TRACE("int16");
    bus->Zero();
    bus->FromInterleaved(
        kTestVectorInt16, kTestVectorFrames, sizeof(*kTestVectorInt16));
    VerifyBusWithEpsilon(bus.get(), expected.get(), 1.0f / (kuint16max + 1.0f));
  }
  {
    SCOPED_TRACE("int32");
    bus->Zero();
    bus->FromInterleaved(
        kTestVectorInt32, kTestVectorFrames, sizeof(*kTestVectorInt32));
    VerifyBusWithEpsilon(bus.get(), expected.get(), 1.0f / (kuint32max + 1.0f));
  }
}

// Verify FromInterleavedPartial() deinterleaves audio correctly.
TEST_F(AudioBusTest, FromInterleavedPartial) {
  // Only deinterleave the middle two frames in each channel.
  static const int kPartialStart = 1;
  static const int kPartialFrames = 2;
  ASSERT_LE(kPartialStart + kPartialFrames, kTestVectorFrames);

  scoped_ptr<AudioBus> bus = AudioBus::Create(
      kTestVectorChannels, kTestVectorFrames);
  scoped_ptr<AudioBus> expected = AudioBus::Create(
      kTestVectorChannels, kTestVectorFrames);
  expected->Zero();
  for (int ch = 0; ch < kTestVectorChannels; ++ch) {
    memcpy(expected->channel(ch) + kPartialStart,
           kTestVectorResult[ch] + kPartialStart,
           kPartialFrames * sizeof(*expected->channel(ch)));
  }

  bus->Zero();
  bus->FromInterleavedPartial(
      kTestVectorInt32 + kPartialStart * bus->channels(), kPartialStart,
      kPartialFrames, sizeof(*kTestVectorInt32));
  VerifyBus(bus.get(), expected.get());
}

// Verify ToInterleaved() interleaves audio in suported formats correctly.
TEST_F(AudioBusTest, ToInterleaved) {
  scoped_ptr<AudioBus> bus = AudioBus::Create(
      kTestVectorChannels, kTestVectorFrames);
  // Fill the bus with our test vector.
  for (int ch = 0; ch < bus->channels(); ++ch) {
    memcpy(bus->channel(ch), kTestVectorResult[ch],
           kTestVectorFrames * sizeof(*bus->channel(ch)));
  }
  {
    SCOPED_TRACE("uint8");
    uint8 test_array[arraysize(kTestVectorUint8)];
    bus->ToInterleaved(bus->frames(), sizeof(*kTestVectorUint8), test_array);
    ASSERT_EQ(memcmp(
        test_array, kTestVectorUint8, sizeof(kTestVectorUint8)), 0);
  }
  {
    SCOPED_TRACE("int16");
    int16 test_array[arraysize(kTestVectorInt16)];
    bus->ToInterleaved(bus->frames(), sizeof(*kTestVectorInt16), test_array);
    ASSERT_EQ(memcmp(
        test_array, kTestVectorInt16, sizeof(kTestVectorInt16)), 0);
  }
  {
    SCOPED_TRACE("int32");
    int32 test_array[arraysize(kTestVectorInt32)];
    bus->ToInterleaved(bus->frames(), sizeof(*kTestVectorInt32), test_array);

    // Some compilers get better precision than others on the half-max test, so
    // let the test pass with an off by one check on the half-max.
    int32 fixed_test_array[arraysize(kTestVectorInt32)];
    memcpy(fixed_test_array, kTestVectorInt32, sizeof(kTestVectorInt32));
    ASSERT_EQ(fixed_test_array[4], kint32max / 2);
    fixed_test_array[4]++;

    ASSERT_TRUE(
       memcmp(test_array, kTestVectorInt32, sizeof(kTestVectorInt32)) == 0 ||
       memcmp(test_array, fixed_test_array, sizeof(fixed_test_array)) == 0);
  }
}

// Verify ToInterleavedPartial() interleaves audio correctly.
TEST_F(AudioBusTest, ToInterleavedPartial) {
  // Only interleave the middle two frames in each channel.
  static const int kPartialStart = 1;
  static const int kPartialFrames = 2;
  ASSERT_LE(kPartialStart + kPartialFrames, kTestVectorFrames);

  scoped_ptr<AudioBus> expected = AudioBus::Create(
      kTestVectorChannels, kTestVectorFrames);
  for (int ch = 0; ch < kTestVectorChannels; ++ch) {
    memcpy(expected->channel(ch), kTestVectorResult[ch],
           kTestVectorFrames * sizeof(*expected->channel(ch)));
  }

  int16 test_array[arraysize(kTestVectorInt16)];
  expected->ToInterleavedPartial(
      kPartialStart, kPartialFrames, sizeof(*kTestVectorInt16), test_array);
  ASSERT_EQ(memcmp(
      test_array, kTestVectorInt16 + kPartialStart * kTestVectorChannels,
      kPartialFrames * sizeof(*kTestVectorInt16) * kTestVectorChannels), 0);
}

TEST_F(AudioBusTest, Scale) {
  scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrameCount);

  // Fill the bus with dummy data.
  static const float kFillValue = 1;
  for (int i = 0; i < bus->channels(); ++i)
    std::fill(bus->channel(i), bus->channel(i) + bus->frames(), kFillValue);

  // Adjust by an invalid volume and ensure volume is unchanged.
  bus->Scale(-1);
  for (int i = 0; i < bus->channels(); ++i) {
    SCOPED_TRACE("Invalid Scale");
    VerifyValue(bus->channel(i), bus->frames(), kFillValue);
  }

  // Verify correct volume adjustment.
  static const float kVolume = 0.5;
  bus->Scale(kVolume);
  for (int i = 0; i < bus->channels(); ++i) {
    SCOPED_TRACE("Half Scale");
    VerifyValue(bus->channel(i), bus->frames(), kFillValue * kVolume);
  }

  // Verify zero volume case.
  bus->Scale(0);
  for (int i = 0; i < bus->channels(); ++i) {
    SCOPED_TRACE("Zero Scale");
    VerifyValue(bus->channel(i), bus->frames(), 0);
  }
}

}  // namespace media
