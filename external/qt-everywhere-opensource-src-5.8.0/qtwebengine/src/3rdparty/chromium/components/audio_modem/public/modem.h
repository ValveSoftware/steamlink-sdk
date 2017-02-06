// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIO_MODEM_PUBLIC_MODEM_H_
#define COMPONENTS_AUDIO_MODEM_PUBLIC_MODEM_H_

#include <memory>
#include <string>

#include "components/audio_modem/public/audio_modem_types.h"

namespace audio_modem {

class WhispernetClient;

class Modem {
 public:
  virtual ~Modem() {}

  // Initializes the object. Do not use this object before calling this method.
  virtual void Initialize(WhispernetClient* client,
                          const TokensCallback& tokens_cb) = 0;

  virtual void StartPlaying(AudioType type) = 0;
  virtual void StopPlaying(AudioType type) = 0;

  virtual void StartRecording(AudioType type) = 0;
  virtual void StopRecording(AudioType type) = 0;

  virtual void SetToken(AudioType type,
                        const std::string& url_safe_token) = 0;

  virtual const std::string GetToken(AudioType type) const = 0;

  virtual bool IsPlayingTokenHeard(AudioType type) const = 0;

  virtual void SetTokenParams(AudioType type,
                              const TokenParameters& params) = 0;

  static std::unique_ptr<Modem> Create();
};

}  // namespace audio_modem

#endif  // COMPONENTS_AUDIO_MODEM_PUBLIC_MODEM_H_
