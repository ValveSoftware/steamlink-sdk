// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_DELEGATE_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_DELEGATE_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"

class FakeProfileOAuth2TokenServiceIOSDelegate
    : public ProfileOAuth2TokenServiceIOSDelegate {
 public:
  FakeProfileOAuth2TokenServiceIOSDelegate(
      SigninClient* client,
      ProfileOAuth2TokenServiceIOSProvider* provider,
      AccountTrackerService* account_tracker_service,
      SigninErrorController* signin_error_controller);
  ~FakeProfileOAuth2TokenServiceIOSDelegate() override;

  OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      OAuth2AccessTokenConsumer* consumer) override;

  bool RefreshTokenIsAvailable(const std::string& account_id) const override;
  bool RefreshTokenHasError(const std::string& account_id) const override;
  void UpdateAuthError(const std::string& account_id,
                       const GoogleServiceAuthError& error) override;

  std::vector<std::string> GetAccounts() override;
  void RevokeAllCredentials() override;

  void LoadCredentials(const std::string& primary_account_id) override;

  void UpdateCredentials(const std::string& account_id,
                         const std::string& refresh_token) override;
  void RevokeCredentials(const std::string& account_id) override;
  void AddOrUpdateAccount(const std::string& account_id) override;
  void RemoveAccount(const std::string& account_id) override;

 private:
  void IssueRefreshTokenForUser(const std::string& account_id,
                                const std::string& token);
  std::string GetRefreshToken(const std::string& account_id) const;

  // Calls to this class are expected to be made from the browser UI thread.
  // The purpose of this checker is to detect access to
  // ProfileOAuth2TokenService from multiple threads in upstream code.
  base::ThreadChecker thread_checker_;

  // Maps account ids to their refresh token strings.
  std::map<std::string, std::string> refresh_tokens_;
  // Maps account ids to their auth errors.
  std::map<std::string, GoogleServiceAuthError> auth_errors_;

  DISALLOW_COPY_AND_ASSIGN(FakeProfileOAuth2TokenServiceIOSDelegate);
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_DELEGATE_H_
