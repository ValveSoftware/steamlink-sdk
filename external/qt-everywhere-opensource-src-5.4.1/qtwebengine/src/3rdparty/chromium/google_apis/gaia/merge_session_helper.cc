// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/merge_session_helper.h"

#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

MergeSessionHelper::MergeSessionHelper(
    OAuth2TokenService* token_service,
    net::URLRequestContextGetter* request_context,
    Observer* observer)
    : token_service_(token_service),
      request_context_(request_context) {
  if (observer)
    AddObserver(observer);
}

MergeSessionHelper::~MergeSessionHelper() {
  DCHECK(accounts_.empty());
}

void MergeSessionHelper::LogIn(const std::string& account_id) {
  DCHECK(!account_id.empty());
  accounts_.push_back(account_id);
  if (accounts_.size() == 1)
    StartFetching();
}

void MergeSessionHelper::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MergeSessionHelper::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void MergeSessionHelper::CancelAll() {
  gaia_auth_fetcher_.reset();
  uber_token_fetcher_.reset();
  accounts_.clear();
}

void MergeSessionHelper::LogOut(
    const std::string& account_id,
    const std::vector<std::string>& accounts) {
  DCHECK(!account_id.empty());
  LogOutInternal(account_id, accounts);
}

void MergeSessionHelper::LogOutInternal(
    const std::string& account_id,
    const std::vector<std::string>& accounts) {
  bool pending = !accounts_.empty();

  if (pending) {
    for (std::deque<std::string>::const_iterator it = accounts_.begin() + 1;
        it != accounts_.end(); it++) {
      if (!it->empty() &&
          (std::find(accounts.begin(), accounts.end(), *it) == accounts.end() ||
           *it == account_id)) {
        // We have a pending log in request for an account followed by
        // a signout.
        GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
        SignalComplete(*it, error);
      }
    }

    // Remove every thing in the work list besides the one that is running.
    accounts_.resize(1);
  }

  // Signal a logout to be the next thing to do unless the pending
  // action is already a logout.
  if (!pending || !accounts_.front().empty())
    accounts_.push_back("");

  for (std::vector<std::string>::const_iterator it = accounts.begin();
      it != accounts.end(); it++) {
    if (*it != account_id) {
      DCHECK(!it->empty());
      accounts_.push_back(*it);
    }
  }

  if (!pending)
    StartLogOutUrlFetch();
}

void MergeSessionHelper::LogOutAllAccounts() {
  LogOutInternal("", std::vector<std::string>());
}

void MergeSessionHelper::SignalComplete(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  // Its possible for the observer to delete |this| object.  Don't access
  // access any members after this calling the observer.  This method should
  // be the last call in any other method.
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    MergeSessionCompleted(account_id, error));
}

void MergeSessionHelper::StartLogOutUrlFetch() {
  DCHECK(accounts_.front().empty());
  GURL logout_url(GaiaUrls::GetInstance()->service_logout_url());
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(logout_url, net::URLFetcher::GET, this);
  fetcher->SetRequestContext(request_context_);
  fetcher->Start();
}

void MergeSessionHelper::OnUbertokenSuccess(const std::string& uber_token) {
  VLOG(1) << "MergeSessionHelper::OnUbertokenSuccess"
          << " account=" << accounts_.front();
  gaia_auth_fetcher_.reset(new GaiaAuthFetcher(this,
                                               GaiaConstants::kChromeSource,
                                               request_context_));
  gaia_auth_fetcher_->StartMergeSession(uber_token);
}

void MergeSessionHelper::OnUbertokenFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed to retrieve ubertoken"
          << " account=" << accounts_.front()
          << " error=" << error.ToString();
  const std::string account_id = accounts_.front();
  HandleNextAccount();
  SignalComplete(account_id, error);
}

void MergeSessionHelper::OnMergeSessionSuccess(const std::string& data) {
  DVLOG(1) << "MergeSession successful account=" << accounts_.front();
  const std::string account_id = accounts_.front();
  HandleNextAccount();
  SignalComplete(account_id, GoogleServiceAuthError::AuthErrorNone());
}

void MergeSessionHelper::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed MergeSession"
          << " account=" << accounts_.front()
          << " error=" << error.ToString();
  const std::string account_id = accounts_.front();
  HandleNextAccount();
  SignalComplete(account_id, error);
}

void MergeSessionHelper::StartFetching() {
  uber_token_fetcher_.reset(new UbertokenFetcher(token_service_,
                                                 this,
                                                 request_context_));
  uber_token_fetcher_->StartFetchingToken(accounts_.front());
}

void MergeSessionHelper::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(accounts_.front().empty());
  HandleNextAccount();
}

void MergeSessionHelper::HandleNextAccount() {
  accounts_.pop_front();
  gaia_auth_fetcher_.reset();
  if (accounts_.empty()) {
    uber_token_fetcher_.reset();
  } else {
    if (accounts_.front().empty()) {
      StartLogOutUrlFetch();
    } else {
      StartFetching();
    }
  }
}
