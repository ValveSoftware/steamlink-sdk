// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_oauth2_token_service.h"

void ProfileOAuth2TokenService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  CancelRequestsForAccount(account_id);
  ClearCacheForAccount(account_id);
}

void ProfileOAuth2TokenService::OnRefreshTokenRevoked(
    const std::string& account_id) {
  CancelRequestsForAccount(account_id);
  ClearCacheForAccount(account_id);
}

ProfileOAuth2TokenService::ProfileOAuth2TokenService(
    OAuth2TokenServiceDelegate* delegate)
    : OAuth2TokenService(delegate) {
  AddObserver(this);
}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {
  RemoveObserver(this);
}

void ProfileOAuth2TokenService::Shutdown() {
  CancelAllRequests();
  GetDelegate()->Shutdown();
}

void ProfileOAuth2TokenService::LoadCredentials(
    const std::string& primary_account_id) {
  GetDelegate()->LoadCredentials(primary_account_id);
}

void ProfileOAuth2TokenService::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  GetDelegate()->UpdateCredentials(account_id, refresh_token);
}

void ProfileOAuth2TokenService::RevokeCredentials(
    const std::string& account_id) {
  GetDelegate()->RevokeCredentials(account_id);
}

const net::BackoffEntry* ProfileOAuth2TokenService::GetDelegateBackoffEntry() {
  return GetDelegate()->BackoffEntry();
}
