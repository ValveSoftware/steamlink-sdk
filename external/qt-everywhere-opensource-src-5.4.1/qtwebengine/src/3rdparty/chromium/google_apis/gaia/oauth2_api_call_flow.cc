// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_api_call_flow.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/stringprintf.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using net::ResponseCookies;
using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLRequestContextGetter;
using net::URLRequestStatus;

namespace {
static const char kAuthorizationHeaderFormat[] =
    "Authorization: Bearer %s";

static std::string MakeAuthorizationHeader(const std::string& auth_token) {
  return base::StringPrintf(kAuthorizationHeaderFormat, auth_token.c_str());
}
}  // namespace

OAuth2ApiCallFlow::OAuth2ApiCallFlow(
    net::URLRequestContextGetter* context,
    const std::string& refresh_token,
    const std::string& access_token,
    const std::vector<std::string>& scopes)
    : context_(context),
      refresh_token_(refresh_token),
      access_token_(access_token),
      scopes_(scopes),
      state_(INITIAL),
      tried_mint_access_token_(false) {
}

OAuth2ApiCallFlow::~OAuth2ApiCallFlow() {}

void OAuth2ApiCallFlow::Start() {
  BeginApiCall();
}

void OAuth2ApiCallFlow::BeginApiCall() {
  CHECK(state_ == INITIAL || state_ == MINT_ACCESS_TOKEN_DONE);

  // If the access token is empty then directly try to mint one.
  if (access_token_.empty()) {
    BeginMintAccessToken();
  } else {
    state_ = API_CALL_STARTED;
    url_fetcher_.reset(CreateURLFetcher());
    url_fetcher_->Start();  // OnURLFetchComplete will be called.
  }
}

void OAuth2ApiCallFlow::EndApiCall(const net::URLFetcher* source) {
  CHECK_EQ(API_CALL_STARTED, state_);
  state_ = API_CALL_DONE;

  URLRequestStatus status = source->GetStatus();
  if (!status.is_success()) {
    state_ = ERROR_STATE;
    ProcessApiCallFailure(source);
    return;
  }

  // If the response code is 401 Unauthorized then access token may have
  // expired. So try generating a new access token.
  if (source->GetResponseCode() == net::HTTP_UNAUTHORIZED) {
    // If we already tried minting a new access token, don't do it again.
    if (tried_mint_access_token_) {
      state_ = ERROR_STATE;
      ProcessApiCallFailure(source);
    } else {
      BeginMintAccessToken();
    }

    return;
  }

  if (source->GetResponseCode() != net::HTTP_OK) {
    state_ = ERROR_STATE;
    ProcessApiCallFailure(source);
    return;
  }

  ProcessApiCallSuccess(source);
}

void OAuth2ApiCallFlow::BeginMintAccessToken() {
  CHECK(state_ == INITIAL || state_ == API_CALL_DONE);
  CHECK(!tried_mint_access_token_);
  state_ = MINT_ACCESS_TOKEN_STARTED;
  tried_mint_access_token_ = true;

  oauth2_access_token_fetcher_.reset(CreateAccessTokenFetcher());
  oauth2_access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      scopes_);
}

void OAuth2ApiCallFlow::EndMintAccessToken(
    const GoogleServiceAuthError* error) {
  CHECK_EQ(MINT_ACCESS_TOKEN_STARTED, state_);

  if (!error) {
    state_ = MINT_ACCESS_TOKEN_DONE;
    BeginApiCall();
  } else {
    state_ = ERROR_STATE;
    ProcessMintAccessTokenFailure(*error);
  }
}

OAuth2AccessTokenFetcher* OAuth2ApiCallFlow::CreateAccessTokenFetcher() {
  return new OAuth2AccessTokenFetcherImpl(this, context_, refresh_token_);
}

void OAuth2ApiCallFlow::OnURLFetchComplete(const net::URLFetcher* source) {
  CHECK(source);
  CHECK_EQ(API_CALL_STARTED, state_);
  EndApiCall(source);
}

void OAuth2ApiCallFlow::OnGetTokenSuccess(const std::string& access_token,
                                          const base::Time& expiration_time) {
  access_token_ = access_token;
  EndMintAccessToken(NULL);
}

void OAuth2ApiCallFlow::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  EndMintAccessToken(&error);
}

URLFetcher* OAuth2ApiCallFlow::CreateURLFetcher() {
  std::string body = CreateApiCallBody();
  bool empty_body = body.empty();
  URLFetcher* result = net::URLFetcher::Create(
      0,
      CreateApiCallUrl(),
      empty_body ? URLFetcher::GET : URLFetcher::POST,
      this);

  result->SetRequestContext(context_);
  result->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                       net::LOAD_DO_NOT_SAVE_COOKIES);
  result->AddExtraRequestHeader(MakeAuthorizationHeader(access_token_));
  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS. Retrying once should
  // be enough in those cases; let the fetcher retry up to 3 times just in case.
  // http://crbug.com/163710
  result->SetAutomaticallyRetryOnNetworkChanges(3);

  if (!empty_body)
    result->SetUploadData("application/x-www-form-urlencoded", body);

  return result;
}
