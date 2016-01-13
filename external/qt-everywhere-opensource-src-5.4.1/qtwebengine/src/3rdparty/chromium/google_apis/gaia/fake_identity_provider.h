// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_FAKE_IDENTITY_PROVIDER_H_
#define GOOGLE_APIS_GAIA_FAKE_IDENTITY_PROVIDER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "google_apis/gaia/identity_provider.h"

class OAuth2TokenService;

// Fake identity provider implementation.
class FakeIdentityProvider : public IdentityProvider {
 public:
  explicit FakeIdentityProvider(OAuth2TokenService* token_service);
  virtual ~FakeIdentityProvider();

  void LogIn(const std::string& account_id);
  void LogOut();

  // IdentityProvider:
  virtual std::string GetActiveUsername() OVERRIDE;
  virtual std::string GetActiveAccountId() OVERRIDE;
  virtual OAuth2TokenService* GetTokenService() OVERRIDE;
  virtual bool RequestLogin() OVERRIDE;

 private:
  std::string account_id_;
  OAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeIdentityProvider);
};

#endif  // GOOGLE_APIS_GAIA_FAKE_IDENTITY_PROVIDER_H_
