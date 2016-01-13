// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/ubertoken_fetcher.h"

#include <vector>

#include "base/logging.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"

UbertokenFetcher::UbertokenFetcher(
    OAuth2TokenService* token_service,
    UbertokenConsumer* consumer,
    net::URLRequestContextGetter* request_context)
    : OAuth2TokenService::Consumer("uber_token_fetcher"),
      token_service_(token_service),
      consumer_(consumer),
      request_context_(request_context) {
  DCHECK(token_service);
  DCHECK(consumer);
  DCHECK(request_context);
}

UbertokenFetcher::~UbertokenFetcher() {
}

void UbertokenFetcher::StartFetchingToken(const std::string& account_id) {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  access_token_request_ =
      token_service_->StartRequest(account_id, scopes, this);
}

void UbertokenFetcher::OnUberAuthTokenSuccess(const std::string& token) {
  consumer_->OnUbertokenSuccess(token);
}

void UbertokenFetcher::OnUberAuthTokenFailure(
    const GoogleServiceAuthError& error) {
  consumer_->OnUbertokenFailure(error);
}

void UbertokenFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  access_token_request_.reset();
  gaia_auth_fetcher_.reset(new GaiaAuthFetcher(this,
                                               GaiaConstants::kChromeSource,
                                               request_context_));
  gaia_auth_fetcher_->StartTokenFetchForUberAuthExchange(access_token);
}

void UbertokenFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  access_token_request_.reset();
  consumer_->OnUbertokenFailure(error);
}
