// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audio_modem/audio_player.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/audio_modem/audio_player_impl.h"
#include "components/audio_modem/public/audio_modem_types.h"
#include "components/audio_modem/test/random_samples.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestAudioOutputStream : public media::AudioOutputStream {
 public:
  using GatherSamplesCallback =
      base::Callback<void(std::unique_ptr<media::AudioBus>, int frames)>;
  TestAudioOutputStream(int default_frame_count,
                        int max_frame_count,
                        GatherSamplesCallback gather_callback)
      : default_frame_count_(default_frame_count),
        max_frame_count_(max_frame_count),
        gather_callback_(gather_callback),
        callback_(nullptr) {
    caller_loop_ = base::MessageLoop::current();
  }

  ~TestAudioOutputStream() override {}

  bool Open() override { return true; }
  void Start(AudioSourceCallback* callback) override {
    callback_ = callback;
    GatherPlayedSamples();
  }
  void Stop() override {}
  void SetVolume(double volume) override {}
  void GetVolume(double* volume) override {}
  void Close() override {}

 private:
  void GatherPlayedSamples() {
    int frames = 0, total_frames = 0;
    do {
      // Call back into the player to get samples that it wants us to play.
      std::unique_ptr<media::AudioBus> dest =
          media::AudioBus::Create(1, default_frame_count_);
      frames = callback_->OnMoreData(dest.get(), 0, 0);
      total_frames += frames;
      // Send the samples given to us by the player to the gather callback.
      caller_loop_->task_runner()->PostTask(
          FROM_HERE, base::Bind(gather_callback_, base::Passed(&dest), frames));
    } while (frames && total_frames < max_frame_count_);
  }

  int default_frame_count_;
  int max_frame_count_;
  GatherSamplesCallback gather_callback_;
  AudioSourceCallback* callback_;
  base::MessageLoop* caller_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioOutputStream);
};

}  // namespace

namespace audio_modem {

class AudioPlayerTest : public testing::Test,
                        public base::SupportsWeakPtr<AudioPlayerTest> {
 public:
  AudioPlayerTest() : buffer_index_(0), player_(nullptr) {
    audio_manager_ = media::AudioManager::CreateForTesting(
        base::ThreadTaskRunnerHandle::Get());
    base::RunLoop().RunUntilIdle();
  }

  ~AudioPlayerTest() override { DeletePlayer(); }

  void CreatePlayer() {
    DeletePlayer();
    player_ = new AudioPlayerImpl();
    player_->set_output_stream_for_testing(new TestAudioOutputStream(
        kDefaultFrameCount,
        kMaxFrameCount,
        base::Bind(&AudioPlayerTest::GatherSamples, AsWeakPtr())));
    player_->Initialize();
    base::RunLoop().RunUntilIdle();
  }

  void DeletePlayer() {
    if (!player_)
      return;
    player_->Finalize();
    player_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  void PlayAndVerifySamples(
      const scoped_refptr<media::AudioBusRefCounted>& samples) {
    DCHECK_LT(samples->frames(), kMaxFrameCount);

    buffer_ = media::AudioBus::Create(1, kMaxFrameCount);
    buffer_index_ = 0;
    player_->Play(samples);
    player_->Stop();
    base::RunLoop().RunUntilIdle();

    int differences = 0;
    for (int i = 0; i < kMaxFrameCount; ++i) {
      differences += (buffer_->channel(0)[i] !=
                      samples->channel(0)[i % samples->frames()]);
    }
    ASSERT_EQ(0, differences);

    buffer_.reset();
  }

  void GatherSamples(std::unique_ptr<media::AudioBus> bus, int frames) {
    if (!buffer_.get())
      return;
    bus->CopyPartialFramesTo(0, frames, buffer_index_, buffer_.get());
    buffer_index_ += frames;
  }

 protected:
  bool IsPlaying() {
    base::RunLoop().RunUntilIdle();
    return player_->is_playing_;
  }

  static const int kDefaultFrameCount = 1024;
  static const int kMaxFrameCount = 1024 * 100;

  base::TestMessageLoop message_loop_;
  media::ScopedAudioManagerPtr audio_manager_;
  std::unique_ptr<media::AudioBus> buffer_;
  int buffer_index_;

  // Deleted by calling Finalize() on the object.
  AudioPlayerImpl* player_;
};

TEST_F(AudioPlayerTest, BasicPlayAndStop) {
  CreatePlayer();
  scoped_refptr<media::AudioBusRefCounted> samples =
      media::AudioBusRefCounted::Create(1, 7331);

  player_->Play(samples);
  EXPECT_TRUE(IsPlaying());

  player_->Stop();
  EXPECT_FALSE(IsPlaying());

  player_->Play(samples);
  EXPECT_TRUE(IsPlaying());

  player_->Stop();
  EXPECT_FALSE(IsPlaying());

  player_->Play(samples);
  EXPECT_TRUE(IsPlaying());

  player_->Stop();
  EXPECT_FALSE(IsPlaying());

  DeletePlayer();
}

TEST_F(AudioPlayerTest, OutOfOrderPlayAndStopMultiple) {
  CreatePlayer();
  scoped_refptr<media::AudioBusRefCounted> samples =
      media::AudioBusRefCounted::Create(1, 1337);

  player_->Stop();
  player_->Stop();
  player_->Stop();
  EXPECT_FALSE(IsPlaying());

  player_->Play(samples);
  player_->Play(samples);
  EXPECT_TRUE(IsPlaying());

  player_->Stop();
  player_->Stop();
  EXPECT_FALSE(IsPlaying());

  DeletePlayer();
}

TEST_F(AudioPlayerTest, PlayingEndToEnd) {
  const int kNumSamples = kDefaultFrameCount * 7 + 321;
  CreatePlayer();

  PlayAndVerifySamples(CreateRandomAudioRefCounted(0x1337, 1, kNumSamples));

  DeletePlayer();
}

TEST_F(AudioPlayerTest, PlayingEndToEndRepeated) {
  const int kNumSamples = kDefaultFrameCount * 7 + 537;
  CreatePlayer();

  PlayAndVerifySamples(CreateRandomAudioRefCounted(0x1337, 1, kNumSamples));

  PlayAndVerifySamples(
      CreateRandomAudioRefCounted(0x7331, 1, kNumSamples - 3123));

  PlayAndVerifySamples(CreateRandomAudioRefCounted(0xf00d, 1, kNumSamples * 2));

  DeletePlayer();
}

}  // namespace audio_modem
