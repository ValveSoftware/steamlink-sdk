// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/logging.h"
#include "base/time/time.h"
#include "components/copresence/copresence_state_impl.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/public/copresence_constants.h"
#include "components/copresence/public/copresence_observer.h"

namespace copresence {

namespace {

template<typename TokenType>
void HandleCommonFields(const TokenType& new_token, TokenType* current_token) {
  if (current_token->id.empty()) {
    current_token->id = new_token.id;
    current_token->medium = new_token.medium;
    current_token->start_time = new_token.start_time;
  } else {
    DCHECK_EQ(new_token.id, current_token->id);
    DCHECK_EQ(new_token.medium, current_token->medium);
    DCHECK(new_token.start_time.is_null() ||
           new_token.start_time == current_token->start_time);
  }
}

void UpdateToken(const TransmittedToken& new_token,
                 TransmittedToken* current_token) {
  HandleCommonFields(new_token, current_token);

  current_token->stop_time = new_token.stop_time;
  current_token->broadcast_confirmed = new_token.broadcast_confirmed;
}

void UpdateToken(const ReceivedToken& new_token,
                 ReceivedToken* current_token) {
  HandleCommonFields(new_token, current_token);

  current_token->last_time = new_token.last_time;
  if (new_token.valid != ReceivedToken::UNKNOWN)
    current_token->valid = new_token.valid;
}

}  // namespace


// Public functions.

CopresenceStateImpl::CopresenceStateImpl() {}

CopresenceStateImpl::~CopresenceStateImpl() {}

void CopresenceStateImpl::AddObserver(CopresenceObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void CopresenceStateImpl::RemoveObserver(CopresenceObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

const std::vector<Directive>& CopresenceStateImpl::active_directives() const {
  return active_directives_;
}

const std::map<std::string, TransmittedToken>&
CopresenceStateImpl::transmitted_tokens() const {
  return transmitted_tokens_;
}

const std::map<std::string, ReceivedToken>&
CopresenceStateImpl::received_tokens() const {
  return received_tokens_;
}

// TODO(ckehoe): Only send updates if the directives have really changed.
void CopresenceStateImpl::UpdateDirectives(
    const std::vector<Directive>& directives) {
  active_directives_ = directives;
  UpdateTransmittingTokens();
  FOR_EACH_OBSERVER(CopresenceObserver, observers_, DirectivesUpdated());
}

void CopresenceStateImpl::UpdateTransmittedToken(
    const TransmittedToken& token) {
  UpdateToken(token, &transmitted_tokens_[token.id]);
  FOR_EACH_OBSERVER(CopresenceObserver,
                    observers_,
                    TokenTransmitted(transmitted_tokens_[token.id]));
}

// TODO(ckehoe): Check which tokens are no longer heard and report them lost.
void CopresenceStateImpl::UpdateReceivedToken(const ReceivedToken& token) {
  DCHECK(!token.id.empty());

  // TODO(ckehoe): Have CopresenceManagerImpl::AudioCheck() use this to check
  // if we can hear our token, and delete the logic from the AudioManager.
  if (transmitted_tokens_.count(token.id) > 0) {
    transmitted_tokens_[token.id].broadcast_confirmed = true;
    FOR_EACH_OBSERVER(CopresenceObserver,
                      observers_,
                      TokenTransmitted(transmitted_tokens_[token.id]));
  } else {
    ReceivedToken& stored_token = received_tokens_[token.id];
    UpdateToken(token, &stored_token);

    // The decoder doesn't track when this token was heard before,
    // so it should just fill in the last_time.
    // If we've never seen this token, we populate the start time too.
    if (stored_token.start_time.is_null())
      stored_token.start_time = token.last_time;

    FOR_EACH_OBSERVER(CopresenceObserver,
                      observers_,
                      TokenReceived(stored_token));
  }
}

void CopresenceStateImpl::UpdateTokenStatus(const std::string& token_id,
                                            TokenStatus status) {
  if (transmitted_tokens_.count(token_id) > 0) {
    LOG_IF(ERROR, status != VALID)
        << "Broadcast token " << token_id << " is invalid";
  } else if (received_tokens_.count(token_id) > 0) {
    received_tokens_[token_id].valid = status == VALID ?
        ReceivedToken::VALID : ReceivedToken::INVALID;
    FOR_EACH_OBSERVER(CopresenceObserver,
                      observers_,
                      TokenReceived(received_tokens_[token_id]));
  } else {
    LOG(ERROR) << "Got status update for unrecognized token " << token_id;
  }
}


// Private functions.

void CopresenceStateImpl::UpdateTransmittingTokens() {
  std::set<std::string> tokens_to_update;
  for (const auto& token_entry : transmitted_tokens_)
    tokens_to_update.insert(token_entry.first);

  for (const Directive& directive : active_directives_) {
    const TokenInstruction& instruction = directive.token_instruction();
    if (instruction.token_instruction_type() == TRANSMIT) {
      tokens_to_update.erase(instruction.token_id());

      TransmittedToken& token = transmitted_tokens_[instruction.token_id()];
      token.id = instruction.token_id();
      token.medium = instruction.medium();
      token.start_time = base::Time::Now();
      token.stop_time = base::Time::Now() +
          base::TimeDelta::FromMilliseconds(directive.ttl_millis());

      FOR_EACH_OBSERVER(CopresenceObserver,
                        observers_,
                        TokenTransmitted(token));
    }
  }

  // Tokens not updated above are no longer transmitting.
  base::Time now = base::Time::Now();
  for (const std::string& token : tokens_to_update) {
    if (transmitted_tokens_[token].stop_time > now)
      transmitted_tokens_[token].stop_time = now;

    FOR_EACH_OBSERVER(CopresenceObserver,
                      observers_,
                      TokenTransmitted(transmitted_tokens_[token]));
  }
}

}  // namespace copresence
