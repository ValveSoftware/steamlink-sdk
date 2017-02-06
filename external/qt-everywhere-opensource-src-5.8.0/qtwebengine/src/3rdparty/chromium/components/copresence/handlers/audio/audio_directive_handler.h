// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_H_
#define COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_H_

#include <string>

#include "components/audio_modem/public/whispernet_client.h"

namespace copresence {

class Directive;

// The AudioDirectiveHandler handles audio transmit and receive instructions.
class AudioDirectiveHandler {
 public:
  virtual ~AudioDirectiveHandler() {}

  // Do not use this class before calling this.
  virtual void Initialize(audio_modem::WhispernetClient* whispernet_client,
                          const audio_modem::TokensCallback& tokens_cb) = 0;

  // Adds an instruction to our handler. The instruction will execute and be
  // removed after the ttl expires.
  virtual void AddInstruction(const Directive& directive,
                              const std::string& op_id) = 0;

  // Removes all instructions associated with this operation id.
  virtual void RemoveInstructions(const std::string& op_id) = 0;

  // Returns the currently playing token.
  virtual const std::string PlayingToken(audio_modem::AudioType type) const = 0;

  // Returns if we have heard the currently playing audio token.
  virtual bool IsPlayingTokenHeard(audio_modem::AudioType type) const = 0;
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_H_
