// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_client.h"

#include <memory>

#include "base/memory/scoped_vector.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_form_manager.h"

namespace password_manager {

ScopedVector<autofill::PasswordForm>
StubPasswordManagerClient::PassThroughCredentialsFilter::FilterResults(
    ScopedVector<autofill::PasswordForm> results) const {
  return results;
}

bool StubPasswordManagerClient::PassThroughCredentialsFilter::ShouldSave(
    const autofill::PasswordForm& form) const {
  return true;
}

StubPasswordManagerClient::StubPasswordManagerClient() {}

StubPasswordManagerClient::~StubPasswordManagerClient() {}

bool StubPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<PasswordFormManager> form_to_save,
    password_manager::CredentialSourceType type,
    bool update_password) {
  return false;
}

bool StubPasswordManagerClient::PromptUserToChooseCredentials(
    ScopedVector<autofill::PasswordForm> local_forms,
    ScopedVector<autofill::PasswordForm> federated_forms,
    const GURL& origin,
    const CredentialsCallback& callback) {
  return false;
}

void StubPasswordManagerClient::NotifyUserAutoSignin(
    ScopedVector<autofill::PasswordForm> local_forms,
    const GURL& origin) {}

void StubPasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<autofill::PasswordForm> form) {}

void StubPasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    const autofill::PasswordForm& form) {}

void StubPasswordManagerClient::NotifyStorePasswordCalled() {}

void StubPasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<PasswordFormManager> saved_manager) {}

PrefService* StubPasswordManagerClient::GetPrefs() {
  return nullptr;
}

PasswordStore* StubPasswordManagerClient::GetPasswordStore() const {
  return nullptr;
}

const GURL& StubPasswordManagerClient::GetLastCommittedEntryURL() const {
  return GURL::EmptyGURL();
}

const CredentialsFilter* StubPasswordManagerClient::GetStoreResultFilter()
    const {
  return &credentials_filter_;
}

const LogManager* StubPasswordManagerClient::GetLogManager() const {
  return &log_manager_;
}

}  // namespace password_manager
