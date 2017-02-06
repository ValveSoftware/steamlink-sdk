// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audio_modem/modem_impl.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/audio_modem/audio_modem_switches.h"
#include "components/audio_modem/audio_player_impl.h"
#include "components/audio_modem/audio_recorder_impl.h"
#include "components/audio_modem/public/whispernet_client.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"
#include "third_party/webrtc/common_audio/wav_file.h"

namespace audio_modem {

namespace {

const int kMaxSamples = 10000;
const int kTokenTimeoutMs = 2000;
const int kMonoChannelCount = 1;

// UrlSafe is defined as:
// '/' represented by a '_' and '+' represented by a '-'
// TODO(ckehoe): Move this to a central place.
std::string FromUrlSafe(std::string token) {
  base::ReplaceChars(token, "-", "+", &token);
  base::ReplaceChars(token, "_", "/", &token);
  return token;
}
std::string ToUrlSafe(std::string token) {
  base::ReplaceChars(token, "+", "-", &token);
  base::ReplaceChars(token, "/", "_", &token);
  return token;
}

// TODO(ckehoe): Move this to a central place.
std::string AudioTypeToString(AudioType audio_type) {
  if (audio_type == AUDIBLE)
    return "audible";
  if (audio_type == INAUDIBLE)
    return "inaudible";

  NOTREACHED() << "Got unexpected token type " << audio_type;
  return std::string();
}

bool ReadBooleanFlag(const std::string& flag, bool default_value) {
  const std::string flag_value = base::ToLowerASCII(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(flag));
  if (flag_value == "true" || flag_value == "1")
    return true;
  if (flag_value == "false" || flag_value == "0")
    return false;
  LOG_IF(ERROR, !flag_value.empty())
      << "Unrecognized value \"" << flag_value << " for flag "
      << flag << ". Defaulting to " << default_value;
  return default_value;
}

}  // namespace


// Public functions.

ModemImpl::ModemImpl() : client_(nullptr), recorder_(nullptr) {
  // TODO(rkc): Move all of these into initializer lists once it is allowed.
  should_be_playing_[AUDIBLE] = false;
  should_be_playing_[INAUDIBLE] = false;
  should_be_recording_[AUDIBLE] = false;
  should_be_recording_[INAUDIBLE] = false;

  player_enabled_[AUDIBLE] = ReadBooleanFlag(
      switches::kAudioModemEnableAudibleBroadcast, true);
  player_enabled_[INAUDIBLE] = ReadBooleanFlag(
      switches::kAudioModemEnableInaudibleBroadcast, true);
  player_[AUDIBLE] = nullptr;
  player_[INAUDIBLE] = nullptr;

  samples_caches_.resize(2);
  samples_caches_[AUDIBLE] = new SamplesMap(kMaxSamples);
  samples_caches_[INAUDIBLE] = new SamplesMap(kMaxSamples);
}

void ModemImpl::Initialize(WhispernetClient* client,
                           const TokensCallback& tokens_cb) {
  DCHECK(client);
  client_ = client;
  tokens_cb_ = tokens_cb;

  // These will be unregistered on destruction, so unretained is safe to use.
  client_->RegisterTokensCallback(
      base::Bind(&ModemImpl::OnTokensFound, base::Unretained(this)));
  client_->RegisterSamplesCallback(
      base::Bind(&ModemImpl::OnTokenEncoded, base::Unretained(this)));

  if (!player_[AUDIBLE])
    player_[AUDIBLE] = new AudioPlayerImpl();
  player_[AUDIBLE]->Initialize();

  if (!player_[INAUDIBLE])
    player_[INAUDIBLE] = new AudioPlayerImpl();
  player_[INAUDIBLE]->Initialize();

  decode_cancelable_cb_.Reset(base::Bind(
      &ModemImpl::DecodeSamplesConnector, base::Unretained(this)));
  if (!recorder_)
    recorder_ = new AudioRecorderImpl();
  recorder_->Initialize(decode_cancelable_cb_.callback());

  dump_tokens_dir_ = base::FilePath(base::CommandLine::ForCurrentProcess()
      ->GetSwitchValueNative(switches::kAudioModemDumpTokensToDir));
}

ModemImpl::~ModemImpl() {
  if (player_[AUDIBLE])
    player_[AUDIBLE]->Finalize();
  if (player_[INAUDIBLE])
    player_[INAUDIBLE]->Finalize();
  if (recorder_)
    recorder_->Finalize();

  // Whispernet initialization may never have completed.
  if (client_) {
    client_->RegisterTokensCallback(TokensCallback());
    client_->RegisterSamplesCallback(SamplesCallback());
  }
}

void ModemImpl::StartPlaying(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  should_be_playing_[type] = true;
  // If we don't have our token encoded yet, this check will be false, for now.
  // Once our token is encoded, OnTokenEncoded will call UpdateToken, which
  // will call this code again (if we're still supposed to be playing).
  SamplesMap::iterator samples =
      samples_caches_[type]->Get(playing_token_[type]);
  if (samples != samples_caches_[type]->end()) {
    DCHECK(!playing_token_[type].empty());
    if (player_enabled_[type]) {
      started_playing_[type] = base::Time::Now();
      player_[type]->Play(samples->second);

      // If we're playing, we always record to hear what we are playing.
      recorder_->Record();
    } else {
      DVLOG(3) << "Skipping playback for disabled " << AudioTypeToString(type)
               << " player.";
    }
  }
}

void ModemImpl::StopPlaying(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  should_be_playing_[type] = false;
  player_[type]->Stop();
  // If we were only recording to hear our own played tokens, stop.
  if (!should_be_recording_[AUDIBLE] && !should_be_recording_[INAUDIBLE])
    recorder_->Stop();
  playing_token_[type] = std::string();
}

void ModemImpl::StartRecording(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  should_be_recording_[type] = true;
  recorder_->Record();
}

void ModemImpl::StopRecording(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  should_be_recording_[type] = false;
  recorder_->Stop();
}

void ModemImpl::SetToken(AudioType type,
                         const std::string& url_safe_token) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  std::string token = FromUrlSafe(url_safe_token);
  if (samples_caches_[type]->Get(token) == samples_caches_[type]->end()) {
    client_->EncodeToken(token, type, token_params_);
  } else {
    UpdateToken(type, token);
  }
}

const std::string ModemImpl::GetToken(AudioType type) const {
  return playing_token_[type];
}

bool ModemImpl::IsPlayingTokenHeard(AudioType type) const {
  base::TimeDelta tokenTimeout =
      base::TimeDelta::FromMilliseconds(kTokenTimeoutMs);

  // This is a bit of a hack. If we haven't been playing long enough,
  // return true to avoid tripping an audio fail alarm.
  if (base::Time::Now() - started_playing_[type] < tokenTimeout)
    return true;

  return base::Time::Now() - heard_own_token_[type] < tokenTimeout;
}

void ModemImpl::SetTokenParams(AudioType type, const TokenParameters& params) {
  DCHECK_GT(params.length, 0u);
  token_params_[type] = params;

  // TODO(ckehoe): Make whispernet handle different token lengths
  // simultaneously without reinitializing the decoder over and over.
}

// static
std::unique_ptr<Modem> Modem::Create() {
  return base::WrapUnique<Modem>(new ModemImpl);
}

// Private functions.

void ModemImpl::OnTokenEncoded(
    AudioType type,
    const std::string& token,
    const scoped_refptr<media::AudioBusRefCounted>& samples) {
  samples_caches_[type]->Put(token, samples);
  DumpToken(type, token, samples.get());
  UpdateToken(type, token);
}

void ModemImpl::OnTokensFound(const std::vector<AudioToken>& tokens) {
  std::vector<AudioToken> tokens_to_report;
  for (const auto& token : tokens) {
    AudioType type = token.audible ? AUDIBLE : INAUDIBLE;
    if (playing_token_[type] == token.token)
      heard_own_token_[type] = base::Time::Now();

    if (should_be_recording_[AUDIBLE] && token.audible) {
      tokens_to_report.push_back(token);
    } else if (should_be_recording_[INAUDIBLE] && !token.audible) {
      tokens_to_report.push_back(token);
    }
  }

  if (!tokens_to_report.empty())
    tokens_cb_.Run(tokens_to_report);
}

void ModemImpl::UpdateToken(AudioType type, const std::string& token) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  if (playing_token_[type] == token)
    return;

  // Update token.
  playing_token_[type] = token;

  // If we are supposed to be playing this token type at this moment, switch
  // out playback with the new samples.
  if (should_be_playing_[type])
    RestartPlaying(type);
}

void ModemImpl::RestartPlaying(AudioType type) {
  DCHECK(type == AUDIBLE || type == INAUDIBLE);
  // We should already have this token in the cache. This function is not
  // called from anywhere except update token and only once we have our samples
  // in the cache.
  DCHECK(samples_caches_[type]->Get(playing_token_[type]) !=
         samples_caches_[type]->end());

  player_[type]->Stop();
  StartPlaying(type);
}

void ModemImpl::DecodeSamplesConnector(const std::string& samples) {
  // If we are either supposed to be recording *or* playing, audible or
  // inaudible, we should be decoding that type. This is so that if we are
  // just playing, we will still decode our recorded token so we can check
  // if we heard our own token. Whether or not we report the token to the
  // server is checked for and handled in OnTokensFound.

  bool decode_audible =
      should_be_recording_[AUDIBLE] || should_be_playing_[AUDIBLE];
  bool decode_inaudible =
      should_be_recording_[INAUDIBLE] || should_be_playing_[INAUDIBLE];

  if (decode_audible && decode_inaudible) {
    client_->DecodeSamples(BOTH, samples, token_params_);
  } else if (decode_audible) {
    client_->DecodeSamples(AUDIBLE, samples, token_params_);
  } else if (decode_inaudible) {
    client_->DecodeSamples(INAUDIBLE, samples, token_params_);
  }
}

void ModemImpl::DumpToken(AudioType audio_type,
                          const std::string& token,
                          const media::AudioBus* samples) {
  if (dump_tokens_dir_.empty())
    return;

  // Convert the samples to 16-bit integers.
  std::vector<int16_t> int_samples;
  int_samples.reserve(samples->frames());
  for (int i = 0; i < samples->frames(); i++) {
    int_samples.push_back(round(
        samples->channel(0)[i] * std::numeric_limits<int16_t>::max()));
  }
  DCHECK_EQ(static_cast<int>(int_samples.size()), samples->frames());
  DCHECK_EQ(kMonoChannelCount, samples->channels());

  const std::string filename = base::StringPrintf("%s %s.wav",
      AudioTypeToString(audio_type).c_str(), ToUrlSafe(token).c_str());
  DVLOG(3) << "Dumping token " << filename;

  std::string file_str;
#if defined(OS_WIN)
  base::FilePath file_path = dump_tokens_dir_.Append(
      base::SysNativeMBToWide(filename));
  file_str = base::SysWideToNativeMB(file_path.value());
#else
  file_str = dump_tokens_dir_.Append(filename).value();
#endif

  webrtc::WavWriter writer(file_str, kDefaultSampleRate, kMonoChannelCount);
  writer.WriteSamples(int_samples.data(), int_samples.size());
}

}  // namespace audio_modem
