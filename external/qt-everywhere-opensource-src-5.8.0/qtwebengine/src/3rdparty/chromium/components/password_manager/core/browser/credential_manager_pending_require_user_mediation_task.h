// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_REQUIRE_USER_MEDIATION_TASK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_REQUIRE_USER_MEDIATION_TASK_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

class GURL;

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace password_manager {

class PasswordStore;

// Handles mediation completion and retrieves embedder-dependent services.
class CredentialManagerPendingRequireUserMediationTaskDelegate {
 public:
  // Retrieves the PasswordStore.
  virtual PasswordStore* GetPasswordStore() = 0;

  // Finishes mediation tasks.
  virtual void DoneRequiringUserMediation() = 0;
};

// Notifies the password store that a list of origins require user mediation.
class CredentialManagerPendingRequireUserMediationTask
    : public PasswordStoreConsumer {
 public:
  CredentialManagerPendingRequireUserMediationTask(
      CredentialManagerPendingRequireUserMediationTaskDelegate* delegate,
      const GURL& origin,
      const std::vector<std::string>& affiliated_realms);
  ~CredentialManagerPendingRequireUserMediationTask() override;

  // Adds an origin to require user mediation.
  void AddOrigin(const GURL& origin);

  // PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      ScopedVector<autofill::PasswordForm> results) override;

 private:
  CredentialManagerPendingRequireUserMediationTaskDelegate* const
      delegate_;  // Weak.
  std::set<std::string> registrable_domains_;
  std::set<std::string> affiliated_realms_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerPendingRequireUserMediationTask);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_REQUIRE_USER_MEDIATION_TASK_H_
