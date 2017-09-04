// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "components/password_manager/core/browser/form_fetcher.h"

namespace password_manager {

class PasswordManagerClient;

// Production implementation of FormFetcher. Fetches credentials associated
// with a particular origin.
// TODO(crbug.com/621355): This class should ultimately work with PasswordStore.
class FormFetcherImpl : public FormFetcher {
 public:
  explicit FormFetcherImpl(const PasswordManagerClient* client);

  ~FormFetcherImpl() override;

  // FormFetcher:
  void AddConsumer(Consumer* consumer) override;
  State GetState() const override;
  const std::vector<const InteractionsStats*>& GetInteractionsStats()
      const override;
  const std::vector<const autofill::PasswordForm*>& GetFederatedMatches()
      const override;

  // TODO(crbug.com/621355): Remove this once FormFetcher becomes a
  // PasswordStoreConsumer.
  void set_state(State state) { state_ = state; }

  // TODO(crbug.com/621355): Remove this once FormFetcher becomes a
  // PasswordStoreConsumer.
  // Resets the results owned by |this| with new results from PasswordStore and
  // sets the state to NOT_WAITING.
  void SetResults(std::vector<std::unique_ptr<autofill::PasswordForm>> results);

  // TODO(crbug.com/621355): Remove this once FormFetcher becomes a
  // PasswordStoreConsumer.
  // Resets the stats owned by |this| with new stats from PasswordStore.
  void SetStats(std::vector<std::unique_ptr<InteractionsStats>> stats);

 private:
  // Results obtained from PasswordStore:
  std::vector<std::unique_ptr<autofill::PasswordForm>> non_federated_;

  // Federated credentials relevant to the observed form. They are neither
  // filled not saved by PasswordFormManager, so they are kept separately from
  // non-federated matches.
  std::vector<std::unique_ptr<autofill::PasswordForm>> federated_;

  // Statistics for the current domain.
  std::vector<std::unique_ptr<InteractionsStats>> interactions_stats_;

  // Non-owning copies of the vectors above.
  std::vector<const autofill::PasswordForm*> weak_non_federated_;
  std::vector<const autofill::PasswordForm*> weak_federated_;
  std::vector<const InteractionsStats*> weak_interactions_stats_;

  // Consumers of the fetcher, all are assumed to outlive |this|.
  std::set<Consumer*> consumers_;

  // Client used to obtain a CredentialFilter.
  const PasswordManagerClient* const client_;

  // The number of non-federated forms which were filtered out by
  // CredentialsFilter and not included in |non_federated_|.
  size_t filtered_count_ = 0;

  // State of the fetcher.
  State state_ = State::NOT_WAITING;

  DISALLOW_COPY_AND_ASSIGN(FormFetcherImpl);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_
