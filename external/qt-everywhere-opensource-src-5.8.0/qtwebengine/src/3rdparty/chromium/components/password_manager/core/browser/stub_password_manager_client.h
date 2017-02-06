// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/stub_log_manager.h"

namespace password_manager {

// Use this class as a base for mock or test clients to avoid stubbing
// uninteresting pure virtual methods. All the implemented methods are just
// trivial stubs.  Do NOT use in production, only use in tests.
class StubPasswordManagerClient : public PasswordManagerClient {
 public:
  StubPasswordManagerClient();
  ~StubPasswordManagerClient() override;

  // PasswordManagerClient:
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManager> form_to_save,
      password_manager::CredentialSourceType type,
      bool update_password) override;
  bool PromptUserToChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_forms,
      ScopedVector<autofill::PasswordForm> federated_forms,
      const GURL& origin,
      const CredentialsCallback& callback) override;
  void NotifyUserAutoSignin(ScopedVector<autofill::PasswordForm> local_forms,
                            const GURL& origin) override;
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<autofill::PasswordForm>) override;
  void NotifySuccessfulLoginWithExistingPassword(
      const autofill::PasswordForm& form) override;
  void NotifyStorePasswordCalled() override;
  void AutomaticPasswordSave(
      std::unique_ptr<PasswordFormManager> saved_manager) override;
  PrefService* GetPrefs() override;
  PasswordStore* GetPasswordStore() const override;
  const GURL& GetLastCommittedEntryURL() const override;
  const CredentialsFilter* GetStoreResultFilter() const override;
  const LogManager* GetLogManager() const override;

 private:
  // This filter does not filter out anything, it is a dummy implementation of
  // the filter interface.
  class PassThroughCredentialsFilter : public CredentialsFilter {
   public:
    PassThroughCredentialsFilter() {}

    // CredentialsFilter:
    ScopedVector<autofill::PasswordForm> FilterResults(
        ScopedVector<autofill::PasswordForm> results) const override;
    bool ShouldSave(const autofill::PasswordForm& form) const override;
  };

  const PassThroughCredentialsFilter credentials_filter_;
  StubLogManager log_manager_;

  DISALLOW_COPY_AND_ASSIGN(StubPasswordManagerClient);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
