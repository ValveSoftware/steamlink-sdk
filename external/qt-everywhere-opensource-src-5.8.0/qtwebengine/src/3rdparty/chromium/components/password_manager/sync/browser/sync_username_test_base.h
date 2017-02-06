// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A base test fixture for mocking sync and signin infrastructure. Used for
// testing sync-related code.

#ifndef COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_SYNC_USERNAME_TEST_BASE_H_
#define COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_SYNC_USERNAME_TEST_BASE_H_

#include "components/autofill/core/common/password_form.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_driver/fake_sync_service.h"
#include "sync/internal_api/public/base/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class SyncUsernameTestBase : public testing::Test {
 public:
  SyncUsernameTestBase();
  ~SyncUsernameTestBase() override;

  // Instruct the signin manager to sign in with |email| or out.
  void FakeSigninAs(const std::string& email);
  void FakeSignout();

  // Produce a sample PasswordForm.
  static autofill::PasswordForm SimpleGaiaForm(const char* username);
  static autofill::PasswordForm SimpleNonGaiaForm(const char* username);

  // Instruct the sync service to pretend whether or not it is syncing
  // passwords.
  void SetSyncingPasswords(bool syncing_passwords);

  const sync_driver::SyncService* sync_service() const {
    return &sync_service_;
  }

  const SigninManagerBase* signin_manager() { return &signin_manager_; }

 private:
  class LocalFakeSyncService : public sync_driver::FakeSyncService {
   public:
    LocalFakeSyncService();
    ~LocalFakeSyncService() override;

    // sync_driver::SyncService:
    syncer::ModelTypeSet GetPreferredDataTypes() const override;

    void set_syncing_passwords(bool syncing_passwords) {
      syncing_passwords_ = syncing_passwords;
    }

   private:
    bool syncing_passwords_;
  };

  class FakeSigninManagerBase : public SigninManagerBase {
   public:
    FakeSigninManagerBase(SigninClient* client,
                          AccountTrackerService* account_tracker_service)
        : SigninManagerBase(client, account_tracker_service) {}

    using SigninManagerBase::clear_authenticated_user;
  };

  user_prefs::TestingPrefServiceSyncable prefs_;
  TestSigninClient signin_client_;
  AccountTrackerService account_tracker_;
  FakeSigninManagerBase signin_manager_;
  LocalFakeSyncService sync_service_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_SYNC_USERNAME_TEST_BASE_H_
