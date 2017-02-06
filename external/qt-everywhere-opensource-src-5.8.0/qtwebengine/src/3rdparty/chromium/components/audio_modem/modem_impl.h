// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIO_MODEM_MODEM_IMPL_H_
#define COMPONENTS_AUDIO_MODEM_MODEM_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/containers/mru_cache.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "components/audio_modem/public/audio_modem_types.h"
#include "components/audio_modem/public/modem.h"
#include "media/base/audio_bus.h"

namespace base {
class Time;
}

namespace audio_modem {

class AudioPlayer;
class AudioRecorder;
class WhispernetClient;

// The ModemImpl class manages the playback and recording of tokens.
// Clients should not necessary expect the modem to play or record continuously.
// In the future, it may timeslice multiple tokens, or implement carrier sense.
class ModemImpl final : public Modem {
 public:
  ModemImpl();
  ~ModemImpl() override;

  // Modem overrides:
  void Initialize(WhispernetClient* client,
                  const TokensCallback& tokens_cb) override;
  void StartPlaying(AudioType type) override;
  void StopPlaying(AudioType type) override;
  void StartRecording(AudioType type) override;
  void StopRecording(AudioType type) override;
  void SetToken(AudioType type, const std::string& url_safe_token) override;
  const std::string GetToken(AudioType type) const override;
  bool IsPlayingTokenHeard(AudioType type) const override;
  void SetTokenParams(AudioType type, const TokenParameters& params) override;

  void set_player_for_testing(AudioType type, AudioPlayer* player) {
    player_[type] = player;
  }
  void set_recorder_for_testing(AudioRecorder* recorder) {
    recorder_ = recorder;
  }

 private:
  using SamplesMap = base::MRUCache<std::string,
                                    scoped_refptr<media::AudioBusRefCounted>>;

  // Receives the audio samples from encoding a token.
  void OnTokenEncoded(AudioType type,
                      const std::string& token,
                      const scoped_refptr<media::AudioBusRefCounted>& samples);

  // Receives any tokens found by decoding audio samples.
  void OnTokensFound(const std::vector<AudioToken>& tokens);

  // Update our currently playing token with the new token. Change the playing
  // samples if needed. Prerequisite: Samples corresponding to this token
  // should already be in the samples cache.
  void UpdateToken(AudioType type, const std::string& token);

  void RestartPlaying(AudioType type);

  void DecodeSamplesConnector(const std::string& samples);

  void DumpToken(AudioType audio_type,
                 const std::string& token,
                 const media::AudioBus* samples);

  WhispernetClient* client_;

  // Callbacks to send tokens back to the client.
  TokensCallback tokens_cb_;

  // This cancelable callback is passed to the recorder. The recorder's
  // destruction will happen on the audio thread, so it can outlive us.
  base::CancelableCallback<void(const std::string&)> decode_cancelable_cb_;

  // We use the AudioType enum to index into all our data structures that work
  // on values for both audible and inaudible.
  static_assert(AUDIBLE == 0, "AudioType::AUDIBLE should be 0.");
  static_assert(INAUDIBLE == 1, "AudioType::INAUDIBLE should be 1.");

  // Indexed using enum AudioType.
  bool player_enabled_[2];
  bool should_be_playing_[2];
  bool should_be_recording_[2];

  // AudioPlayer and AudioRecorder objects are self-deleting. When we call
  // Finalize on them, they clean themselves up on the Audio thread.
  // Indexed using enum AudioType.
  AudioPlayer* player_[2];
  AudioRecorder* recorder_;

  // Indexed using enum AudioType.
  std::string playing_token_[2];
  TokenParameters token_params_[2];
  base::Time started_playing_[2];
  base::Time heard_own_token_[2];

  // Cache that holds the encoded samples. After reaching its limit, the cache
  // expires the oldest samples first.
  // Indexed using enum AudioType.
  ScopedVector<SamplesMap> samples_caches_;

  base::FilePath dump_tokens_dir_;

  DISALLOW_COPY_AND_ASSIGN(ModemImpl);
};

}  // namespace audio_modem

#endif  // COMPONENTS_AUDIO_MODEM_MODEM_IMPL_H_
