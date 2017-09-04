// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_identity_provider.h"

#include "base/callback.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

ProfileIdentityProvider::ProfileIdentityProvider(
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service,
    const base::Closure& request_login_callback)
    : signin_manager_(signin_manager),
      token_service_(token_service),
      request_login_callback_(request_login_callback) {
  signin_manager_->AddObserver(this);
}

ProfileIdentityProvider::~ProfileIdentityProvider() {
  signin_manager_->RemoveObserver(this);
}

std::string ProfileIdentityProvider::GetActiveUsername() {
  return signin_manager_->GetAuthenticatedAccountInfo().email;
}

std::string ProfileIdentityProvider::GetActiveAccountId() {
  return signin_manager_->GetAuthenticatedAccountId();
}

OAuth2TokenService* ProfileIdentityProvider::GetTokenService() {
  return token_service_;
}

bool ProfileIdentityProvider::RequestLogin() {
  if (request_login_callback_.is_null())
    return false;
  request_login_callback_.Run();
  return true;
}

void ProfileIdentityProvider::GoogleSigninSucceeded(
    const std::string& account_id,
    const std::string& username,
    const std::string& password) {
  FireOnActiveAccountLogin();
}

void ProfileIdentityProvider::GoogleSignedOut(const std::string& account_id,
                                              const std::string& username) {
  FireOnActiveAccountLogout();
}
