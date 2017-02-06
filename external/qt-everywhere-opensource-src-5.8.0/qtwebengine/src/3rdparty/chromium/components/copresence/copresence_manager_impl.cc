// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/copresence_manager_impl.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/audio_modem/public/whispernet_client.h"
#include "components/copresence/copresence_state_impl.h"
#include "components/copresence/handlers/directive_handler_impl.h"
#include "components/copresence/handlers/gcm_handler_impl.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/rpc/rpc_handler.h"

using google::protobuf::RepeatedPtrField;

using audio_modem::AUDIBLE;
using audio_modem::AudioToken;
using audio_modem::INAUDIBLE;

namespace {

const int kPollTimerIntervalMs = 3000;   // milliseconds.
const int kAudioCheckIntervalMs = 1000;  // milliseconds.

const int kQueuedMessageTimeout = 10;    // seconds.
const int kMaxQueuedMessages = 1000;

}  // namespace

namespace copresence {

bool SupportedTokenMedium(const TokenObservation& token) {
  for (const TokenSignals& signals : token.signals()) {
    if (signals.medium() == AUDIO_ULTRASOUND_PASSBAND ||
        signals.medium() == AUDIO_AUDIBLE_DTMF)
      return true;
  }
  return false;
}


// Public functions.

CopresenceManagerImpl::CopresenceManagerImpl(CopresenceDelegate* delegate)
    : delegate_(delegate),
      whispernet_init_callback_(
          base::Bind(&CopresenceManagerImpl::WhispernetInitComplete,
                     // This callback gets cancelled when we are destroyed.
                     base::Unretained(this))),
      init_failed_(false),
      state_(new CopresenceStateImpl),
      directive_handler_(new DirectiveHandlerImpl(
          // The directive handler and its descendants
          // will be destructed before the CopresenceState instance.
          base::Bind(&CopresenceStateImpl::UpdateDirectives,
                     base::Unretained(state_.get())))),
      poll_timer_(new base::RepeatingTimer),
      audio_check_timer_(new base::RepeatingTimer),
      queued_messages_by_token_(
          base::TimeDelta::FromSeconds(kQueuedMessageTimeout),
          kMaxQueuedMessages) {
  DCHECK(delegate_);
  DCHECK(delegate_->GetWhispernetClient());
  // TODO(ckehoe): Handle whispernet initialization in the whispernet component.
  delegate_->GetWhispernetClient()->Initialize(
      whispernet_init_callback_.callback());

  MessagesCallback messages_callback = base::Bind(
      &CopresenceManagerImpl::DispatchMessages,
      // This will only be passed to objects that we own.
      base::Unretained(this));

  if (delegate->GetGCMDriver())
    gcm_handler_.reset(new GCMHandlerImpl(delegate->GetGCMDriver(),
                                          directive_handler_.get(),
                                          messages_callback));

  rpc_handler_.reset(new RpcHandler(delegate,
                                    directive_handler_.get(),
                                    state_.get(),
                                    gcm_handler_.get(),
                                    messages_callback));

  directive_handler_->Start(delegate_->GetWhispernetClient(),
                            base::Bind(&CopresenceManagerImpl::ReceivedTokens,
                                       base::Unretained(this)));
}

CopresenceManagerImpl::~CopresenceManagerImpl() {
  whispernet_init_callback_.Cancel();
}

CopresenceState* CopresenceManagerImpl::state() {
  return state_.get();
}

// Returns false if any operations were malformed.
void CopresenceManagerImpl::ExecuteReportRequest(
    const ReportRequest& request,
    const std::string& app_id,
    const std::string& auth_token,
    const StatusCallback& callback) {
  // If initialization has failed, reject all requests.
  if (init_failed_) {
    callback.Run(FAIL);
    return;
  }

  // We'll need to modify the ReportRequest, so we make our own copy to send.
  std::unique_ptr<ReportRequest> request_copy(new ReportRequest(request));
  rpc_handler_->SendReportRequest(std::move(request_copy), app_id, auth_token,
                                  callback);
}


// Private functions.

void CopresenceManagerImpl::WhispernetInitComplete(bool success) {
  if (success) {
    DVLOG(3) << "Whispernet initialized successfully.";
    poll_timer_->Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(kPollTimerIntervalMs),
                       base::Bind(&CopresenceManagerImpl::PollForMessages,
                                  base::Unretained(this)));
    audio_check_timer_->Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kAudioCheckIntervalMs),
        base::Bind(&CopresenceManagerImpl::AudioCheck, base::Unretained(this)));
  } else {
    LOG(ERROR) << "Whispernet initialization failed!";
    init_failed_ = true;
  }
}

void CopresenceManagerImpl::ReceivedTokens(
    const std::vector<AudioToken>& tokens) {
  rpc_handler_->ReportTokens(tokens);

  for (const AudioToken audio_token : tokens) {
    const std::string& token_id = audio_token.token;
    DVLOG(3) << "Heard token: " << token_id;

    // Update the CopresenceState.
    ReceivedToken token(
        token_id,
        audio_token.audible ? AUDIO_AUDIBLE_DTMF : AUDIO_ULTRASOUND_PASSBAND,
        base::Time::Now());
    state_->UpdateReceivedToken(token);

    // Deliver messages that were pre-sent on this token.
    if (queued_messages_by_token_.HasKey(token_id)) {
      // Not const because we have to remove the required tokens for delivery.
      // We're going to delete this whole vector at the end anyway.
      RepeatedPtrField<SubscribedMessage>* messages =
          queued_messages_by_token_.GetMutableValue(token_id);
      DCHECK_GT(messages->size(), 0)
          << "Empty entry in queued_messages_by_token_";

      // These messages still have their required tokens stored, and
      // DispatchMessages() will still check for them. If we don't remove
      // the tokens before delivery, we'll just end up re-queuing the message.
      for (SubscribedMessage& message : *messages)
        message.mutable_required_token()->Clear();

      DVLOG(3) << "Delivering " << messages->size()
               << " message(s) pre-sent on token " << token_id;
      DispatchMessages(*messages);

      // The messages have been delivered, so we don't need to keep them
      // in the queue. Note that the token will still be reported
      // to the server (above), so we'll keep getting the message.
      // But we can now drop our local copy of it.
      int erase_count = queued_messages_by_token_.Erase(token_id);
      DCHECK_GT(erase_count, 0);
    }
  }
}

void CopresenceManagerImpl::AudioCheck() {
  if (!directive_handler_->GetCurrentAudioToken(AUDIBLE).empty() &&
      !directive_handler_->IsAudioTokenHeard(AUDIBLE)) {
    delegate_->HandleStatusUpdate(AUDIO_FAIL);
  } else if (!directive_handler_->GetCurrentAudioToken(INAUDIBLE).empty() &&
             !directive_handler_->IsAudioTokenHeard(INAUDIBLE)) {
    delegate_->HandleStatusUpdate(AUDIO_FAIL);
  }
}

// Report our currently playing tokens to the server.
void CopresenceManagerImpl::PollForMessages() {
  const std::string& audible_token =
      directive_handler_->GetCurrentAudioToken(AUDIBLE);
  const std::string& inaudible_token =
      directive_handler_->GetCurrentAudioToken(INAUDIBLE);

  std::vector<AudioToken> tokens;
  if (!audible_token.empty())
    tokens.push_back(AudioToken(audible_token, true));
  if (!inaudible_token.empty())
    tokens.push_back(AudioToken(inaudible_token, false));

  if (!tokens.empty())
    rpc_handler_->ReportTokens(tokens);
}

void CopresenceManagerImpl::DispatchMessages(
    const RepeatedPtrField<SubscribedMessage>& messages) {
  if (messages.size() == 0)
    return;

  // Index the messages by subscription id.
  std::map<std::string, std::vector<Message>> messages_by_subscription;
  DVLOG(3) << "Processing " << messages.size() << " received message(s).";
  int immediate_message_count = 0;
  for (const SubscribedMessage& message : messages) {
    // If tokens are required for this message, queue it.
    // Otherwise stage it for delivery.
    if (message.required_token_size() > 0) {
      int supported_token_count = 0;
      for (const TokenObservation& token : message.required_token()) {
        if (SupportedTokenMedium(token)) {
          if (!queued_messages_by_token_.HasKey(token.token_id())) {
            queued_messages_by_token_.Add(
                token.token_id(), RepeatedPtrField<SubscribedMessage>());
          }
          RepeatedPtrField<SubscribedMessage>* queued_messages =
              queued_messages_by_token_.GetMutableValue(token.token_id());
          DCHECK(queued_messages);
          queued_messages->Add()->CopyFrom(message);
          supported_token_count++;
        }
      }

      if (supported_token_count > 0) {
        DVLOG(3) << "Queued message under " << supported_token_count
                 << "token(s).";
      } else {
        VLOG(2) << "Discarded message that requires one of "
                << message.required_token_size()
                << " token(s), all on unsupported mediums.";
      }
    } else {
      immediate_message_count++;
      for (const std::string& subscription_id : message.subscription_id()) {
        messages_by_subscription[subscription_id].push_back(
            message.published_message());
      }
    }
  }

  // Send the messages for each subscription.
  DVLOG(3) << "Dispatching " << immediate_message_count << "message(s) for "
           << messages_by_subscription.size() << " subscription(s).";
  for (const auto& map_entry : messages_by_subscription) {
    // TODO(ckehoe): Once we have the app ID from the server, we need to pass
    // it in here and get rid of the app id registry from the main API class.
    const std::string& subscription = map_entry.first;
    const std::vector<Message>& messages = map_entry.second;
    delegate_->HandleMessages(std::string(), subscription, messages);
  }
}

}  // namespace copresence
