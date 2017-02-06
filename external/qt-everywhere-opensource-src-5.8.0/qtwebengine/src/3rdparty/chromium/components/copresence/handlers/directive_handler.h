// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_H_
#define COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "components/audio_modem/public/whispernet_client.h"

namespace copresence {

class Directive;

// The directive handler manages transmit and receive directives.
class DirectiveHandler {
 public:
  DirectiveHandler() {}
  virtual ~DirectiveHandler() {}

  // Starts processing directives with the provided Whispernet delegate.
  // Directives will be queued until this function is called.
  // |whispernet_client| is owned by the caller and must outlive the
  // DirectiveHandler.
  // |tokens_cb| is called for all audio tokens found in recorded audio.
  // TODO(ckehoe): This is no longer needed. Merge into the constructor.
  virtual void Start(audio_modem::WhispernetClient* whispernet_client,
                     const audio_modem::TokensCallback& tokens_cb) = 0;

  // Adds a directive to handle.
  virtual void AddDirective(const Directive& directive) = 0;

  // Removes any directives associated with the given operation id.
  virtual void RemoveDirectives(const std::string& op_id) = 0;

  // TODO(ckehoe): Move the Modem to be owned by CopresenceManager.
  // Then this will not need to be passed all the way down the tree.
  virtual const std::string
  GetCurrentAudioToken(audio_modem::AudioType type) const = 0;
  virtual bool IsAudioTokenHeard(audio_modem::AudioType type) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DirectiveHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_H_
