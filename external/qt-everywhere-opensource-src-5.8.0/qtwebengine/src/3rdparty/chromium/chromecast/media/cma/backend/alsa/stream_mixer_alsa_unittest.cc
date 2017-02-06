// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/media/cma/backend/alsa/mock_alsa_wrapper.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromecast {
namespace media {

namespace {

// Testing constants that are common to multiple test cases.
const size_t kBytesPerSample = sizeof(int32_t);
const int kNumChannels = 2;
const int kTestMaxReadSize = 4096;

// kTestSamplesPerSecond needs to be higher than kLowSampleRateCutoff for the
// mixer to use it.
const int kTestSamplesPerSecond = 54321;

// This array holds |NUM_DATA_SETS| sets of arbitrary interleaved float data.
// Each set holds |NUM_SAMPLES| / kNumChannels frames of data.
#define NUM_DATA_SETS 2u
#define NUM_SAMPLES 64u

// Note: Test data should be represented as 32-bit integers and copied into
// ::media::AudioBus instances, rather than wrapping statically declared float
// arrays. The latter method is brittle, as ::media::AudioBus requires 16-bit
// alignment for internal data.
const int32_t kTestData[NUM_DATA_SETS][NUM_SAMPLES] = {
  {
     74343736,   -1333200799,
    -1360871126,  1138806283,
     1931811865,  1856308487,
     649203634,   564640023,
     1676630678,  23416591,
    -1293255456,  547928305,
    -976258952,   1840550252,
     1714525174,  358704931,
     983646295,   1264863573,
     442473973,   1222979052,
     317404525,   366912613,
     1393280948, -1022004648,
    -2054669405, -159762261,
     1127018745, -1984491787,
     1406988336, -693327981,
    -1549544744,  1232236854,
     970338338,  -1750160519,
    -783213057,   1231504562,
     1155296810, -820018779,
     1155689800, -1108462340,
    -150535168,   1033717023,
     2121241397,  1829995370,
    -1893006836, -819097508,
    -495186107,   1001768909,
    -1441111852,  692174781,
     1916569026, -687787473,
    -910565280,   1695751872,
     994166817,   1775451433,
     909418522,   492671403,
    -761744663,  -2064315902,
     1357716471, -1580019684,
     1872702377, -1524457840,
  }, {
     1951643876,  712069070,
     1105286211,  1725522438,
    -986938388,   229538084,
     1042753634,  1888456317,
     1477803757,  1486284170,
    -340193623,  -1828672521,
     1418790906, -724453609,
    -1057163251,  1408558147,
    -31441309,    1421569750,
    -1231100836,  545866721,
     1430262764,  2107819625,
    -2077050480, -1128358776,
    -1799818931, -1041097926,
     1911058583, -1177896929,
    -1911123008, -929110948,
     1267464176,  172218310,
    -2048128170, -2135590884,
     734347065,   1214930283,
     1301338583, -326962976,
    -498269894,  -1167887508,
    -589067650,   591958162,
     592999692,  -788367017,
    -1389422,     1466108561,
     386162657,   1389031078,
     936083827,  -1438801160,
     1340850135, -1616803932,
    -850779335,   1666492408,
     1290349909, -492418001,
     659200170,  -542374913,
    -120005682,   1030923147,
    -877887021,  -870241979,
     1322678128, -344799975,
  }
};

// Return a scoped pointer filled with the data laid out at |index| above.
std::unique_ptr<::media::AudioBus> GetTestData(size_t index) {
  CHECK_LT(index, NUM_DATA_SETS);
  int frames = NUM_SAMPLES / kNumChannels;
  auto data = ::media::AudioBus::Create(kNumChannels, frames);
  data->FromInterleaved(kTestData[index], frames, kBytesPerSample);
  return data;
}

class MockInputQueue : public StreamMixerAlsa::InputQueue {
 public:
  explicit MockInputQueue(int samples_per_second)
      : paused_(true),
        samples_per_second_(samples_per_second),
        max_read_size_(kTestMaxReadSize),
        multiplier_(1.0),
        primary_(true),
        deleting_(false) {
    ON_CALL(*this, GetResampledData(_, _)).WillByDefault(
        testing::Invoke(this, &MockInputQueue::DoGetResampledData));
    ON_CALL(*this, PrepareToDelete(_)).WillByDefault(
        testing::Invoke(this, &MockInputQueue::DoPrepareToDelete));
  }
  ~MockInputQueue() override {}

  bool paused() const { return paused_; }

  // StreamMixerAlsa::InputQueue implementation:
  int input_samples_per_second() const override { return samples_per_second_; }
  float volume_multiplier() const override { return multiplier_; }
  bool primary() const override { return primary_; }
  bool IsDeleting() const override { return deleting_; }
  MOCK_METHOD1(Initialize,
               void(const MediaPipelineBackendAlsa::RenderingDelay&
                        mixer_rendering_delay));
  int MaxReadSize() override { return max_read_size_; }
  MOCK_METHOD2(GetResampledData, void(::media::AudioBus* dest, int frames));
  MOCK_METHOD1(AfterWriteFrames,
               void(const MediaPipelineBackendAlsa::RenderingDelay&
                        mixer_rendering_delay));
  MOCK_METHOD1(SignalError, void(StreamMixerAlsaInput::MixerError error));
  MOCK_METHOD1(PrepareToDelete, void(const OnReadyToDeleteCb& delete_cb));

  // Setters and getters for test control.
  void SetPaused(bool paused) { paused_ = paused; }
  void SetMaxReadSize(int max_read_size) { max_read_size_ = max_read_size; }
  void SetData(std::unique_ptr<::media::AudioBus> data) {
    CHECK(!data_);
    data_ = std::move(data);
    max_read_size_ = data_->frames();
  }
  void SetVolumeMultiplier(float multiplier) {
    CHECK(multiplier >= 0.0 && multiplier <= 1.0);
    multiplier_ = multiplier;
  }
  void SetPrimary(bool primary) { primary_ = primary; }
  const ::media::AudioBus& data() {
    CHECK(data_);
    return *data_;
  }
  float multiplier() const { return multiplier_; }

 private:
  void DoGetResampledData(::media::AudioBus* dest, int frames) {
    CHECK(dest);
    CHECK_GE(dest->frames(), frames);
    if (data_) {
      data_->CopyPartialFramesTo(0, frames, 0, dest);
    } else {
      dest->ZeroFramesPartial(0, frames);
    }
  }

  void DoPrepareToDelete(const OnReadyToDeleteCb& delete_cb) {
    deleting_ = true;
    delete_cb.Run(this);
  }

  bool paused_;
  int samples_per_second_;
  int max_read_size_;
  float multiplier_;
  bool primary_;
  bool deleting_;
  std::unique_ptr<::media::AudioBus> data_;

  DISALLOW_COPY_AND_ASSIGN(MockInputQueue);
};

// Given |inputs|, returns mixed audio data according to the mixing method used
// by the mixer.
std::unique_ptr<::media::AudioBus> GetMixedAudioData(
    const std::vector<testing::StrictMock<MockInputQueue>*>& inputs) {
  int read_size = std::numeric_limits<int>::max();
  for (const auto input : inputs) {
    CHECK(input);
    read_size = std::min(input->MaxReadSize(), read_size);
  }

  // Verify all inputs are the right size.
  for (const auto input : inputs) {
    CHECK_EQ(kNumChannels, input->data().channels());
    CHECK_LE(read_size, input->data().frames());
  }

  // Currently, the mixing algorithm is simply to sum the scaled, clipped input
  // streams. Go sample-by-sample and mix the data.
  auto mixed = ::media::AudioBus::Create(kNumChannels, read_size);
  for (int c = 0; c < mixed->channels(); ++c) {
    for (int f = 0; f < read_size; ++f) {
      float* result = mixed->channel(c) + f;

      // Sum the sample from each input stream, scaling each stream.
      *result = 0.0;
      for (const auto input : inputs)
        *result += *(input->data().channel(c) + f) * input->multiplier();

      // Clamp the mixed sample between 1.0 and -1.0.
      *result = std::min(1.0f, std::max(-1.0f, *result));
    }
  }
  return mixed;
}

// Like the method above, but accepts a single input. This returns an AudioBus
// with this input after it is scaled and clipped.
std::unique_ptr<::media::AudioBus> GetMixedAudioData(
    testing::StrictMock<MockInputQueue>* input) {
  return GetMixedAudioData(
      std::vector<testing::StrictMock<MockInputQueue>*>(1, input));
}

// Asserts that |expected| matches |actual| exactly.
void CompareAudioData(const ::media::AudioBus& expected,
                      const ::media::AudioBus& actual) {
  ASSERT_EQ(expected.channels(), actual.channels());
  ASSERT_EQ(expected.frames(), actual.frames());
  for (int c = 0; c < expected.channels(); ++c) {
    const float* expected_data = expected.channel(c);
    const float* actual_data = actual.channel(c);
    for (int f = 0; f < expected.frames(); ++f)
      ASSERT_FLOAT_EQ(*expected_data++, *actual_data++) << c << " " << f;
  }
}

}  // namespace

class StreamMixerAlsaTest : public testing::Test {
 protected:
  StreamMixerAlsaTest()
      : message_loop_(new base::MessageLoop()),
        mock_alsa_(new testing::NiceMock<MockAlsaWrapper>()) {
    StreamMixerAlsa::MakeSingleThreadedForTest();
    StreamMixerAlsa::Get()->SetAlsaWrapperForTest(base::WrapUnique(mock_alsa_));
  }

  ~StreamMixerAlsaTest() override {
    StreamMixerAlsa::Get()->ClearInputsForTest();
  }

  MockAlsaWrapper* mock_alsa() { return mock_alsa_; }

 private:
  const std::unique_ptr<base::MessageLoop> message_loop_;
  testing::NiceMock<MockAlsaWrapper>* mock_alsa_;

  DISALLOW_COPY_AND_ASSIGN(StreamMixerAlsaTest);
};

TEST_F(StreamMixerAlsaTest, AddSingleInput) {
  auto input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();

  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(base::WrapUnique(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());
}

TEST_F(StreamMixerAlsaTest, AddMultipleInputs) {
  auto input1 = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  auto input2 =
      new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond * 2);
  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();

  EXPECT_CALL(*input1, Initialize(_)).Times(1);
  EXPECT_CALL(*input2, Initialize(_)).Times(1);
  mixer->AddInput(base::WrapUnique(input1));
  mixer->AddInput(base::WrapUnique(input2));

  // The mixer should be ready to play, and should sample to the initial
  // sample rate.
  EXPECT_EQ(kTestSamplesPerSecond, mixer->output_samples_per_second());
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());
}

TEST_F(StreamMixerAlsaTest, RemoveInput) {
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  const int kNumInputs = 3;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(new testing::StrictMock<MockInputQueue>(
        kTestSamplesPerSecond * (i + 1)));
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], PrepareToDelete(_)).Times(1);
    mixer->RemoveInput(inputs[i]);
  }

  // Need to wait for the removal task (it is always posted).
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(mixer->empty());
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());
}

TEST_F(StreamMixerAlsaTest, WriteFrames) {
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  const int kNumInputs = 3;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(
        new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond));
    inputs.back()->SetPaused(false);
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  ASSERT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // The mixer should pull data from all streams, using the smallest
  // MaxReadSize provided by any of the channels.
  // TODO(slan): Check that the proper number of frames is pulled.
  ASSERT_EQ(3u, inputs.size());
  inputs[0]->SetMaxReadSize(1024);
  inputs[1]->SetMaxReadSize(512);
  inputs[2]->SetMaxReadSize(2048);
  for (const auto input : inputs) {
    EXPECT_CALL(*input, GetResampledData(_, 512)).Times(1);
    EXPECT_CALL(*input, AfterWriteFrames(_)).Times(1);
  }

  // TODO(slan): Verify that the data is mixed properly with math.
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 512)).Times(1);
  mixer->WriteFramesForTest();

  // Make two of these streams non-primary, and exhaust a non-primary stream.
  // All non-empty streams shall be polled for data and the mixer shall write
  // to ALSA.
  inputs[1]->SetPrimary(false);
  inputs[1]->SetMaxReadSize(0);
  inputs[2]->SetPrimary(false);
  for (const auto input : inputs) {
    if (input != inputs[1])
      EXPECT_CALL(*input, GetResampledData(_, 1024)).Times(1);
    EXPECT_CALL(*input, AfterWriteFrames(_)).Times(1);
  }
  // Note that the new smallest stream shall dictate the length of the write.
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 1024)).Times(1);
  mixer->WriteFramesForTest();

  // Exhaust a primary stream. No streams shall be polled for data, and no
  // data shall be written to ALSA.
  inputs[0]->SetMaxReadSize(0);
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, _)).Times(0);
  mixer->WriteFramesForTest();
}

TEST_F(StreamMixerAlsaTest, OneStreamMixesProperly) {
  auto input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  input->SetPaused(false);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(base::WrapUnique(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // Populate the stream with data.
  const int kNumFrames = 32;
  input->SetData(GetTestData(0));

  ASSERT_EQ(mock_alsa()->data().size(), 0u);

  // Write the stream to ALSA.
  EXPECT_CALL(*input, GetResampledData(_, kNumFrames));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  mixer->WriteFramesForTest();

  // Get the actual stream rendered to ALSA, and compare it against the
  // expected stream. The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ASSERT_GT(mock_alsa()->data().size(), 0u);
  actual->FromInterleaved(
      &(mock_alsa()->data()[0]), kNumFrames, kBytesPerSample);
  CompareAudioData(input->data(), *actual);
}

TEST_F(StreamMixerAlsaTest, OneStreamIsScaledDownProperly) {
  auto input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  input->SetPaused(false);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(base::WrapUnique(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // Populate the stream with data.
  const int kNumFrames = 32;
  ASSERT_EQ(sizeof(kTestData[0]), kNumChannels * kNumFrames * kBytesPerSample);
  auto data = GetTestData(0);
  input->SetData(std::move(data));

  // Set a volume multiplier on the stream.
  input->SetVolumeMultiplier(0.75);

  // Write the stream to ALSA.
  EXPECT_CALL(*input, GetResampledData(_, kNumFrames));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  mixer->WriteFramesForTest();

  // Check that the retrieved stream is scaled correctly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  actual->FromInterleaved(
      &(mock_alsa()->data()[0]), kNumFrames, kBytesPerSample);
  auto expected = GetMixedAudioData(input);
  CompareAudioData(*expected, *actual);
}

TEST_F(StreamMixerAlsaTest, TwoUnscaledStreamsMixProperly) {
  // Create a group of input streams.
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  const int kNumInputs = 2;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(
        new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond));
    inputs.back()->SetPaused(false);
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  // Poll the inputs for data.
  const int kNumFrames = 32;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
    EXPECT_CALL(*inputs[i], GetResampledData(_, kNumFrames));
    EXPECT_CALL(*inputs[i], AfterWriteFrames(_));
  }

  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kNumFrames)).Times(1);
  mixer->WriteFramesForTest();

  // Mix the inputs manually.
  auto expected = GetMixedAudioData(inputs);

  // Get the actual stream rendered to ALSA, and compare it against the
  // expected stream. The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  actual->FromInterleaved(
      &(mock_alsa()->data()[0]), kNumFrames, kBytesPerSample);
  CompareAudioData(*expected, *actual);
}

TEST_F(StreamMixerAlsaTest, TwoUnscaledStreamsMixProperlyWithEdgeCases) {
  // Create a group of input streams.
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  const int kNumInputs = 2;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(
        new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond));
    inputs.back()->SetPaused(false);
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  // Create edge case data for the inputs. By mixing these two short streams,
  // every combination of {-(2^31), 0, 2^31-1} is tested. This test case is
  // intended to be a hand-checkable gut check.
  // Note: Test data should be represented as 32-bit integers and copied into
  // ::media::AudioBus instances, rather than wrapping statically declared float
  // arrays. The latter method is brittle, as ::media::AudioBus requires 16-bit
  // alignment for internal data.
  const int kNumFrames = 3;

  const int32_t kMaxSample = std::numeric_limits<int32_t>::max();
  const int32_t kMinSample = std::numeric_limits<int32_t>::min();
  const int32_t kEdgeData[2][8] = {
    {
      kMinSample, kMinSample,
      kMinSample, 0.0,
      0.0,        kMaxSample,
      0.0,        0.0,
    }, {
      kMinSample, 0.0,
      kMaxSample, 0.0,
      kMaxSample, kMaxSample,
      0.0,        0.0,
    }
  };

  // Hand-calculate the results. Index 0 is clamped to -(2^31). Index 5 is
  // clamped to 2^31-1.
  const int32_t kResult[8] = {
    kMinSample,  kMinSample,
    0.0,         0.0,
    kMaxSample,  kMaxSample,
    0.0,         0.0,
  };

  for (size_t i = 0; i < inputs.size(); ++i) {
    auto test_data = ::media::AudioBus::Create(kNumChannels, kNumFrames);
    test_data->FromInterleaved(kEdgeData[i], kNumFrames, kBytesPerSample);
    inputs[i]->SetData(std::move(test_data));
    EXPECT_CALL(*inputs[i], GetResampledData(_, kNumFrames));
    EXPECT_CALL(*inputs[i], AfterWriteFrames(_));
  }

  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kNumFrames)).Times(1);
  mixer->WriteFramesForTest();

  // Use the hand-calculated results above.
  auto expected = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  expected->FromInterleaved(kResult, kNumFrames, kBytesPerSample);

  // Get the actual stream rendered to ALSA, and compare it against the
  // expected stream. The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  actual->FromInterleaved(
      &(mock_alsa()->data()[0]), kNumFrames, kBytesPerSample);
  CompareAudioData(*expected, *actual);
}

TEST_F(StreamMixerAlsaTest, WriteBuffersOfVaryingLength) {
  auto input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  input->SetPaused(false);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(base::WrapUnique(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // The input stream will provide buffers of several different lengths.
  input->SetMaxReadSize(7);
  EXPECT_CALL(*input, GetResampledData(_, 7));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 7)).Times(1);
  mixer->WriteFramesForTest();

  input->SetMaxReadSize(100);
  EXPECT_CALL(*input, GetResampledData(_, 100));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 100)).Times(1);
  mixer->WriteFramesForTest();

  input->SetMaxReadSize(32);
  EXPECT_CALL(*input, GetResampledData(_, 32));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 32)).Times(1);
  mixer->WriteFramesForTest();

  input->SetMaxReadSize(1024);
  EXPECT_CALL(*input, GetResampledData(_, 1024));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 1024)).Times(1);
  mixer->WriteFramesForTest();
}

}  // namespace media
}  // namespace chromecast
