// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_fetcher_impl.h"

#include <algorithm>
#include <utility>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"

using autofill::PasswordForm;

namespace password_manager {

namespace {

// Splits |store_results| into a vector of non-federated and federated matches.
// Returns the federated matches and keeps the non-federated in |store_results|.
std::vector<std::unique_ptr<PasswordForm>> SplitFederatedMatches(
    std::vector<std::unique_ptr<PasswordForm>>* store_results) {
  const auto first_federated = std::partition(
      store_results->begin(), store_results->end(),
      [](const std::unique_ptr<PasswordForm>& form) {
        return form->federation_origin.unique();  // False means federated.
      });

  // Move out federated matches.
  std::vector<std::unique_ptr<PasswordForm>> federated_matches;
  federated_matches.resize(store_results->end() - first_federated);
  std::move(first_federated, store_results->end(), federated_matches.begin());

  store_results->erase(first_federated, store_results->end());
  return federated_matches;
}

// Create a vector of const T* from a vector of unique_ptr<T>.
template <typename T>
std::vector<const T*> MakeWeakCopies(
    const std::vector<std::unique_ptr<T>>& owning) {
  std::vector<const T*> result;
  result.resize(owning.size());
  std::transform(owning.begin(), owning.end(), result.begin(),
                 [](const std::unique_ptr<T>& ptr) { return ptr.get(); });
  return result;
}

}  // namespace

FormFetcherImpl::FormFetcherImpl(const PasswordManagerClient* client)
    : client_(client) {}

FormFetcherImpl::~FormFetcherImpl() = default;

void FormFetcherImpl::AddConsumer(Consumer* consumer) {
  DCHECK(consumer);
  consumers_.insert(consumer);
  if (state_ == State::NOT_WAITING)
    consumer->ProcessMatches(weak_non_federated_, filtered_count_);
}

FormFetcherImpl::State FormFetcherImpl::GetState() const {
  return state_;
}

const std::vector<const InteractionsStats*>&
FormFetcherImpl::GetInteractionsStats() const {
  return weak_interactions_stats_;
}

const std::vector<const autofill::PasswordForm*>&
FormFetcherImpl::GetFederatedMatches() const {
  return weak_federated_;
}

void FormFetcherImpl::SetResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  DCHECK_EQ(State::WAITING, state_);

  federated_ = SplitFederatedMatches(&results);
  non_federated_ = std::move(results);

  const size_t original_count = non_federated_.size();

  non_federated_ =
      client_->GetStoreResultFilter()->FilterResults(std::move(non_federated_));

  filtered_count_ = original_count - non_federated_.size();

  weak_non_federated_ = MakeWeakCopies(non_federated_);
  weak_federated_ = MakeWeakCopies(federated_);

  state_ = State::NOT_WAITING;

  for (Consumer* consumer : consumers_)
    consumer->ProcessMatches(weak_non_federated_, filtered_count_);
}

void FormFetcherImpl::SetStats(
    std::vector<std::unique_ptr<InteractionsStats>> stats) {
  interactions_stats_ = std::move(stats);
  weak_interactions_stats_ = MakeWeakCopies(interactions_stats_);
}

}  // namespace password_manager
