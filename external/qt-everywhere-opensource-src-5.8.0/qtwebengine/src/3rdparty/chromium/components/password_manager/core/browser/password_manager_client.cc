// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

bool PasswordManagerClient::IsAutomaticPasswordSavingEnabled() const {
  return false;
}

bool PasswordManagerClient::IsSavingAndFillingEnabledForCurrentPage() const {
  return true;
}

bool PasswordManagerClient::IsFillingEnabledForCurrentPage() const {
  return true;
}

void PasswordManagerClient::ForceSavePassword() {
}

void PasswordManagerClient::GeneratePassword() {}

void PasswordManagerClient::PasswordWasAutofilled(
    const autofill::PasswordFormMap& best_matches,
    const GURL& origin,
    const std::vector<std::unique_ptr<autofill::PasswordForm>>*
        federated_matches) const {}

PasswordSyncState PasswordManagerClient::GetPasswordSyncState() const {
  return NOT_SYNCING_PASSWORDS;
}

bool PasswordManagerClient::WasLastNavigationHTTPError() const {
  return false;
}

bool PasswordManagerClient::DidLastPageLoadEncounterSSLErrors() const {
  return false;
}

bool PasswordManagerClient::IsOffTheRecord() const {
  return false;
}

const PasswordManager* PasswordManagerClient::GetPasswordManager() const {
  return nullptr;
}

PasswordManager* PasswordManagerClient::GetPasswordManager() {
  return const_cast<PasswordManager*>(
      static_cast<const PasswordManagerClient*>(this)->GetPasswordManager());
}

autofill::AutofillManager*
PasswordManagerClient::GetAutofillManagerForMainFrame() {
  return nullptr;
}

const GURL& PasswordManagerClient::GetMainFrameURL() const {
  return GURL::EmptyGURL();
}

bool PasswordManagerClient::IsUpdatePasswordUIEnabled() const {
  return false;
}

const LogManager* PasswordManagerClient::GetLogManager() const {
  return nullptr;
}

}  // namespace password_manager
