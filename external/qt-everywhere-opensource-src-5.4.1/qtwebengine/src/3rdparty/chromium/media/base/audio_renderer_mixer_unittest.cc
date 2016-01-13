// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES
#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Parameters which control the many input case tests.
const int kMixerInputs = 8;
const int kMixerCycles = 3;

// Parameters used for testing.
const int kBitsPerChannel = 32;
const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
const int kHighLatencyBufferSize = 8192;
const int kLowLatencyBufferSize = 256;

// Number of full sine wave cycles for each Render() call.
const int kSineCycles = 4;

// Tuple of <input sampling rate, output sampling rate, epsilon>.
typedef std::tr1::tuple<int, int, double> AudioRendererMixerTestData;
class AudioRendererMixerTest
    : public testing::TestWithParam<AudioRendererMixerTestData> {
 public:
  AudioRendererMixerTest()
      : epsilon_(std::tr1::get<2>(GetParam())),
        half_fill_(false) {
    // Create input and output parameters based on test parameters.
    input_parameters_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout,
        std::tr1::get<0>(GetParam()), kBitsPerChannel, kHighLatencyBufferSize);
    output_parameters_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LOW_LATENCY, kChannelLayout,
        std::tr1::get<1>(GetParam()), 16, kLowLatencyBufferSize);

    sink_ = new MockAudioRendererSink();
    EXPECT_CALL(*sink_, Start());
    EXPECT_CALL(*sink_, Stop());

    mixer_.reset(new AudioRendererMixer(
        input_parameters_, output_parameters_, sink_));
    mixer_callback_ = sink_->callback();

    audio_bus_ = AudioBus::Create(output_parameters_);
    expected_audio_bus_ = AudioBus::Create(output_parameters_);

    // Allocate one callback for generating expected results.
    double step = kSineCycles / static_cast<double>(
        output_parameters_.frames_per_buffer());
    expected_callback_.reset(new FakeAudioRenderCallback(step));
  }

  AudioRendererMixer* GetMixer(const AudioParameters& params) {
    return mixer_.get();
  }

  MOCK_METHOD1(RemoveMixer, void(const AudioParameters&));

  void InitializeInputs(int count) {
    mixer_inputs_.reserve(count);
    fake_callbacks_.reserve(count);

    // Setup FakeAudioRenderCallback step to compensate for resampling.
    double scale_factor = input_parameters_.sample_rate() /
        static_cast<double>(output_parameters_.sample_rate());
    double step = kSineCycles / (scale_factor *
        static_cast<double>(output_parameters_.frames_per_buffer()));

    for (int i = 0; i < count; ++i) {
      fake_callbacks_.push_back(new FakeAudioRenderCallback(step));
      mixer_inputs_.push_back(new AudioRendererMixerInput(
          base::Bind(&AudioRendererMixerTest::GetMixer,
                     base::Unretained(this)),
          base::Bind(&AudioRendererMixerTest::RemoveMixer,
                     base::Unretained(this))));
      mixer_inputs_[i]->Initialize(input_parameters_, fake_callbacks_[i]);
      mixer_inputs_[i]->SetVolume(1.0f);
    }
    EXPECT_CALL(*this, RemoveMixer(testing::_)).Times(count);
  }

  bool ValidateAudioData(int index, int frames, float scale, double epsilon) {
    for (int i = 0; i < audio_bus_->channels(); ++i) {
      for (int j = index; j < frames; j++) {
        double error = fabs(audio_bus_->channel(i)[j] -
            expected_audio_bus_->channel(i)[j] * scale);
        if (error > epsilon) {
          EXPECT_NEAR(expected_audio_bus_->channel(i)[j] * scale,
                      audio_bus_->channel(i)[j], epsilon)
              << " i=" << i << ", j=" << j;
          return false;
        }
      }
    }
    return true;
  }

  bool ValidateAudioData(int index, int frames, float scale) {
    return ValidateAudioData(index, frames, scale, epsilon_);
  }

  bool RenderAndValidateAudioData(float scale) {
    if (half_fill_) {
      for (size_t i = 0; i < fake_callbacks_.size(); ++i)
        fake_callbacks_[i]->set_half_fill(true);
      expected_callback_->set_half_fill(true);
      // Initialize the AudioBus completely or we'll run into Valgrind problems
      // during the verification step below.
      expected_audio_bus_->Zero();
    }

    // Render actual audio data.
    int frames = mixer_callback_->Render(audio_bus_.get(), 0);
    if (frames != audio_bus_->frames())
      return false;

    // Render expected audio data (without scaling).
    expected_callback_->Render(expected_audio_bus_.get(), 0);

    if (half_fill_) {
      // In this case, just verify that every frame was initialized, this will
      // only fail under tooling such as valgrind.
      return ValidateAudioData(
          0, frames, 0, std::numeric_limits<double>::max());
    } else {
      return ValidateAudioData(0, frames, scale);
    }
  }

  // Fill |audio_bus_| fully with |value|.
  void FillAudioData(float value) {
    for (int i = 0; i < audio_bus_->channels(); ++i) {
      std::fill(audio_bus_->channel(i),
                audio_bus_->channel(i) + audio_bus_->frames(), value);
    }
  }

  // Verify silence when mixer inputs are in pre-Start() and post-Start().
  void StartTest(int inputs) {
    InitializeInputs(inputs);

    // Verify silence before any inputs have been started.  Fill the buffer
    // before hand with non-zero data to ensure we get zeros back.
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));

    // Start() all even numbered mixer inputs and ensure we still get silence.
    for (size_t i = 0; i < mixer_inputs_.size(); i += 2)
      mixer_inputs_[i]->Start();
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));

    // Start() all mixer inputs and ensure we still get silence.
    for (size_t i = 1; i < mixer_inputs_.size(); i += 2)
      mixer_inputs_[i]->Start();
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Stop();
  }

  // Verify output when mixer inputs are in post-Play() state.
  void PlayTest(int inputs) {
    InitializeInputs(inputs);

    // Play() all mixer inputs and ensure we get the right values.
    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    for (int i = 0; i < kMixerCycles; ++i)
      ASSERT_TRUE(RenderAndValidateAudioData(mixer_inputs_.size()));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Stop();
  }

  // Verify volume adjusted output when mixer inputs are in post-Play() state.
  void PlayVolumeAdjustedTest(int inputs) {
    InitializeInputs(inputs);

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    // Set a different volume for each mixer input and verify the results.
    float total_scale = 0;
    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      float volume = static_cast<float>(i) / mixer_inputs_.size();
      total_scale += volume;
      EXPECT_TRUE(mixer_inputs_[i]->SetVolume(volume));
    }
    for (int i = 0; i < kMixerCycles; ++i)
      ASSERT_TRUE(RenderAndValidateAudioData(total_scale));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Stop();
  }

  // Verify output when mixer inputs can only partially fulfill a Render().
  void PlayPartialRenderTest(int inputs) {
    InitializeInputs(inputs);

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    // Verify a properly filled buffer when half filled (remainder zeroed).
    half_fill_ = true;
    ASSERT_TRUE(RenderAndValidateAudioData(mixer_inputs_.size()));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Stop();
  }

  // Verify output when mixer inputs are in Pause() state.
  void PauseTest(int inputs) {
    InitializeInputs(inputs);

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    // Pause() all even numbered mixer inputs and ensure we get the right value.
    for (size_t i = 0; i < mixer_inputs_.size(); i += 2)
      mixer_inputs_[i]->Pause();
    for (int i = 0; i < kMixerCycles; ++i)
      ASSERT_TRUE(RenderAndValidateAudioData(mixer_inputs_.size() / 2));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Stop();
  }

  // Verify output when mixer inputs are in post-Stop() state.
  void StopTest(int inputs) {
    InitializeInputs(inputs);

    // Start() and Stop() all inputs.
    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Stop();
    }

    // Verify we get silence back; fill |audio_bus_| before hand to be sure.
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));
  }

 protected:
  virtual ~AudioRendererMixerTest() {}

  scoped_refptr<MockAudioRendererSink> sink_;
  scoped_ptr<AudioRendererMixer> mixer_;
  AudioRendererSink::RenderCallback* mixer_callback_;
  AudioParameters input_parameters_;
  AudioParameters output_parameters_;
  scoped_ptr<AudioBus> audio_bus_;
  scoped_ptr<AudioBus> expected_audio_bus_;
  std::vector< scoped_refptr<AudioRendererMixerInput> > mixer_inputs_;
  ScopedVector<FakeAudioRenderCallback> fake_callbacks_;
  scoped_ptr<FakeAudioRenderCallback> expected_callback_;
  double epsilon_;
  bool half_fill_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerTest);
};

class AudioRendererMixerBehavioralTest : public AudioRendererMixerTest {};

ACTION_P(SignalEvent, event) {
  event->Signal();
}

// Verify a mixer with no inputs returns silence for all requested frames.
TEST_P(AudioRendererMixerTest, NoInputs) {
  FillAudioData(1.0f);
  EXPECT_TRUE(RenderAndValidateAudioData(0.0f));
}

// Test mixer output with one input in the pre-Start() and post-Start() state.
TEST_P(AudioRendererMixerTest, OneInputStart) {
  StartTest(1);
}

// Test mixer output with many inputs in the pre-Start() and post-Start() state.
TEST_P(AudioRendererMixerTest, ManyInputStart) {
  StartTest(kMixerInputs);
}

// Test mixer output with one input in the post-Play() state.
TEST_P(AudioRendererMixerTest, OneInputPlay) {
  PlayTest(1);
}

// Test mixer output with many inputs in the post-Play() state.
TEST_P(AudioRendererMixerTest, ManyInputPlay) {
  PlayTest(kMixerInputs);
}

// Test volume adjusted mixer output with one input in the post-Play() state.
TEST_P(AudioRendererMixerTest, OneInputPlayVolumeAdjusted) {
  PlayVolumeAdjustedTest(1);
}

// Test volume adjusted mixer output with many inputs in the post-Play() state.
TEST_P(AudioRendererMixerTest, ManyInputPlayVolumeAdjusted) {
  PlayVolumeAdjustedTest(kMixerInputs);
}

// Test mixer output with one input and partial Render() in post-Play() state.
TEST_P(AudioRendererMixerTest, OneInputPlayPartialRender) {
  PlayPartialRenderTest(1);
}

// Test mixer output with many inputs and partial Render() in post-Play() state.
TEST_P(AudioRendererMixerTest, ManyInputPlayPartialRender) {
  PlayPartialRenderTest(kMixerInputs);
}

// Test mixer output with one input in the post-Pause() state.
TEST_P(AudioRendererMixerTest, OneInputPause) {
  PauseTest(1);
}

// Test mixer output with many inputs in the post-Pause() state.
TEST_P(AudioRendererMixerTest, ManyInputPause) {
  PauseTest(kMixerInputs);
}

// Test mixer output with one input in the post-Stop() state.
TEST_P(AudioRendererMixerTest, OneInputStop) {
  StopTest(1);
}

// Test mixer output with many inputs in the post-Stop() state.
TEST_P(AudioRendererMixerTest, ManyInputStop) {
  StopTest(kMixerInputs);
}

// Test mixer with many inputs in mixed post-Stop() and post-Play() states.
TEST_P(AudioRendererMixerTest, ManyInputMixedStopPlay) {
  InitializeInputs(kMixerInputs);

  // Start() all inputs.
  for (size_t i = 0; i < mixer_inputs_.size(); ++i)
    mixer_inputs_[i]->Start();

  // Stop() all even numbered mixer inputs and Play() all odd numbered inputs
  // and ensure we get the right value.
  for (size_t i = 1; i < mixer_inputs_.size(); i += 2) {
    mixer_inputs_[i - 1]->Stop();
    mixer_inputs_[i]->Play();
  }
  ASSERT_TRUE(RenderAndValidateAudioData(std::max(
      mixer_inputs_.size() / 2, static_cast<size_t>(1))));

  for (size_t i = 1; i < mixer_inputs_.size(); i += 2)
    mixer_inputs_[i]->Stop();
}

TEST_P(AudioRendererMixerBehavioralTest, OnRenderError) {
  InitializeInputs(kMixerInputs);
  for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
    mixer_inputs_[i]->Start();
    mixer_inputs_[i]->Play();
    EXPECT_CALL(*fake_callbacks_[i], OnRenderError()).Times(1);
  }

  mixer_callback_->OnRenderError();
  for (size_t i = 0; i < mixer_inputs_.size(); ++i)
    mixer_inputs_[i]->Stop();
}

TEST_P(AudioRendererMixerBehavioralTest, OnRenderErrorPausedInput) {
  InitializeInputs(kMixerInputs);

  for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
    mixer_inputs_[i]->Start();
    EXPECT_CALL(*fake_callbacks_[i], OnRenderError()).Times(1);
  }

  // Fire the error before attaching any inputs.  Ensure an error is recieved
  // even if the input is not connected.
  mixer_callback_->OnRenderError();

  for (size_t i = 0; i < mixer_inputs_.size(); ++i)
    mixer_inputs_[i]->Stop();
}

// Ensure constructing an AudioRendererMixerInput, but not initializing it does
// not call RemoveMixer().
TEST_P(AudioRendererMixerBehavioralTest, NoInitialize) {
  EXPECT_CALL(*this, RemoveMixer(testing::_)).Times(0);
  scoped_refptr<AudioRendererMixerInput> audio_renderer_mixer =
      new AudioRendererMixerInput(
          base::Bind(&AudioRendererMixerTest::GetMixer,
                     base::Unretained(this)),
          base::Bind(&AudioRendererMixerTest::RemoveMixer,
                     base::Unretained(this)));
}

// Ensure the physical stream is paused after a certain amount of time with no
// inputs playing.  The test will hang if the behavior is incorrect.
TEST_P(AudioRendererMixerBehavioralTest, MixerPausesStream) {
  const base::TimeDelta kPauseTime = base::TimeDelta::FromMilliseconds(500);
  // This value can't be too low or valgrind, tsan will timeout on the bots.
  const base::TimeDelta kTestTimeout = 10 * kPauseTime;
  mixer_->set_pause_delay_for_testing(kPauseTime);

  base::WaitableEvent pause_event(true, false);
  EXPECT_CALL(*sink_.get(), Pause()).Times(2)
      .WillRepeatedly(SignalEvent(&pause_event));
  InitializeInputs(1);

  // Ensure never playing the input results in a sink pause.
  const base::TimeDelta kSleepTime = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks start_time = base::TimeTicks::Now();
  while (!pause_event.IsSignaled()) {
    mixer_callback_->Render(audio_bus_.get(), 0);
    base::PlatformThread::Sleep(kSleepTime);
    ASSERT_TRUE(base::TimeTicks::Now() - start_time < kTestTimeout);
  }
  pause_event.Reset();

  // Playing the input for the first time should cause a sink play.
  mixer_inputs_[0]->Start();
  EXPECT_CALL(*sink_.get(), Play());
  mixer_inputs_[0]->Play();
  mixer_inputs_[0]->Pause();

  // Ensure once the input is paused the sink eventually pauses.
  start_time = base::TimeTicks::Now();
  while (!pause_event.IsSignaled()) {
    mixer_callback_->Render(audio_bus_.get(), 0);
    base::PlatformThread::Sleep(kSleepTime);
    ASSERT_TRUE(base::TimeTicks::Now() - start_time < kTestTimeout);
  }

  mixer_inputs_[0]->Stop();
}

INSTANTIATE_TEST_CASE_P(
    AudioRendererMixerTest, AudioRendererMixerTest, testing::Values(
        // No resampling.
        std::tr1::make_tuple(44100, 44100, 0.00000048),

        // Upsampling.
        std::tr1::make_tuple(44100, 48000, 0.033),

        // Downsampling.
        std::tr1::make_tuple(48000, 41000, 0.042)));

// Test cases for behavior which is independent of parameters.  Values() doesn't
// support single item lists and we don't want these test cases to run for every
// parameter set.
INSTANTIATE_TEST_CASE_P(
    AudioRendererMixerBehavioralTest, AudioRendererMixerBehavioralTest,
    testing::ValuesIn(std::vector<AudioRendererMixerTestData>(
        1, std::tr1::make_tuple(44100, 44100, 0))));

}  // namespace media
