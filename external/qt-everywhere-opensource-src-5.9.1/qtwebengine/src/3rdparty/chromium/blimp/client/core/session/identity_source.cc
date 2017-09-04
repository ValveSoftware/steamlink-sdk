// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/session/identity_source.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "blimp/client/core/switches/blimp_client_switches.h"

namespace blimp {
namespace client {

namespace {
// OAuth2 token scope.
const char kOAuth2TokenScope[] =
    "https://www.googleapis.com/auth/userinfo.email";

// Max retry times when OAuth2 token request is canceled.
const int kTokenRequestCancelMaxRetry = 3;
}  // namespace

IdentitySource::IdentitySource(
    std::unique_ptr<IdentityProvider> identity_provider,
    const base::Callback<void(const GoogleServiceAuthError&)>& error_callback,
    const TokenCallback& callback)
    : OAuth2TokenService::Consumer("blimp_client"),
      identity_provider_(std::move(identity_provider)),
      error_callback_(error_callback),
      token_callback_(callback),
      is_fetching_token_(false),
      retry_times_(0) {
  DCHECK(identity_provider_.get());
  identity_provider_->AddObserver(this);
}

IdentitySource::~IdentitySource() {
  identity_provider_->RemoveActiveAccountRefreshTokenObserver(this);
  identity_provider_->RemoveObserver(this);
}

void IdentitySource::Connect() {
  if (is_fetching_token_) {
    return;
  }

  // Pass empty token to assignment source if we have command line switches.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kEngineIP)) {
    if (token_callback_) {
      token_callback_.Run(std::string());
    }
    return;
  }

  // User must sign in first to get an OAuth2 token.
  const std::string& account_id = identity_provider_->GetActiveAccountId();
  if (account_id.empty()) {
    VLOG(1) << "User is not signed in before connection to Blimp engine.";
    return;
  }

  account_id_ = account_id;
  is_fetching_token_ = true;
  FetchAuthToken();
}

std::string IdentitySource::GetActiveUsername() {
  return identity_provider_->GetActiveUsername();
}

// Add sign in state observer.
void IdentitySource::AddObserver(IdentityProvider::Observer* observer) {
  DCHECK(identity_provider_);
  identity_provider_->AddObserver(observer);
}

// Remove sign in state observer.
void IdentitySource::RemoveObserver(IdentityProvider::Observer* observer) {
  DCHECK(identity_provider_);
  identity_provider_->RemoveObserver(observer);
}

void IdentitySource::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  token_request_.reset();
  is_fetching_token_ = false;
  retry_times_ = 0;

  if (token_callback_) {
    token_callback_.Run(access_token);
  }
}

// Fail to get the token after retries attempts in native layer and Java layer.
void IdentitySource::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  token_request_.reset();

  // Retry the request.
  // The embedder can invalidate the refresh token at any time, this happens
  // during application start up or when user switches account.
  // OnGetTokenFailure will be called and the error code is REQUEST_CANCELED.
  if (error.state() == GoogleServiceAuthError::State::REQUEST_CANCELED &&
      retry_times_ < kTokenRequestCancelMaxRetry) {
    retry_times_++;
    VLOG(1) << "Retrying to get OAuth2 token due to request cancellation. "
               "retry time = "
            << retry_times_;
    FetchAuthToken();
    return;
  }

  // If request failure was not caused by cancellation, or reached max retry
  // times on request cancellation, propagate the error to embedder.
  is_fetching_token_ = false;
  retry_times_ = 0;
  VLOG(1) << "OAuth2 token error: " << error.state();
  error_callback_.Run(error);
}

void IdentitySource::OnRefreshTokenAvailable(const std::string& account_id) {
  if (account_id != account_id_) {
    return;
  }

  identity_provider_->RemoveActiveAccountRefreshTokenObserver(this);
  FetchAuthToken();
}

void IdentitySource::FetchAuthToken() {
  OAuth2TokenService* token_service = identity_provider_->GetTokenService();
  DCHECK(token_service);

  if (token_service->RefreshTokenIsAvailable(account_id_)) {
    OAuth2TokenService::ScopeSet scopes;
    scopes.insert(kOAuth2TokenScope);
    token_request_ = token_service->StartRequest(account_id_, scopes, this);
  } else {
    identity_provider_->AddActiveAccountRefreshTokenObserver(this);
  }
}

}  // namespace client
}  // namespace blimp
