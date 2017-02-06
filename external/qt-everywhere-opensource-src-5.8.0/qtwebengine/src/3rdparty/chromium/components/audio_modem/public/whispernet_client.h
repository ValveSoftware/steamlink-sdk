// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIO_MODEM_PUBLIC_WHISPERNET_CLIENT_H_
#define COMPONENTS_AUDIO_MODEM_PUBLIC_WHISPERNET_CLIENT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "components/audio_modem/public/audio_modem_types.h"
#include "media/base/audio_bus.h"

namespace audio_modem {

// Generic callback to indicate a boolean success or failure.
using SuccessCallback = base::Callback<void(bool)>;

// Callback to receive encoded samples from Whispernet.
// AudioType type: Type of audio encoding - AUDIBLE or INAUDIBLE.
// const std::string& token: The token that we encoded.
// const scoped_refptr<media::AudioBusRefCounted>& samples - Encoded samples.
using SamplesCallback =
    base::Callback<void(AudioType,
                        const std::string&,
                        const scoped_refptr<media::AudioBusRefCounted>&)>;

// A client for the Whispernet audio library,
// responsible for the actual encoding and decoding of tokens.
class WhispernetClient {
 public:
  // Initialize the whispernet client and call the callback when done. The
  // parameter indicates whether we succeeded or failed.
  virtual void Initialize(const SuccessCallback& init_callback) = 0;

  // Fires an event to request a token encode.
  virtual void EncodeToken(const std::string& token,
                           AudioType type,
                           const TokenParameters token_params[2]) = 0;
  // Fires an event to request a decode for the given samples.
  virtual void DecodeSamples(AudioType type,
                             const std::string& samples,
                             const TokenParameters token_params[2]) = 0;

  // Callback registration methods. The modem will set these to receive data.
  virtual void RegisterTokensCallback(
      const TokensCallback& tokens_callback) = 0;
  virtual void RegisterSamplesCallback(
      const SamplesCallback& samples_callback) = 0;

  // Don't cache these callbacks, as they may become invalid at any time.
  // Always invoke callbacks directly through these accessors.
  virtual TokensCallback GetTokensCallback() = 0;
  virtual SamplesCallback GetSamplesCallback() = 0;
  virtual SuccessCallback GetInitializedCallback() = 0;

  virtual ~WhispernetClient() {}
};

}  // namespace audio_modem

#endif  // COMPONENTS_AUDIO_MODEM_PUBLIC_WHISPERNET_CLIENT_H_
