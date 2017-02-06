// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_password_form_manager.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store.h"

using autofill::PasswordForm;

namespace password_manager {

CredentialManagerPasswordFormManager::CredentialManagerPasswordFormManager(
    PasswordManagerClient* client,
    base::WeakPtr<PasswordManagerDriver> driver,
    const PasswordForm& observed_form,
    std::unique_ptr<autofill::PasswordForm> saved_form,
    CredentialManagerPasswordFormManagerDelegate* delegate)
    : PasswordFormManager(
          driver->GetPasswordManager(),
          client,
          driver,
          observed_form,
          true,
          base::WrapUnique(new FormSaverImpl(client->GetPasswordStore()))),
      delegate_(delegate),
      saved_form_(std::move(saved_form)) {
  DCHECK(saved_form_);
  FetchDataFromPasswordStore();
}

CredentialManagerPasswordFormManager::~CredentialManagerPasswordFormManager() {
}

void CredentialManagerPasswordFormManager::OnGetPasswordStoreResults(
    ScopedVector<autofill::PasswordForm> results) {
  PasswordFormManager::OnGetPasswordStoreResults(std::move(results));

  // Mark the form as "preferred", as we've been told by the API that this is
  // indeed the credential set that the user used to sign into the site.
  saved_form_->preferred = true;
  ProvisionallySave(*saved_form_, IGNORE_OTHER_POSSIBLE_USERNAMES);
  delegate_->OnProvisionalSaveComplete();
}

}  // namespace password_manager
