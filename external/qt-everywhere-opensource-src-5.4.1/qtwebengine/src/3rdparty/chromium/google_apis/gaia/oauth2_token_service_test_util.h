// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_TEST_UTIL_H_
#define GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_TEST_UTIL_H_

#include <string>

#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"

std::string GetValidTokenResponse(std::string token, int expiration);

// A simple testing consumer.
class TestingOAuth2TokenServiceConsumer : public OAuth2TokenService::Consumer {
 public:
  TestingOAuth2TokenServiceConsumer();
  virtual ~TestingOAuth2TokenServiceConsumer();

  // OAuth2TokenService::Consumer overrides.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& token,
                                 const base::Time& expiration_date) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  std::string last_token_;
  int number_of_successful_tokens_;
  GoogleServiceAuthError last_error_;
  int number_of_errors_;
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_TEST_UTIL_H_
