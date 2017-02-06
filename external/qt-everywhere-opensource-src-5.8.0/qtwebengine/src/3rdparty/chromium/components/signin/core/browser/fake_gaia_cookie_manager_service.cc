// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"

FakeGaiaCookieManagerService::FakeGaiaCookieManagerService(
    OAuth2TokenService* token_service,
    const std::string& source,
    SigninClient* client)
    : GaiaCookieManagerService(token_service, source, client),
      url_fetcher_factory_(NULL) {}

void FakeGaiaCookieManagerService::Init(
    net::FakeURLFetcherFactory* url_fetcher_factory) {
  url_fetcher_factory_ = url_fetcher_factory;
}

void FakeGaiaCookieManagerService::SetListAccountsResponseHttpNotFound() {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      "", net::HTTP_NOT_FOUND, net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseWebLoginRequired() {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      "Info=WebLoginRequired", net::HTTP_OK, net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseNoAccounts() {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      "[\"f\", []]", net::HTTP_OK, net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseOneAccount(
    const char* email,
    const char* gaia_id) {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      base::StringPrintf(
          "[\"f\", [[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, 1, \"%s\"]]]",
          email, gaia_id),
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseOneAccountWithExpiry(
    const char* email,
    const char* gaia_id,
    bool expired) {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      base::StringPrintf(
          "[\"f\", [[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, %d, \"%s\"]]]",
          email, expired ? 0 : 1, gaia_id),
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseTwoAccounts(
    const char* email1,
    const char* gaia_id1,
    const char* email2,
    const char* gaia_id2) {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      base::StringPrintf(
          "[\"f\", [[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, 1, \"%s\"], "
          "[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, 1, \"%s\"]]]",
          email1, gaia_id1, email2, gaia_id2),
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseTwoAccountsWithExpiry(
    const char* email1,
    const char* gaia_id1,
    bool account1_expired,
    const char* email2,
    const char* gaia_id2,
    bool account2_expired) {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      base::StringPrintf(
          "[\"f\", [[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, %d, \"%s\"], "
          "[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, %d, \"%s\"]]]",
          email1, account1_expired ? 0 : 1, gaia_id1, email2,
          account2_expired ? 0 : 1, gaia_id2),
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
}
