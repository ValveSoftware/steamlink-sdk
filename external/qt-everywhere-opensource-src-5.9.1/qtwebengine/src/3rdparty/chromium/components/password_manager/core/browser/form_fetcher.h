// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_H_

#include <memory>
#include <vector>

#include "base/macros.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

struct InteractionsStats;

// This is an API for providing stored credentials to PasswordFormManager (PFM),
// so that PFM instances do not have to talk to PasswordStore directly. This
// indirection allows caching of identical requests from PFM on the same origin,
// as well as easier testing (no need to mock the whole PasswordStore when
// testing a PFM).
// TODO(crbug.com/621355): Actually modify the API to support fetching in the
// FormFetcher instance.
class FormFetcher {
 public:
  // State of waiting for a response from a PasswordStore. There might be
  // multiple transitions between these states.
  enum class State { WAITING, NOT_WAITING };

  // API to be implemented by classes which want the results from FormFetcher.
  class Consumer {
   public:
    virtual ~Consumer() = default;

    // FormFetcher calls this method every time the state changes from WAITING
    // to UP_TO_DATE. It fills |non_federated| with pointers to non-federated
    // matches (pointees stay owned by FormFetcher). To access the federated
    // matches, the consumer can simply call GetFederatedMatches().
    // |filtered_count| is the number of non-federated forms which were
    // filtered out by CredentialsFilter and not included in |non_federated|.
    virtual void ProcessMatches(
        const std::vector<const autofill::PasswordForm*>& non_federated,
        size_t filtered_count) = 0;
  };

  FormFetcher() = default;

  virtual ~FormFetcher() = default;

  // Adds |consumer|, which must not be null. If the current state is
  // UP_TO_DATE, calls ProcessMatches on the consumer immediately. Assumes that
  // |consumer| outlives |this|.
  virtual void AddConsumer(Consumer* consumer) = 0;

  // Returns the current state of the FormFetcher
  virtual State GetState() const = 0;

  // Statistics for recent password bubble usage.
  virtual const std::vector<const InteractionsStats*>& GetInteractionsStats()
      const = 0;

  // Federated matches obtained from the backend. Valid only if GetState()
  // returns NOT_WAITING.
  virtual const std::vector<const autofill::PasswordForm*>&
  GetFederatedMatches() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormFetcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_H_
