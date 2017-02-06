// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/fake_profile_oauth2_token_service_ios_delegate.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"

FakeProfileOAuth2TokenServiceIOSDelegate::
    FakeProfileOAuth2TokenServiceIOSDelegate(
        SigninClient* client,
        ProfileOAuth2TokenServiceIOSProvider* provider,
        AccountTrackerService* account_tracker_service,
        SigninErrorController* signin_error_controller)
    : ProfileOAuth2TokenServiceIOSDelegate(client,
                                           provider,
                                           account_tracker_service,
                                           signin_error_controller) {}

FakeProfileOAuth2TokenServiceIOSDelegate::
    ~FakeProfileOAuth2TokenServiceIOSDelegate() {}

OAuth2AccessTokenFetcher*
FakeProfileOAuth2TokenServiceIOSDelegate::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  std::map<std::string, std::string>::const_iterator it =
      refresh_tokens_.find(account_id);
  DCHECK(it != refresh_tokens_.end());
  std::string refresh_token(it->second);
  return new OAuth2AccessTokenFetcherImpl(consumer, getter, refresh_token);
}

bool FakeProfileOAuth2TokenServiceIOSDelegate::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return !GetRefreshToken(account_id).empty();
}

bool FakeProfileOAuth2TokenServiceIOSDelegate::RefreshTokenHasError(
    const std::string& account_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = auth_errors_.find(account_id);
  return it != auth_errors_.end() && IsError(it->second);
}

void FakeProfileOAuth2TokenServiceIOSDelegate::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auth_errors_.erase(account_id);
  auth_errors_.emplace(account_id, error);
}

std::string FakeProfileOAuth2TokenServiceIOSDelegate::GetRefreshToken(
    const std::string& account_id) const {
  std::map<std::string, std::string>::const_iterator it =
      refresh_tokens_.find(account_id);
  if (it != refresh_tokens_.end())
    return it->second;
  return std::string();
}

std::vector<std::string>
FakeProfileOAuth2TokenServiceIOSDelegate::GetAccounts() {
  std::vector<std::string> account_ids;
  for (std::map<std::string, std::string>::const_iterator iter =
           refresh_tokens_.begin();
       iter != refresh_tokens_.end(); ++iter) {
    account_ids.push_back(iter->first);
  }
  return account_ids;
}

void FakeProfileOAuth2TokenServiceIOSDelegate::RevokeAllCredentials() {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<std::string> account_ids = GetAccounts();
  for (std::vector<std::string>::const_iterator it = account_ids.begin();
       it != account_ids.end(); it++) {
    RevokeCredentials(*it);
  }
}

void FakeProfileOAuth2TokenServiceIOSDelegate::LoadCredentials(
    const std::string& primary_account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FireRefreshTokensLoaded();
}

void FakeProfileOAuth2TokenServiceIOSDelegate::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  IssueRefreshTokenForUser(account_id, refresh_token);
}

void FakeProfileOAuth2TokenServiceIOSDelegate::AddOrUpdateAccount(
    const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  UpdateCredentials(account_id, "fake_refresh_token");
}

void FakeProfileOAuth2TokenServiceIOSDelegate::RemoveAccount(
    const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!account_id.empty());

  IssueRefreshTokenForUser(account_id, "");
}

void FakeProfileOAuth2TokenServiceIOSDelegate::IssueRefreshTokenForUser(
    const std::string& account_id,
    const std::string& token) {
  ScopedBatchChange batch(this);
  if (token.empty()) {
    refresh_tokens_.erase(account_id);
    auth_errors_.erase(account_id);
    FireRefreshTokenRevoked(account_id);
  } else {
    refresh_tokens_[account_id] = token;
    auth_errors_.emplace(account_id,
                         GoogleServiceAuthError(GoogleServiceAuthError::NONE));
    FireRefreshTokenAvailable(account_id);
  }
}

void FakeProfileOAuth2TokenServiceIOSDelegate::RevokeCredentials(
    const std::string& account_id) {
  IssueRefreshTokenForUser(account_id, std::string());
}
