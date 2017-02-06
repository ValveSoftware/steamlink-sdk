// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/fake_signin_manager.h"

#include "base/callback_helpers.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_metrics.h"

FakeSigninManagerBase::FakeSigninManagerBase(
    SigninClient* client,
    AccountTrackerService* account_tracker_service)
    : SigninManagerBase(client, account_tracker_service) {}

FakeSigninManagerBase::~FakeSigninManagerBase() {}

void FakeSigninManagerBase::SignIn(const std::string& account_id) {
  SetAuthenticatedAccountId(account_id);
}

#if !defined(OS_CHROMEOS)

FakeSigninManager::FakeSigninManager(
    SigninClient* client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    GaiaCookieManagerService* cookie_manager_service)
    : SigninManager(client,
                    token_service,
                    account_tracker_service,
                    cookie_manager_service) {}

FakeSigninManager::~FakeSigninManager() {}

void FakeSigninManager::StartSignInWithRefreshToken(
    const std::string& refresh_token,
    const std::string& gaia_id,
    const std::string& username,
    const std::string& password,
    const OAuthTokenFetchedCallback& oauth_fetched_callback) {
  set_auth_in_progress(
      account_tracker_service()->SeedAccountInfo(gaia_id, username));
  set_password(password);
  username_ = username;

  possibly_invalid_gaia_id_.assign(gaia_id);
  possibly_invalid_email_.assign(username);

  if (!oauth_fetched_callback.is_null())
    oauth_fetched_callback.Run(refresh_token);
}

void FakeSigninManager::CompletePendingSignin() {
  SetAuthenticatedAccountId(GetAccountIdForAuthInProgress());
  set_auth_in_progress(std::string());
  FOR_EACH_OBSERVER(
      SigninManagerBase::Observer, observer_list_,
      GoogleSigninSucceeded(authenticated_account_id_, username_, password_));
}

void FakeSigninManager::SignIn(const std::string& gaia_id,
                               const std::string& username,
                               const std::string& password) {
  StartSignInWithRefreshToken(std::string(), gaia_id, username, password,
                              OAuthTokenFetchedCallback());
  CompletePendingSignin();
}

void FakeSigninManager::ForceSignOut() {
  prohibit_signout_ = false;
  SignOut(signin_metrics::SIGNOUT_TEST,
          signin_metrics::SignoutDelete::IGNORE_METRIC);
}

void FakeSigninManager::FailSignin(const GoogleServiceAuthError& error) {
  FOR_EACH_OBSERVER(SigninManagerBase::Observer, observer_list_,
                    GoogleSigninFailed(error));
}

void FakeSigninManager::SignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  if (IsSignoutProhibited())
    return;
  set_auth_in_progress(std::string());
  set_password(std::string());
  const std::string account_id = GetAuthenticatedAccountId();
  const std::string username = GetAuthenticatedAccountInfo().email;
  authenticated_account_id_.clear();

  FOR_EACH_OBSERVER(SigninManagerBase::Observer, observer_list_,
                    GoogleSignedOut(account_id, username));
}

#endif  // !defined (OS_CHROMEOS)
