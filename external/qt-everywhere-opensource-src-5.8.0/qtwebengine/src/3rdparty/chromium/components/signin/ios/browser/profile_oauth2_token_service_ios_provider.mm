// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/profile_oauth2_token_service_ios_provider.h"

std::vector<ProfileOAuth2TokenServiceIOSProvider::AccountInfo>
ProfileOAuth2TokenServiceIOSProvider::GetAllAccounts() const {
  return std::vector<ProfileOAuth2TokenServiceIOSProvider::AccountInfo>();
}

void ProfileOAuth2TokenServiceIOSProvider::GetAccessToken(
    const std::string& gaia_id,
    const std::string& client_id,
    const std::string& client_secret,
    const std::set<std::string>& scopes,
    const AccessTokenCallback& callback) {}

AuthenticationErrorCategory
ProfileOAuth2TokenServiceIOSProvider::GetAuthenticationErrorCategory(
    const std::string& gaia_id,
    NSError* error) const {
  return kAuthenticationErrorCategoryUnknownErrors;
}

