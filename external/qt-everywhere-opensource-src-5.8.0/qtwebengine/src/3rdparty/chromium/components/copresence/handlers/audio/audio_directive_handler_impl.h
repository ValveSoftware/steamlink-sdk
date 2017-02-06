// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_IMPL_H_
#define COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "components/audio_modem/public/modem.h"
#include "components/copresence/handlers/audio/audio_directive_handler.h"
#include "components/copresence/public/copresence_constants.h"

namespace base {
class TimeTicks;
class Timer;
}

namespace media {
class AudioBusRefCounted;
}

namespace copresence {

class AudioDirectiveList;
class TickClockRefCounted;

// The AudioDirectiveHandler handles audio transmit and receive instructions.
// TODO(rkc): Currently since WhispernetClient can only have one token encoded
// callback at a time, we need to have both the audible and inaudible in this
// class. Investigate a better way to do this; a few options are abstracting
// out token encoding to a separate class, or allowing whispernet to have
// multiple callbacks for encoded tokens being sent back and have two versions
// of this class.
class AudioDirectiveHandlerImpl final : public AudioDirectiveHandler {
 public:
  explicit AudioDirectiveHandlerImpl(
      const DirectivesCallback& update_directives_callback);
  AudioDirectiveHandlerImpl(
      const DirectivesCallback& update_directives_callback,
      std::unique_ptr<audio_modem::Modem> audio_modem,
      std::unique_ptr<base::Timer> timer,
      const scoped_refptr<TickClockRefCounted>& clock);

  ~AudioDirectiveHandlerImpl() override;

  // AudioDirectiveHandler overrides:
  void Initialize(audio_modem::WhispernetClient* whispernet_client,
                  const audio_modem::TokensCallback& tokens_cb) override;
  void AddInstruction(const Directive& directive,
                      const std::string& op_id) override;
  void RemoveInstructions(const std::string& op_id) override;
  const std::string PlayingToken(audio_modem::AudioType type) const override;
  bool IsPlayingTokenHeard(audio_modem::AudioType type) const override;

 private:
  // Processes the next active instruction,
  // updating our audio manager state accordingly.
  void ProcessNextInstruction();

  // Returns the time that an instruction expires at. This will always return
  // the earliest expiry time among all the active receive and transmit
  // instructions. If we don't have any active instructions, returns false.
  bool GetNextInstructionExpiry(base::TimeTicks* next_event);

  DirectivesCallback update_directives_callback_;
  std::unique_ptr<audio_modem::Modem> audio_modem_;
  std::unique_ptr<base::Timer> audio_event_timer_;
  scoped_refptr<TickClockRefCounted> clock_;

  // Lists of transmits and receives, for both audible and inaudible tokens.
  // AUDIBLE = element 0, INAUDIBLE = element 1 (see copresence_constants.h).
  ScopedVector<AudioDirectiveList> transmits_lists_;
  ScopedVector<AudioDirectiveList> receives_lists_;

  DISALLOW_COPY_AND_ASSIGN(AudioDirectiveHandlerImpl);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_HANDLER_IMPL_H_
