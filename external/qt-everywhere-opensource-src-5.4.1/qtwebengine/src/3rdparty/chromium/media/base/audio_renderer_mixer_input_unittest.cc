// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kBitsPerChannel = 16;
static const int kSampleRate = 48000;
static const int kBufferSize = 8192;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;

class AudioRendererMixerInputTest : public testing::Test {
 public:
  AudioRendererMixerInputTest() {
    audio_parameters_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate,
        kBitsPerChannel, kBufferSize);

    CreateMixerInput();
    fake_callback_.reset(new FakeAudioRenderCallback(0));
    mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
    audio_bus_ = AudioBus::Create(audio_parameters_);
  }

  void CreateMixerInput() {
    mixer_input_ = new AudioRendererMixerInput(
        base::Bind(
            &AudioRendererMixerInputTest::GetMixer, base::Unretained(this)),
        base::Bind(
            &AudioRendererMixerInputTest::RemoveMixer, base::Unretained(this)));
  }

  AudioRendererMixer* GetMixer(const AudioParameters& params) {
    if (!mixer_) {
      scoped_refptr<MockAudioRendererSink> sink = new MockAudioRendererSink();
      EXPECT_CALL(*sink.get(), Start());
      EXPECT_CALL(*sink.get(), Stop());

      mixer_.reset(new AudioRendererMixer(
          audio_parameters_, audio_parameters_, sink));
    }
    EXPECT_CALL(*this, RemoveMixer(testing::_));
    return mixer_.get();
  }

  double ProvideInput() {
    return mixer_input_->ProvideInput(audio_bus_.get(), base::TimeDelta());
  }

  MOCK_METHOD1(RemoveMixer, void(const AudioParameters&));

 protected:
  virtual ~AudioRendererMixerInputTest() {}

  AudioParameters audio_parameters_;
  scoped_ptr<AudioRendererMixer> mixer_;
  scoped_refptr<AudioRendererMixerInput> mixer_input_;
  scoped_ptr<FakeAudioRenderCallback> fake_callback_;
  scoped_ptr<AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerInputTest);
};

// Test that getting and setting the volume work as expected.  The volume is
// returned from ProvideInput() only when playing.
TEST_F(AudioRendererMixerInputTest, GetSetVolume) {
  mixer_input_->Start();
  mixer_input_->Play();

  // Starting volume should be 1.0.
  EXPECT_DOUBLE_EQ(ProvideInput(), 1);

  const double kVolume = 0.5;
  EXPECT_TRUE(mixer_input_->SetVolume(kVolume));
  EXPECT_DOUBLE_EQ(ProvideInput(), kVolume);

  mixer_input_->Stop();
}

// Test Start()/Play()/Pause()/Stop()/playing() all work as expected.  Also
// implicitly tests that AddMixerInput() and RemoveMixerInput() work without
// crashing; functional tests for these methods are in AudioRendererMixerTest.
TEST_F(AudioRendererMixerInputTest, StartPlayPauseStopPlaying) {
  mixer_input_->Start();
  mixer_input_->Play();
  EXPECT_DOUBLE_EQ(ProvideInput(), 1);
  mixer_input_->Pause();
  mixer_input_->Play();
  EXPECT_DOUBLE_EQ(ProvideInput(), 1);
  mixer_input_->Stop();
}

// Test that Stop() can be called before Initialize() and Start().
TEST_F(AudioRendererMixerInputTest, StopBeforeInitializeOrStart) {
  // |mixer_input_| was initialized during construction.
  mixer_input_->Stop();

  // Verify Stop() works without Initialize() or Start().
  CreateMixerInput();
  mixer_input_->Stop();
}

// Test that Start() can be called after Stop().
// TODO(dalecurtis): We shouldn't allow this.  See http://crbug.com/151051
TEST_F(AudioRendererMixerInputTest, StartAfterStop) {
  mixer_input_->Stop();
  mixer_input_->Start();
  mixer_input_->Stop();
}

}  // namespace media
