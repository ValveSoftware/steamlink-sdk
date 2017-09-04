// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_REFRESH_TOKEN_ANNOTATION_REQUEST_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_REFRESH_TOKEN_ANNOTATION_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "google_apis/gaia/oauth2_token_service.h"

class PrefService;
class SigninClient;

// RefreshTokenAnnotationRequest sends request to IssueToken endpoint with
// device_id to backfill device info for refresh tokens issued pre-M38. It is
// important to keep server QPS low therefore this request is sent on average
// once per 10 days per profile.
// This code shold be removed once majority of refresh tokens are updated.
class RefreshTokenAnnotationRequest : public base::NonThreadSafe,
                                      public OAuth2TokenService::Consumer,
                                      public OAuth2ApiCallFlow {
 public:
  ~RefreshTokenAnnotationRequest() override;

  // Checks if it's time to send IssueToken request. If it is then schedule
  // delay for next request and start request flow.
  //
  // If request is started then return value is scoped_ptr to request. Callback
  // will be posted on current thread when request flow is finished one way or
  // another. Callback's purpose is to delete request after it is done.
  //
  // If request is deleted before flow is complete all internal requests will be
  // cancelled and callback will not be posted.
  //
  // If SendIfNeeded returns nullptr then callback will not be posted.
  static std::unique_ptr<RefreshTokenAnnotationRequest> SendIfNeeded(
      PrefService* pref_service,
      OAuth2TokenService* token_service,
      SigninClient* signin_client,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const std::string& account_id,
      const base::Closure& request_callback);

  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // OAuth2ApiCallFlow implementation.
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  void ProcessApiCallSuccess(const net::URLFetcher* source) override;
  void ProcessApiCallFailure(const net::URLFetcher* source) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RefreshTokenAnnotationRequestTest, ShouldSendNow);
  FRIEND_TEST_ALL_PREFIXES(RefreshTokenAnnotationRequestTest,
                           CreateApiCallBody);

  RefreshTokenAnnotationRequest(
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const std::string& product_version,
      const std::string& device_id,
      const std::string& client_id,
      const base::Closure& request_callback);

  static bool ShouldSendNow(PrefService* pref_service);
  void RequestAccessToken(OAuth2TokenService* token_service,
                          const std::string& account_id);

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  std::string product_version_;
  std::string device_id_;
  std::string client_id_;
  base::Closure request_callback_;

  std::unique_ptr<OAuth2TokenService::Request> access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(RefreshTokenAnnotationRequest);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_REFRESH_TOKEN_ANNOTATION_REQUEST_H_
