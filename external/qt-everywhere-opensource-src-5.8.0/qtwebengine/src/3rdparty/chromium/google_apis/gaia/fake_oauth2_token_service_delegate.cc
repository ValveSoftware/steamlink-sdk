// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"

FakeOAuth2TokenServiceDelegate::AccountInfo::AccountInfo(
    const std::string& refresh_token)
    : refresh_token(refresh_token),
      error(GoogleServiceAuthError::NONE) {}

FakeOAuth2TokenServiceDelegate::FakeOAuth2TokenServiceDelegate(
    net::URLRequestContextGetter* request_context)
    : request_context_(request_context) {
}

FakeOAuth2TokenServiceDelegate::~FakeOAuth2TokenServiceDelegate() {
}

OAuth2AccessTokenFetcher*
FakeOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  AccountInfoMap::const_iterator it = refresh_tokens_.find(account_id);
  DCHECK(it != refresh_tokens_.end());
  return new OAuth2AccessTokenFetcherImpl(consumer, getter,
                                          it->second->refresh_token);
}

bool FakeOAuth2TokenServiceDelegate::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  return !GetRefreshToken(account_id).empty();
}

bool FakeOAuth2TokenServiceDelegate::RefreshTokenHasError(
    const std::string& account_id) const {
  auto it = refresh_tokens_.find(account_id);
  // TODO(rogerta): should we distinguish between transient and persistent?
  return it == refresh_tokens_.end() ? false : IsError(it->second->error);
}

std::string FakeOAuth2TokenServiceDelegate::GetRefreshToken(
    const std::string& account_id) const {
  AccountInfoMap::const_iterator it = refresh_tokens_.find(account_id);
  if (it != refresh_tokens_.end())
    return it->second->refresh_token;
  return std::string();
}

std::vector<std::string> FakeOAuth2TokenServiceDelegate::GetAccounts() {
  std::vector<std::string> account_ids;
  for (AccountInfoMap::const_iterator iter = refresh_tokens_.begin();
       iter != refresh_tokens_.end(); ++iter) {
    account_ids.push_back(iter->first);
  }
  return account_ids;
}

void FakeOAuth2TokenServiceDelegate::RevokeAllCredentials() {
  std::vector<std::string> account_ids = GetAccounts();
  for (std::vector<std::string>::const_iterator it = account_ids.begin();
       it != account_ids.end(); it++) {
    RevokeCredentials(*it);
  }
}

void FakeOAuth2TokenServiceDelegate::LoadCredentials(
    const std::string& primary_account_id) {
  FireRefreshTokensLoaded();
}

void FakeOAuth2TokenServiceDelegate::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  IssueRefreshTokenForUser(account_id, refresh_token);
}

void FakeOAuth2TokenServiceDelegate::IssueRefreshTokenForUser(
    const std::string& account_id,
    const std::string& token) {
  ScopedBatchChange batch(this);
  if (token.empty()) {
    refresh_tokens_.erase(account_id);
    FireRefreshTokenRevoked(account_id);
  } else {
    refresh_tokens_[account_id].reset(new AccountInfo(token));
    FireRefreshTokenAvailable(account_id);
    // TODO(atwilson): Maybe we should also call FireRefreshTokensLoaded() here?
  }
}

void FakeOAuth2TokenServiceDelegate::RevokeCredentials(
    const std::string& account_id) {
  IssueRefreshTokenForUser(account_id, std::string());
}

net::URLRequestContextGetter*
FakeOAuth2TokenServiceDelegate::GetRequestContext() const {
  return request_context_.get();
}

void FakeOAuth2TokenServiceDelegate::SetLastErrorForAccount(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  auto it = refresh_tokens_.find(account_id);
  DCHECK(it != refresh_tokens_.end());
  it->second->error = error;
}
