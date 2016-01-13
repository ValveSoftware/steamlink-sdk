// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <sstream>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/audio_bus.h"
#include "media/base/media.h"
#include "media/cast/audio_sender/audio_encoder.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/utility/audio_utility.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = INT64_C(12345678900000);

namespace {

class TestEncodedAudioFrameReceiver {
 public:
  explicit TestEncodedAudioFrameReceiver(transport::AudioCodec codec)
      : codec_(codec), frames_received_(0), rtp_lower_bound_(0) {}
  virtual ~TestEncodedAudioFrameReceiver() {}

  int frames_received() const { return frames_received_; }

  void SetCaptureTimeBounds(const base::TimeTicks& lower_bound,
                            const base::TimeTicks& upper_bound) {
    lower_bound_ = lower_bound;
    upper_bound_ = upper_bound;
  }

  void FrameEncoded(scoped_ptr<transport::EncodedFrame> encoded_frame) {
    EXPECT_EQ(encoded_frame->dependency, transport::EncodedFrame::KEY);
    EXPECT_EQ(static_cast<uint8>(frames_received_ & 0xff),
              encoded_frame->frame_id);
    EXPECT_EQ(encoded_frame->frame_id, encoded_frame->referenced_frame_id);
    // RTP timestamps should be monotonically increasing and integer multiples
    // of the fixed frame size.
    EXPECT_LE(rtp_lower_bound_, encoded_frame->rtp_timestamp);
    rtp_lower_bound_ = encoded_frame->rtp_timestamp;
    // Note: In audio_encoder.cc, 100 is the fixed audio frame rate.
    const int kSamplesPerFrame = kDefaultAudioSamplingRate / 100;
    EXPECT_EQ(0u, encoded_frame->rtp_timestamp % kSamplesPerFrame);
    EXPECT_TRUE(!encoded_frame->data.empty());

    EXPECT_LE(lower_bound_, encoded_frame->reference_time);
    lower_bound_ = encoded_frame->reference_time;
    EXPECT_GT(upper_bound_, encoded_frame->reference_time);

    ++frames_received_;
  }

 private:
  const transport::AudioCodec codec_;
  int frames_received_;
  uint32 rtp_lower_bound_;
  base::TimeTicks lower_bound_;
  base::TimeTicks upper_bound_;

  DISALLOW_COPY_AND_ASSIGN(TestEncodedAudioFrameReceiver);
};

struct TestScenario {
  const int64* durations_in_ms;
  size_t num_durations;

  TestScenario(const int64* d, size_t n)
      : durations_in_ms(d), num_durations(n) {}

  std::string ToString() const {
    std::ostringstream out;
    for (size_t i = 0; i < num_durations; ++i) {
      if (i > 0)
        out << ", ";
      out << durations_in_ms[i];
    }
    return out.str();
  }
};

}  // namespace

class AudioEncoderTest : public ::testing::TestWithParam<TestScenario> {
 public:
  AudioEncoderTest() {
    InitializeMediaLibraryForTesting();
    testing_clock_ = new base::SimpleTestTickClock();
    testing_clock_->Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  virtual void SetUp() {
    task_runner_ = new test::FakeSingleThreadTaskRunner(testing_clock_);
    cast_environment_ =
        new CastEnvironment(scoped_ptr<base::TickClock>(testing_clock_).Pass(),
                            task_runner_,
                            task_runner_,
                            task_runner_);
  }

  virtual ~AudioEncoderTest() {}

  void RunTestForCodec(transport::AudioCodec codec) {
    const TestScenario& scenario = GetParam();
    SCOPED_TRACE(::testing::Message() << "Durations: " << scenario.ToString());

    CreateObjectsForCodec(codec);

    // Note: In audio_encoder.cc, 10 ms is the fixed frame duration.
    const base::TimeDelta frame_duration =
        base::TimeDelta::FromMilliseconds(10);

    for (size_t i = 0; i < scenario.num_durations; ++i) {
      const bool simulate_missing_data = scenario.durations_in_ms[i] < 0;
      const base::TimeDelta duration = base::TimeDelta::FromMilliseconds(
          std::abs(scenario.durations_in_ms[i]));
      receiver_->SetCaptureTimeBounds(
          testing_clock_->NowTicks() - frame_duration,
          testing_clock_->NowTicks() + duration);
      if (simulate_missing_data) {
        task_runner_->RunTasks();
        testing_clock_->Advance(duration);
      } else {
        audio_encoder_->InsertAudio(audio_bus_factory_->NextAudioBus(duration),
                                    testing_clock_->NowTicks());
        task_runner_->RunTasks();
        testing_clock_->Advance(duration);
      }
    }

    DVLOG(1) << "Received " << receiver_->frames_received()
             << " frames for this test run: " << scenario.ToString();
  }

 private:
  void CreateObjectsForCodec(transport::AudioCodec codec) {
    AudioSenderConfig audio_config;
    audio_config.codec = codec;
    audio_config.use_external_encoder = false;
    audio_config.frequency = kDefaultAudioSamplingRate;
    audio_config.channels = 2;
    audio_config.bitrate = kDefaultAudioEncoderBitrate;
    audio_config.rtp_config.payload_type = 127;

    audio_bus_factory_.reset(
        new TestAudioBusFactory(audio_config.channels,
                                audio_config.frequency,
                                TestAudioBusFactory::kMiddleANoteFreq,
                                0.5f));

    receiver_.reset(new TestEncodedAudioFrameReceiver(codec));

    audio_encoder_.reset(new AudioEncoder(
        cast_environment_,
        audio_config,
        base::Bind(&TestEncodedAudioFrameReceiver::FrameEncoded,
                   base::Unretained(receiver_.get()))));
  }

  base::SimpleTestTickClock* testing_clock_;  // Owned by CastEnvironment.
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<TestAudioBusFactory> audio_bus_factory_;
  scoped_ptr<TestEncodedAudioFrameReceiver> receiver_;
  scoped_ptr<AudioEncoder> audio_encoder_;
  scoped_refptr<CastEnvironment> cast_environment_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoderTest);
};

TEST_P(AudioEncoderTest, EncodeOpus) { RunTestForCodec(transport::kOpus); }

TEST_P(AudioEncoderTest, EncodePcm16) { RunTestForCodec(transport::kPcm16); }

static const int64 kOneCall_3Millis[] = {3};
static const int64 kOneCall_10Millis[] = {10};
static const int64 kOneCall_13Millis[] = {13};
static const int64 kOneCall_20Millis[] = {20};

static const int64 kTwoCalls_3Millis[] = {3, 3};
static const int64 kTwoCalls_10Millis[] = {10, 10};
static const int64 kTwoCalls_Mixed1[] = {3, 10};
static const int64 kTwoCalls_Mixed2[] = {10, 3};
static const int64 kTwoCalls_Mixed3[] = {3, 17};
static const int64 kTwoCalls_Mixed4[] = {17, 3};

static const int64 kManyCalls_3Millis[] = {3, 3, 3, 3, 3, 3, 3, 3,
                                           3, 3, 3, 3, 3, 3, 3};
static const int64 kManyCalls_10Millis[] = {10, 10, 10, 10, 10, 10, 10, 10,
                                            10, 10, 10, 10, 10, 10, 10};
static const int64 kManyCalls_Mixed1[] = {3,  10, 3,  10, 3,  10, 3,  10, 3,
                                          10, 3,  10, 3,  10, 3,  10, 3,  10};
static const int64 kManyCalls_Mixed2[] = {10, 3, 10, 3, 10, 3, 10, 3, 10, 3,
                                          10, 3, 10, 3, 10, 3, 10, 3, 10, 3};
static const int64 kManyCalls_Mixed3[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8,
                                          9, 7, 9, 3, 2, 3, 8, 4, 6, 2, 6, 4};
static const int64 kManyCalls_Mixed4[] = {31, 4, 15, 9,  26, 53, 5,  8, 9,
                                          7,  9, 32, 38, 4,  62, 64, 3};
static const int64 kManyCalls_Mixed5[] = {3, 14, 15, 9, 26, 53, 58, 9, 7,
                                          9, 3,  23, 8, 4,  6,  2,  6, 43};

static const int64 kOneBigUnderrun[] = {10, 10, 10, 10, -1000, 10, 10, 10};
static const int64 kTwoBigUnderruns[] = {10, 10, 10, 10, -712, 10, 10, 10,
                                         -1311, 10, 10, 10};
static const int64 kMixedUnderruns[] = {31, -64, 4, 15, 9, 26, -53, 5,  8, -9,
                                        7,  9, 32, 38, -4, 62, -64, 3};

INSTANTIATE_TEST_CASE_P(
    AudioEncoderTestScenarios,
    AudioEncoderTest,
    ::testing::Values(
        TestScenario(kOneCall_3Millis, arraysize(kOneCall_3Millis)),
        TestScenario(kOneCall_10Millis, arraysize(kOneCall_10Millis)),
        TestScenario(kOneCall_13Millis, arraysize(kOneCall_13Millis)),
        TestScenario(kOneCall_20Millis, arraysize(kOneCall_20Millis)),
        TestScenario(kTwoCalls_3Millis, arraysize(kTwoCalls_3Millis)),
        TestScenario(kTwoCalls_10Millis, arraysize(kTwoCalls_10Millis)),
        TestScenario(kTwoCalls_Mixed1, arraysize(kTwoCalls_Mixed1)),
        TestScenario(kTwoCalls_Mixed2, arraysize(kTwoCalls_Mixed2)),
        TestScenario(kTwoCalls_Mixed3, arraysize(kTwoCalls_Mixed3)),
        TestScenario(kTwoCalls_Mixed4, arraysize(kTwoCalls_Mixed4)),
        TestScenario(kManyCalls_3Millis, arraysize(kManyCalls_3Millis)),
        TestScenario(kManyCalls_10Millis, arraysize(kManyCalls_10Millis)),
        TestScenario(kManyCalls_Mixed1, arraysize(kManyCalls_Mixed1)),
        TestScenario(kManyCalls_Mixed2, arraysize(kManyCalls_Mixed2)),
        TestScenario(kManyCalls_Mixed3, arraysize(kManyCalls_Mixed3)),
        TestScenario(kManyCalls_Mixed4, arraysize(kManyCalls_Mixed4)),
        TestScenario(kManyCalls_Mixed5, arraysize(kManyCalls_Mixed5)),
        TestScenario(kOneBigUnderrun, arraysize(kOneBigUnderrun)),
        TestScenario(kTwoBigUnderruns, arraysize(kTwoBigUnderruns)),
        TestScenario(kMixedUnderruns, arraysize(kMixedUnderruns))));

}  // namespace cast
}  // namespace media
