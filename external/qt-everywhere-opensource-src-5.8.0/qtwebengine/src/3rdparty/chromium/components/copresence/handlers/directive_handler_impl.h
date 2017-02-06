// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_IMPL_H_
#define COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/copresence/handlers/audio/audio_directive_handler_impl.h"
#include "components/copresence/handlers/directive_handler.h"
#include "components/copresence/public/copresence_constants.h"

namespace copresence {

// The directive handler manages transmit and receive directives.
class DirectiveHandlerImpl final : public DirectiveHandler {
 public:
  DirectiveHandlerImpl();
  explicit DirectiveHandlerImpl(
      const DirectivesCallback& update_directives_callback);
  DirectiveHandlerImpl(std::unique_ptr<AudioDirectiveHandler> audio_handler);
  ~DirectiveHandlerImpl() override;

  // DirectiveHandler overrides.
  void Start(audio_modem::WhispernetClient* whispernet_client,
             const audio_modem::TokensCallback& tokens_cb) override;
  void AddDirective(const Directive& directive) override;
  void RemoveDirectives(const std::string& op_id) override;
  const std::string
  GetCurrentAudioToken(audio_modem::AudioType type) const override;
  bool IsAudioTokenHeard(audio_modem::AudioType type) const override;

 private:
  // Starts actually running a directive.
  void StartDirective(const std::string& op_id, const Directive& directive);

  std::unique_ptr<AudioDirectiveHandler> audio_handler_;
  std::map<std::string, std::vector<Directive>> pending_directives_;

  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(DirectiveHandlerImpl);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_DIRECTIVE_HANDLER_IMPL_H_
