// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_require_user_mediation_task.h"

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_store.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace password_manager {

CredentialManagerPendingRequireUserMediationTask::
    CredentialManagerPendingRequireUserMediationTask(
        CredentialManagerPendingRequireUserMediationTaskDelegate* delegate,
        const GURL& origin,
        const std::vector<std::string>& affiliated_realms)
    : delegate_(delegate),
      affiliated_realms_(affiliated_realms.begin(), affiliated_realms.end()) {
  registrable_domains_.insert(
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
}

CredentialManagerPendingRequireUserMediationTask::
    ~CredentialManagerPendingRequireUserMediationTask() = default;

void CredentialManagerPendingRequireUserMediationTask::AddOrigin(
    const GURL& origin) {
  registrable_domains_.insert(
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
}

void CredentialManagerPendingRequireUserMediationTask::
    OnGetPasswordStoreResults(ScopedVector<autofill::PasswordForm> results) {
  PasswordStore* store = delegate_->GetPasswordStore();
  for (autofill::PasswordForm* form : results) {
    std::string form_registrable_domain =
        net::registry_controlled_domains::GetDomainAndRegistry(
            form->origin,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (registrable_domains_.count(form_registrable_domain) ||
        (affiliated_realms_.count(form->signon_realm) &&
         AffiliatedMatchHelper::IsValidAndroidCredential(*form))) {
      form->skip_zero_click = true;
      // Note that UpdateLogin ends up copying the form while posting a task to
      // update the PasswordStore, so it's fine to let |results| delete the
      // original at the end of this method.
      store->UpdateLogin(*form);
    }
  }

  delegate_->DoneRequiringUserMediation();
}

}  // namespace password_manager
