// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_oauth2_token_service.h"

FakeOAuth2TokenService::PendingRequest::PendingRequest() {
}

FakeOAuth2TokenService::PendingRequest::~PendingRequest() {
}

FakeOAuth2TokenService::FakeOAuth2TokenService() : request_context_(NULL) {
}

FakeOAuth2TokenService::~FakeOAuth2TokenService() {
}

std::vector<std::string> FakeOAuth2TokenService::GetAccounts() {
  return std::vector<std::string>(account_ids_.begin(), account_ids_.end());
}

void FakeOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  PendingRequest pending_request;
  pending_request.account_id = account_id;
  pending_request.client_id = client_id;
  pending_request.client_secret = client_secret;
  pending_request.scopes = scopes;
  pending_request.request = request->AsWeakPtr();
  pending_requests_.push_back(pending_request);
}

void FakeOAuth2TokenService::InvalidateOAuth2Token(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
}

net::URLRequestContextGetter* FakeOAuth2TokenService::GetRequestContext() {
  return request_context_;
}

bool FakeOAuth2TokenService::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  return account_ids_.count(account_id) != 0;
};

void FakeOAuth2TokenService::AddAccount(const std::string& account_id) {
  account_ids_.insert(account_id);
  FireRefreshTokenAvailable(account_id);
}

void FakeOAuth2TokenService::RemoveAccount(const std::string& account_id) {
  account_ids_.erase(account_id);
  FireRefreshTokenRevoked(account_id);
}

void FakeOAuth2TokenService::IssueAllTokensForAccount(
    const std::string& account_id,
    const std::string& access_token,
    const base::Time& expiration) {

  // Walk the requests and notify the callbacks.
  for (std::vector<PendingRequest>::iterator it = pending_requests_.begin();
       it != pending_requests_.end(); ++it) {
    if (it->request && (account_id == it->account_id)) {
      it->request->InformConsumer(
          GoogleServiceAuthError::AuthErrorNone(), access_token, expiration);
    }
  }
}


OAuth2AccessTokenFetcher* FakeOAuth2TokenService::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  // |FakeOAuth2TokenService| overrides |FetchOAuth2Token| and thus
  // |CreateAccessTokenFetcher| should never be called.
  NOTREACHED();
  return NULL;
}
