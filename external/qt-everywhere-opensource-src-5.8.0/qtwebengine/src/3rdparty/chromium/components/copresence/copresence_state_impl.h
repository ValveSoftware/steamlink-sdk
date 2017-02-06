// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_COPRESENCE_STATE_IMPL_H_
#define COMPONENTS_COPRESENCE_COPRESENCE_STATE_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/copresence/proto/enums.pb.h"
#include "components/copresence/public/copresence_state.h"

namespace copresence {

class Directive;
struct ReceivedToken;
struct TransmittedToken;

// This class tracks the internal state of the copresence component
// for debugging purposes. CopresenceState only allows observation,
// but this class accepts updates from elsewhere in the component.
class CopresenceStateImpl final : public CopresenceState {
 public:
  CopresenceStateImpl();
  ~CopresenceStateImpl() override;

  // CopresenceState overrides.
  void AddObserver(CopresenceObserver* observer) override;
  void RemoveObserver(CopresenceObserver* observer) override;
  const std::vector<Directive>& active_directives() const override;
  const std::map<std::string, TransmittedToken>&
  transmitted_tokens() const override;
  const std::map<std::string, ReceivedToken>&
  received_tokens() const override;

  // Update the current active directives.
  void UpdateDirectives(const std::vector<Directive>& directives);

  // Report transmitting a token.
  void UpdateTransmittedToken(const TransmittedToken& token);

  // Report receiving a token.
  void UpdateReceivedToken(const ReceivedToken& token);

  // Report the token state from the server.
  void UpdateTokenStatus(const std::string& token_id, TokenStatus status);

 private:
  // Reconcile the |active_directives_| against |transmitted_tokens_|.
  void UpdateTransmittingTokens();

  std::vector<Directive> active_directives_;

  // TODO(ckehoe): When we support more mediums, separate tokens by medium.
  // Otherwise tokens from different mediums could overwrite each other.
  // TODO(ckehoe): Limit the number of tokens stored.
  std::map<std::string, TransmittedToken> transmitted_tokens_;
  std::map<std::string, ReceivedToken> received_tokens_;

  base::ObserverList<CopresenceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceStateImpl);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_COPRESENCE_STATE_IMPL_H_
