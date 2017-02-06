// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audio_modem/public/modem.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/audio_modem/audio_player.h"
#include "components/audio_modem/audio_recorder.h"
#include "components/audio_modem/modem_impl.h"
#include "components/audio_modem/test/random_samples.h"
#include "components/audio_modem/test/stub_whispernet_client.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio_modem {

class AudioPlayerStub final : public AudioPlayer {
 public:
  AudioPlayerStub() : is_playing_(false) {}
  ~AudioPlayerStub() override {}

  // AudioPlayer overrides:
  void Initialize() override {}
  void Play(const scoped_refptr<media::AudioBusRefCounted>&) override {
    is_playing_ = true;
  }
  void Stop() override { is_playing_ = false; }
  void Finalize() override { delete this; }

  bool IsPlaying() { return is_playing_; }

 private:
  bool is_playing_;
  DISALLOW_COPY_AND_ASSIGN(AudioPlayerStub);
};

class AudioRecorderStub final : public AudioRecorder {
 public:
  AudioRecorderStub() : is_recording_(false) {}
  ~AudioRecorderStub() override {}

  // AudioRecorder overrides:
  void Initialize(const RecordedSamplesCallback& cb) override { cb_ = cb; }
  void Record() override { is_recording_ = true; }
  void Stop() override { is_recording_ = false; }
  void Finalize() override { delete this; }

  bool IsRecording() { return is_recording_; }

  void TriggerDecodeRequest() {
    if (!cb_.is_null())
      cb_.Run(std::string(0x1337, 'a'));
  }

 private:
  RecordedSamplesCallback cb_;
  bool is_recording_;

  DISALLOW_COPY_AND_ASSIGN(AudioRecorderStub);
};

class ModemTest : public testing::Test {
 public:
  ModemTest()
      : modem_(new ModemImpl),
        audible_player_(new AudioPlayerStub),
        inaudible_player_(new AudioPlayerStub),
        recorder_(new AudioRecorderStub),
        last_received_decode_type_(AUDIO_TYPE_UNKNOWN) {
    std::vector<AudioToken> tokens;
    tokens.push_back(AudioToken("abcdef", true));
    tokens.push_back(AudioToken("123456", false));
    client_.reset(new StubWhispernetClient(
        CreateRandomAudioRefCounted(0x123, 1, 0x321), tokens));

    // TODO(ckehoe): Pass these into the Modem constructor instead.
    modem_->set_player_for_testing(AUDIBLE, audible_player_);
    modem_->set_player_for_testing(INAUDIBLE, inaudible_player_);
    modem_->set_recorder_for_testing(recorder_);
    modem_->Initialize(
        client_.get(),
        base::Bind(&ModemTest::GetTokens, base::Unretained(this)));
  }

  ~ModemTest() override {}

 protected:
  void GetTokens(const std::vector<AudioToken>& tokens) {
    last_received_decode_type_ = AUDIO_TYPE_UNKNOWN;
    for (const auto& token : tokens) {
      if (token.audible && last_received_decode_type_ == INAUDIBLE) {
        last_received_decode_type_ = BOTH;
      } else if (!token.audible && last_received_decode_type_ == AUDIBLE) {
        last_received_decode_type_ = BOTH;
      } else if (token.audible) {
        last_received_decode_type_ = AUDIBLE;
      } else {
        last_received_decode_type_ = INAUDIBLE;
      }
    }
  }

  base::MessageLoop message_loop_;
  // This order is important. The WhispernetClient needs to outlive the Modem.
  std::unique_ptr<WhispernetClient> client_;
  std::unique_ptr<ModemImpl> modem_;

  // These will be deleted by the Modem's destructor calling finalize on them.
  AudioPlayerStub* audible_player_;
  AudioPlayerStub* inaudible_player_;
  AudioRecorderStub* recorder_;

  AudioType last_received_decode_type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModemTest);
};

TEST_F(ModemTest, EncodeToken) {
  modem_->StartPlaying(AUDIBLE);
  // No token yet, player shouldn't be playing.
  EXPECT_FALSE(audible_player_->IsPlaying());

  modem_->SetToken(INAUDIBLE, "abcd");
  // No *audible* token yet, so player still shouldn't be playing.
  EXPECT_FALSE(audible_player_->IsPlaying());

  modem_->SetToken(AUDIBLE, "abcd");
  EXPECT_TRUE(audible_player_->IsPlaying());
}

TEST_F(ModemTest, Record) {
  recorder_->TriggerDecodeRequest();
  EXPECT_EQ(AUDIO_TYPE_UNKNOWN, last_received_decode_type_);

  modem_->StartRecording(AUDIBLE);
  recorder_->TriggerDecodeRequest();
  EXPECT_EQ(AUDIBLE, last_received_decode_type_);

  modem_->StartRecording(INAUDIBLE);
  recorder_->TriggerDecodeRequest();
  EXPECT_EQ(BOTH, last_received_decode_type_);

  modem_->StopRecording(AUDIBLE);
  recorder_->TriggerDecodeRequest();
  EXPECT_EQ(INAUDIBLE, last_received_decode_type_);
}

}  // namespace audio_modem
