// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_handler_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/audio_modem/public/modem.h"
#include "components/copresence/handlers/audio/audio_directive_list.h"
#include "components/copresence/handlers/audio/tick_clock_ref_counted.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/public/copresence_constants.h"
#include "media/base/audio_bus.h"

using audio_modem::AUDIBLE;
using audio_modem::INAUDIBLE;
using audio_modem::TokenParameters;

namespace copresence {

namespace {

base::TimeTicks GetEarliestEventTime(AudioDirectiveList* list,
                                     base::TimeTicks event_time) {
  std::unique_ptr<AudioDirective> active_directive = list->GetActiveDirective();

  if (!active_directive)
    return event_time;
  if (event_time.is_null())
    return active_directive->end_time;

  return std::min(active_directive->end_time, event_time);
}

void ConvertDirectives(const std::vector<AudioDirective>& in_directives,
                       std::vector<Directive>* out_directives) {
  for (const AudioDirective& in_directive : in_directives)
    out_directives->push_back(in_directive.server_directive);
}

}  // namespace


// Public functions.

AudioDirectiveHandlerImpl::AudioDirectiveHandlerImpl(
    const DirectivesCallback& update_directives_callback)
    : update_directives_callback_(update_directives_callback),
      audio_modem_(audio_modem::Modem::Create()),
      audio_event_timer_(new base::OneShotTimer),
      clock_(new TickClockRefCounted(new base::DefaultTickClock)) {}

AudioDirectiveHandlerImpl::AudioDirectiveHandlerImpl(
    const DirectivesCallback& update_directives_callback,
    std::unique_ptr<audio_modem::Modem> audio_modem,
    std::unique_ptr<base::Timer> timer,
    const scoped_refptr<TickClockRefCounted>& clock)
    : update_directives_callback_(update_directives_callback),
      audio_modem_(std::move(audio_modem)),
      audio_event_timer_(std::move(timer)),
      clock_(clock) {}

AudioDirectiveHandlerImpl::~AudioDirectiveHandlerImpl() {}

void AudioDirectiveHandlerImpl::Initialize(
    audio_modem::WhispernetClient* whispernet_client,
    const audio_modem::TokensCallback& tokens_cb) {
  DCHECK(audio_modem_);
  audio_modem_->Initialize(whispernet_client, tokens_cb);

  DCHECK(transmits_lists_.empty());
  transmits_lists_.push_back(new AudioDirectiveList(clock_));
  transmits_lists_.push_back(new AudioDirectiveList(clock_));

  DCHECK(receives_lists_.empty());
  receives_lists_.push_back(new AudioDirectiveList(clock_));
  receives_lists_.push_back(new AudioDirectiveList(clock_));
}

void AudioDirectiveHandlerImpl::AddInstruction(
    const Directive& directive,
    const std::string& op_id) {
  DCHECK(transmits_lists_.size() == 2u && receives_lists_.size() == 2u)
      << "Call Initialize() before other AudioDirectiveHandler methods";

  const TokenInstruction& instruction = directive.token_instruction();
  base::TimeDelta ttl =
      base::TimeDelta::FromMilliseconds(directive.ttl_millis());
  const size_t token_length = directive.configuration().token_params().length();

  switch (instruction.token_instruction_type()) {
    case TRANSMIT:
      DVLOG(2) << "Audio Transmit Directive received. Token: "
               << instruction.token_id()
               << " with medium=" << instruction.medium()
               << " with TTL=" << ttl.InMilliseconds();
      DCHECK_GT(token_length, 0u);
      switch (instruction.medium()) {
        case AUDIO_ULTRASOUND_PASSBAND:
          audio_modem_->SetTokenParams(INAUDIBLE,
                                       TokenParameters(token_length));
          transmits_lists_[INAUDIBLE]->AddDirective(op_id, directive);
          audio_modem_->SetToken(INAUDIBLE, instruction.token_id());
          break;
        case AUDIO_AUDIBLE_DTMF:
          audio_modem_->SetTokenParams(AUDIBLE, TokenParameters(token_length));
          transmits_lists_[AUDIBLE]->AddDirective(op_id, directive);
          audio_modem_->SetToken(AUDIBLE, instruction.token_id());
          break;
        default:
          NOTREACHED();
      }
      break;

    case RECEIVE:
      DVLOG(2) << "Audio Receive Directive received."
               << " with medium=" << instruction.medium()
               << " with TTL=" << ttl.InMilliseconds();
      DCHECK_GT(token_length, 0u);
      switch (instruction.medium()) {
        case AUDIO_ULTRASOUND_PASSBAND:
          audio_modem_->SetTokenParams(INAUDIBLE,
                                       TokenParameters(token_length));
          receives_lists_[INAUDIBLE]->AddDirective(op_id, directive);
          break;
        case AUDIO_AUDIBLE_DTMF:
          audio_modem_->SetTokenParams(AUDIBLE, TokenParameters(token_length));
          receives_lists_[AUDIBLE]->AddDirective(op_id, directive);
          break;
        default:
          NOTREACHED();
      }
      break;

    case UNKNOWN_TOKEN_INSTRUCTION_TYPE:
    default:
      LOG(WARNING) << "Unknown Audio Transmit Directive received. type = "
                   << instruction.token_instruction_type();
  }

  ProcessNextInstruction();
}

void AudioDirectiveHandlerImpl::RemoveInstructions(const std::string& op_id) {
  DCHECK(transmits_lists_.size() == 2u && receives_lists_.size() == 2u)
      << "Call Initialize() before other AudioDirectiveHandler methods";

  transmits_lists_[AUDIBLE]->RemoveDirective(op_id);
  transmits_lists_[INAUDIBLE]->RemoveDirective(op_id);
  receives_lists_[AUDIBLE]->RemoveDirective(op_id);
  receives_lists_[INAUDIBLE]->RemoveDirective(op_id);

  ProcessNextInstruction();
}

const std::string AudioDirectiveHandlerImpl::PlayingToken(
    audio_modem::AudioType type) const {
  return audio_modem_->GetToken(type);
}

bool AudioDirectiveHandlerImpl::IsPlayingTokenHeard(
    audio_modem::AudioType type) const {
  return audio_modem_->IsPlayingTokenHeard(type);
}


// Private functions.

void AudioDirectiveHandlerImpl::ProcessNextInstruction() {
  DCHECK(audio_event_timer_);
  audio_event_timer_->Stop();

  // Change |audio_modem_| state for audible transmits.
  if (transmits_lists_[AUDIBLE]->GetActiveDirective())
    audio_modem_->StartPlaying(AUDIBLE);
  else
    audio_modem_->StopPlaying(AUDIBLE);

  // Change audio_modem_ state for inaudible transmits.
  if (transmits_lists_[INAUDIBLE]->GetActiveDirective())
    audio_modem_->StartPlaying(INAUDIBLE);
  else
    audio_modem_->StopPlaying(INAUDIBLE);

  // Change audio_modem_ state for audible receives.
  if (receives_lists_[AUDIBLE]->GetActiveDirective())
    audio_modem_->StartRecording(AUDIBLE);
  else
    audio_modem_->StopRecording(AUDIBLE);

  // Change audio_modem_ state for inaudible receives.
  if (receives_lists_[INAUDIBLE]->GetActiveDirective())
    audio_modem_->StartRecording(INAUDIBLE);
  else
    audio_modem_->StopRecording(INAUDIBLE);

  base::TimeTicks next_event_time;
  if (GetNextInstructionExpiry(&next_event_time)) {
    audio_event_timer_->Start(
        FROM_HERE,
        next_event_time - clock_->NowTicks(),
        base::Bind(&AudioDirectiveHandlerImpl::ProcessNextInstruction,
                   base::Unretained(this)));
  }

  // TODO(crbug.com/436584): Instead of this, store the directives
  // in a single list, and prune them when expired.
  if (!update_directives_callback_.is_null()) {
    std::vector<Directive> directives;
    ConvertDirectives(transmits_lists_[AUDIBLE]->directives(), &directives);
    ConvertDirectives(transmits_lists_[INAUDIBLE]->directives(), &directives);
    ConvertDirectives(receives_lists_[AUDIBLE]->directives(), &directives);
    ConvertDirectives(receives_lists_[INAUDIBLE]->directives(), &directives);
    update_directives_callback_.Run(directives);
  }
}

bool AudioDirectiveHandlerImpl::GetNextInstructionExpiry(
    base::TimeTicks* expiry) {
  DCHECK(expiry);

  *expiry = GetEarliestEventTime(transmits_lists_[AUDIBLE], base::TimeTicks());
  *expiry = GetEarliestEventTime(transmits_lists_[INAUDIBLE], *expiry);
  *expiry = GetEarliestEventTime(receives_lists_[AUDIBLE], *expiry);
  *expiry = GetEarliestEventTime(receives_lists_[INAUDIBLE], *expiry);

  return !expiry->is_null();
}

}  // namespace copresence
