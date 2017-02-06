// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/gaia_cookie_manager_service.h"

#include <stddef.h>

#include <queue>
#include <vector>

#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace {

// In case of an error while fetching using the GaiaAuthFetcher or URLFetcher,
// retry with exponential backoff. Try up to 7 times within 15 minutes.
const net::BackoffEntry::Policy kBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential backoff in ms.
  1000,

  // Factor by which the waiting time will be multiplied.
  3,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.2, // 20%

  // Maximum amount of time we are willing to delay our request in ms.
  1000 * 60 * 60 * 4, // 15 minutes.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

const int kMaxFetcherRetries = 8;

// Name of the GAIA cookie that is being observed to detect when available
// accounts have changed in the content-area.
const char* kGaiaCookieName = "APISID";

enum GaiaCookieRequestType {
  ADD_ACCOUNT,
  LOG_OUT_ALL_ACCOUNTS,
  LOG_OUT_ONE_ACCOUNT,
  LIST_ACCOUNTS
};

}  // namespace

GaiaCookieManagerService::GaiaCookieRequest::GaiaCookieRequest(
    GaiaCookieRequestType request_type,
    const std::string& account_id)
  : request_type_(request_type),
    account_id_(account_id) {}

GaiaCookieManagerService::GaiaCookieRequest::~GaiaCookieRequest() {
}

// static
GaiaCookieManagerService::GaiaCookieRequest
GaiaCookieManagerService::GaiaCookieRequest::CreateAddAccountRequest(
      const std::string& account_id) {
  return GaiaCookieManagerService::GaiaCookieRequest(
      GaiaCookieManagerService::GaiaCookieRequestType::ADD_ACCOUNT, account_id);
}

// static
GaiaCookieManagerService::GaiaCookieRequest
GaiaCookieManagerService::GaiaCookieRequest::CreateLogOutRequest() {
  return GaiaCookieManagerService::GaiaCookieRequest(
      GaiaCookieManagerService::GaiaCookieRequestType::LOG_OUT, std::string());
}

GaiaCookieManagerService::GaiaCookieRequest
GaiaCookieManagerService::GaiaCookieRequest::CreateListAccountsRequest() {
  return GaiaCookieManagerService::GaiaCookieRequest(
      GaiaCookieManagerService::GaiaCookieRequestType::LIST_ACCOUNTS,
      std::string());
}

GaiaCookieManagerService::ExternalCcResultFetcher::ExternalCcResultFetcher(
    GaiaCookieManagerService* helper)
    : helper_(helper) {
  DCHECK(helper_);
}

GaiaCookieManagerService::ExternalCcResultFetcher::~ExternalCcResultFetcher() {
  CleanupTransientState();
}

std::string
GaiaCookieManagerService::ExternalCcResultFetcher::GetExternalCcResult() {
  std::vector<std::string> results;
  for (ResultMap::const_iterator it = results_.begin(); it != results_.end();
       ++it) {
    results.push_back(it->first + ":" + it->second);
  }
  return base::JoinString(results, ",");
}

void GaiaCookieManagerService::ExternalCcResultFetcher::Start() {
  m_external_cc_result_start_time_ = base::Time::Now();

  CleanupTransientState();
  results_.clear();
  helper_->gaia_auth_fetcher_.reset(
      helper_->signin_client_->CreateGaiaAuthFetcher(
          this, helper_->source_, helper_->request_context()));
  helper_->gaia_auth_fetcher_->StartGetCheckConnectionInfo();

  // Some fetches may timeout.  Start a timer to decide when the result fetcher
  // has waited long enough.
  // TODO(rogerta): I have no idea how long to wait before timing out.
  // Gaia folks say this should take no more than 2 second even in mobile.
  // This will need to be tweaked.
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(5), this,
               &GaiaCookieManagerService::ExternalCcResultFetcher::Timeout);
}

bool GaiaCookieManagerService::ExternalCcResultFetcher::IsRunning() {
  return helper_->gaia_auth_fetcher_ || fetchers_.size() > 0u;
}

void GaiaCookieManagerService::ExternalCcResultFetcher::TimeoutForTests() {
  Timeout();
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    OnGetCheckConnectionInfoSuccess(const std::string& data) {
  helper_->fetcher_backoff_.InformOfRequest(true);
  gaia_auth_fetcher_timer_.Stop();
  std::unique_ptr<base::Value> value = base::JSONReader::Read(data);
  const base::ListValue* list;
  if (!value || !value->GetAsList(&list)) {
    CleanupTransientState();
    GetCheckConnectionInfoCompleted(false);
    return;
  }

  // If there is nothing to check, terminate immediately.
  if (list->GetSize() == 0) {
    CleanupTransientState();
    GetCheckConnectionInfoCompleted(true);
    return;
  }

  // Start a fetcher for each connection URL that needs to be checked.
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* dict;
    if (list->GetDictionary(i, &dict)) {
      std::string token;
      std::string url;
      if (dict->GetString("carryBackToken", &token) &&
          dict->GetString("url", &url)) {
        results_[token] = "null";
        net::URLFetcher* fetcher = CreateFetcher(GURL(url)).release();
        fetchers_[fetcher->GetOriginalURL()] = std::make_pair(token, fetcher);
        fetcher->Start();
      }
    }
  }
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    OnGetCheckConnectionInfoError(const GoogleServiceAuthError& error) {
  if (++helper_->fetcher_retries_ < kMaxFetcherRetries &&
      error.IsTransientError()) {
    helper_->fetcher_backoff_.InformOfRequest(false);
    gaia_auth_fetcher_timer_.Start(
        FROM_HERE, helper_->fetcher_backoff_.GetTimeUntilRelease(),
        this, &GaiaCookieManagerService::ExternalCcResultFetcher::Start);
    return;
  }

  CleanupTransientState();
  GetCheckConnectionInfoCompleted(false);
}

std::unique_ptr<net::URLFetcher>
GaiaCookieManagerService::ExternalCcResultFetcher::CreateFetcher(
    const GURL& url) {
  std::unique_ptr<net::URLFetcher> fetcher =
      net::URLFetcher::Create(0, url, net::URLFetcher::GET, this);
  fetcher->SetRequestContext(helper_->request_context());
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);

  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS.
  fetcher->SetAutomaticallyRetryOnNetworkChanges(1);
  return fetcher;
}

void GaiaCookieManagerService::ExternalCcResultFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  const GURL& url = source->GetOriginalURL();
  const net::URLRequestStatus& status = source->GetStatus();
  int response_code = source->GetResponseCode();
  if (status.is_success() && response_code == net::HTTP_OK &&
      fetchers_.count(url) > 0) {
    std::string data;
    source->GetResponseAsString(&data);
    // Only up to the first 16 characters of the response are important to GAIA.
    // Truncate if needed to keep amount data sent back to GAIA down.
    if (data.size() > 16)
      data.resize(16);
    results_[fetchers_[url].first] = data;

    // Clean up tracking of this fetcher.  The rest will be cleaned up after
    // the timer expires in CleanupTransientState().
    DCHECK_EQ(source, fetchers_[url].second);
    fetchers_.erase(url);
    delete source;

    // If all expected responses have been received, cancel the timer and
    // report the result.
    if (fetchers_.empty()) {
      CleanupTransientState();
      GetCheckConnectionInfoCompleted(true);
    }
  }
}

void GaiaCookieManagerService::ExternalCcResultFetcher::Timeout() {
  CleanupTransientState();
  GetCheckConnectionInfoCompleted(false);
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    CleanupTransientState() {
  timer_.Stop();
  helper_->gaia_auth_fetcher_.reset();

  for (URLToTokenAndFetcher::const_iterator it = fetchers_.begin();
       it != fetchers_.end(); ++it) {
    delete it->second.second;
  }
  fetchers_.clear();
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    GetCheckConnectionInfoCompleted(bool succeeded) {
  base::TimeDelta time_to_check_connections =
      base::Time::Now() - m_external_cc_result_start_time_;
  signin_metrics::LogExternalCcResultFetches(succeeded,
                                             time_to_check_connections);

  helper_->external_cc_result_fetched_ = true;
  // Since the ExternalCCResultFetcher is only Started in place of calling
  // StartFetchingMergeSession, we can assume we need to call
  // StartFetchingMergeSession. If this assumption becomes invalid, a Callback
  // will need to be passed to Start() and Run() here.
  helper_->StartFetchingMergeSession();
}

GaiaCookieManagerService::GaiaCookieManagerService(
    OAuth2TokenService* token_service,
    const std::string& source,
    SigninClient* signin_client)
    : token_service_(token_service),
      signin_client_(signin_client),
      external_cc_result_fetcher_(this),
      fetcher_backoff_(&kBackoffPolicy),
      fetcher_retries_(0),
      source_(source),
      external_cc_result_fetched_(false),
      list_accounts_stale_(true) {}

GaiaCookieManagerService::~GaiaCookieManagerService() {
  CancelAll();
  DCHECK(requests_.empty());
}

void GaiaCookieManagerService::Init() {
  cookie_changed_subscription_ = signin_client_->AddCookieChangedCallback(
      GaiaUrls::GetInstance()->google_url(), kGaiaCookieName,
      base::Bind(&GaiaCookieManagerService::OnCookieChanged,
                 base::Unretained(this)));
}

void GaiaCookieManagerService::Shutdown() {
  cookie_changed_subscription_.reset();
}


void GaiaCookieManagerService::AddAccountToCookieInternal(
    const std::string& account_id) {
  DCHECK(!account_id.empty());
  if (!signin_client_->AreSigninCookiesAllowed()) {
    SignalComplete(account_id,
        GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
    return;
  }

  requests_.push_back(GaiaCookieRequest::CreateAddAccountRequest(account_id));
  if (requests_.size() == 1) {
    signin_client_->DelayNetworkCall(
        base::Bind(&GaiaCookieManagerService::StartFetchingUbertoken,
                   base::Unretained(this)));
  }
}

void GaiaCookieManagerService::AddAccountToCookie(
    const std::string& account_id) {
  VLOG(1) << "GaiaCookieManagerService::AddAccountToCookie: " << account_id;
  access_token_ = std::string();
  AddAccountToCookieInternal(account_id);
}

void GaiaCookieManagerService::AddAccountToCookieWithToken(
    const std::string& account_id,
    const std::string& access_token) {
  VLOG(1) << "GaiaCookieManagerService::AddAccountToCookieWithToken: "
          << account_id;
  DCHECK(!access_token.empty());
  access_token_ = access_token;
  AddAccountToCookieInternal(account_id);
}

bool GaiaCookieManagerService::ListAccounts(
    std::vector<gaia::ListedAccount>* accounts,
    std::vector<gaia::ListedAccount>* signed_out_accounts) {
  if (!list_accounts_stale_) {
    if (accounts)
      accounts->assign(listed_accounts_.begin(), listed_accounts_.end());

    if (signed_out_accounts) {
      signed_out_accounts->assign(signed_out_accounts_.begin(),
                                  signed_out_accounts_.end());
    }

    return true;
  }

  TriggerListAccounts();
  return false;
}

void GaiaCookieManagerService::TriggerListAccounts() {
  if (requests_.empty()) {
    fetcher_retries_ = 0;
    requests_.push_back(GaiaCookieRequest::CreateListAccountsRequest());
    signin_client_->DelayNetworkCall(
        base::Bind(&GaiaCookieManagerService::StartFetchingListAccounts,
                   base::Unretained(this)));
  } else if (std::find_if(requests_.begin(), requests_.end(),
                          [](const GaiaCookieRequest& request) {
                            return request.request_type() == LIST_ACCOUNTS;
                          }) == requests_.end()) {
    requests_.push_back(GaiaCookieRequest::CreateListAccountsRequest());
  }
}

void GaiaCookieManagerService::ForceOnCookieChangedProcessing() {
  GURL google_url = GaiaUrls::GetInstance()->google_url();
  net::CanonicalCookie cookie(
      google_url, kGaiaCookieName, "", google_url.host(), "", base::Time(),
      base::Time(), base::Time(), false, false,
      net::CookieSameSite::DEFAULT_MODE, net::COOKIE_PRIORITY_DEFAULT);
  OnCookieChanged(cookie, true);
}

void GaiaCookieManagerService::LogOutAllAccounts() {
  VLOG(1) << "GaiaCookieManagerService::LogOutAllAccounts";

  bool log_out_queued = false;
  if (!requests_.empty()) {
    // Track requests to keep; all other unstarted requests will be removed.
    std::vector<GaiaCookieRequest> requests_to_keep;

    // Check all pending, non-executing requests.
    for (auto it = requests_.begin() + 1; it != requests_.end(); ++it) {
      if (it->request_type() == GaiaCookieRequestType::ADD_ACCOUNT) {
        // We have a pending log in request for an account followed by
        // a signout.
        GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
        SignalComplete(it->account_id(), error);
      }

      // Keep all requests except for ADD_ACCOUNTS.
      if (it->request_type() != GaiaCookieRequestType::ADD_ACCOUNT)
        requests_to_keep.push_back(*it);

      // Verify a LOG_OUT isn't already queued.
      if (it->request_type() == GaiaCookieRequestType::LOG_OUT)
        log_out_queued = true;
    }

    // Verify a LOG_OUT isn't currently being processed.
    if (requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT)
      log_out_queued = true;

    // Remove all but the executing request. Re-add all requests being kept.
    if (requests_.size() > 1) {
      requests_.erase(requests_.begin() + 1, requests_.end());
      requests_.insert(
          requests_.end(), requests_to_keep.begin(), requests_to_keep.end());
    }
  }

  if (!log_out_queued) {
    requests_.push_back(GaiaCookieRequest::CreateLogOutRequest());
    if (requests_.size() == 1) {
      fetcher_retries_ = 0;
      signin_client_->DelayNetworkCall(
          base::Bind(&GaiaCookieManagerService::StartFetchingLogOut,
                     base::Unretained(this)));
    }
  }
}

void GaiaCookieManagerService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void GaiaCookieManagerService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void GaiaCookieManagerService::CancelAll() {
  VLOG(1) << "GaiaCookieManagerService::CancelAll";
  gaia_auth_fetcher_.reset();
  uber_token_fetcher_.reset();
  requests_.clear();
  fetcher_timer_.Stop();
}

void GaiaCookieManagerService::OnCookieChanged(
    const net::CanonicalCookie& cookie,
    bool removed) {
  DCHECK_EQ(kGaiaCookieName, cookie.Name());
  DCHECK_EQ(GaiaUrls::GetInstance()->google_url().host(), cookie.Domain());
  list_accounts_stale_ = true;
  // Ignore changes to the cookie while requests are pending.  These changes
  // are caused by the service itself as it adds accounts.  A side effects is
  // that any changes to the gaia cookie outside of this class, while requests
  // are pending, will be lost.  However, trying to process these changes could
  // cause an endless loop (see crbug.com/516070).
  if (requests_.empty()) {
    requests_.push_back(GaiaCookieRequest::CreateListAccountsRequest());
    fetcher_retries_ = 0;
    signin_client_->DelayNetworkCall(
        base::Bind(&GaiaCookieManagerService::StartFetchingListAccounts,
                   base::Unretained(this)));
  }
}

void GaiaCookieManagerService::SignalComplete(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  // Its possible for the observer to delete |this| object.  Don't access
  // access any members after this calling the observer.  This method should
  // be the last call in any other method.
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnAddAccountToCookieCompleted(account_id, error));
}

void GaiaCookieManagerService::OnUbertokenSuccess(
    const std::string& uber_token) {
  DCHECK(requests_.front().request_type() ==
      GaiaCookieRequestType::ADD_ACCOUNT);
  VLOG(1) << "GaiaCookieManagerService::OnUbertokenSuccess"
          << " account=" << requests_.front().account_id();
  fetcher_retries_ = 0;
  uber_token_ = uber_token;

  if (!external_cc_result_fetched_ &&
      !external_cc_result_fetcher_.IsRunning()) {
    external_cc_result_fetcher_.Start();
    return;
  }

  signin_client_->DelayNetworkCall(
      base::Bind(&GaiaCookieManagerService::StartFetchingMergeSession,
                 base::Unretained(this)));
}

void GaiaCookieManagerService::OnUbertokenFailure(
    const GoogleServiceAuthError& error) {
  // Note that the UberToken fetcher already retries transient errors.
  VLOG(1) << "Failed to retrieve ubertoken"
          << " account=" << requests_.front().account_id()
          << " error=" << error.ToString();
  const std::string account_id = requests_.front().account_id();
  HandleNextRequest();
  SignalComplete(account_id, error);
}

void GaiaCookieManagerService::OnMergeSessionSuccess(const std::string& data) {
  VLOG(1) << "MergeSession successful account="
          << requests_.front().account_id();
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::ADD_ACCOUNT);
  const std::string account_id = requests_.front().account_id();
  HandleNextRequest();
  SignalComplete(account_id, GoogleServiceAuthError::AuthErrorNone());

  fetcher_backoff_.InformOfRequest(true);
  uber_token_ = std::string();
}

void GaiaCookieManagerService::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::ADD_ACCOUNT);
  VLOG(1) << "Failed MergeSession"
          << " account=" << requests_.front().account_id()
          << " error=" << error.ToString();
  if (++fetcher_retries_ < kMaxFetcherRetries && error.IsTransientError()) {
    fetcher_backoff_.InformOfRequest(false);
    UMA_HISTOGRAM_ENUMERATION("OAuth2Login.MergeSessionRetry",
        error.state(), GoogleServiceAuthError::NUM_STATES);
    fetcher_timer_.Start(
        FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
        base::Bind(&SigninClient::DelayNetworkCall,
                   base::Unretained(signin_client_),
                   base::Bind(
                       &GaiaCookieManagerService::StartFetchingMergeSession,
                       base::Unretained(this))));
    return;
  }

  uber_token_ = std::string();
  const std::string account_id = requests_.front().account_id();

  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.MergeSessionFailure",
      error.state(), GoogleServiceAuthError::NUM_STATES);
  HandleNextRequest();
  SignalComplete(account_id, error);
}

void GaiaCookieManagerService::OnListAccountsSuccess(const std::string& data) {
  VLOG(1) << "ListAccounts successful";
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::LIST_ACCOUNTS);
  fetcher_backoff_.InformOfRequest(true);

  if (!gaia::ParseListAccountsData(
          data, &listed_accounts_, &signed_out_accounts_)) {
    listed_accounts_.clear();
    signed_out_accounts_.clear();
    OnListAccountsFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE));
    return;
  }

  for (gaia::ListedAccount& account : listed_accounts_) {
    DCHECK(account.id.empty());
    account.id = AccountTrackerService::PickAccountIdForAccount(
        signin_client_->GetPrefs(), account.gaia_id, account.email);
  }

  list_accounts_stale_ = false;
  HandleNextRequest();
  // HandleNextRequest before sending out the notification because some
  // services, in response to OnGaiaAccountsInCookieUpdated, may try in return
  // to call ListAccounts, which would immediately return false if the
  // ListAccounts request is still sitting in queue.
  FOR_EACH_OBSERVER(Observer, observer_list_,
      OnGaiaAccountsInCookieUpdated(
          listed_accounts_,
          signed_out_accounts_,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE)));
}

void GaiaCookieManagerService::OnListAccountsFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "ListAccounts failed";
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::LIST_ACCOUNTS);
  if (++fetcher_retries_ < kMaxFetcherRetries && error.IsTransientError()) {
    fetcher_backoff_.InformOfRequest(false);
    UMA_HISTOGRAM_ENUMERATION("Signin.ListAccountsRetry",
        error.state(), GoogleServiceAuthError::NUM_STATES);
    fetcher_timer_.Start(
        FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
        base::Bind(&SigninClient::DelayNetworkCall,
                   base::Unretained(signin_client_),
                   base::Bind(
                       &GaiaCookieManagerService::StartFetchingListAccounts,
                       base::Unretained(this))));
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Signin.ListAccountsFailure",
      error.state(), GoogleServiceAuthError::NUM_STATES);
  FOR_EACH_OBSERVER(Observer, observer_list_,
      OnGaiaAccountsInCookieUpdated(
            listed_accounts_, signed_out_accounts_, error));
  HandleNextRequest();
}

void GaiaCookieManagerService::OnLogOutSuccess() {
  DCHECK(requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT);
  VLOG(1) << "GaiaCookieManagerService::OnLogOutSuccess";

  fetcher_backoff_.InformOfRequest(true);
  HandleNextRequest();
}

void GaiaCookieManagerService::OnLogOutFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT);
  VLOG(1) << "GaiaCookieManagerService::OnLogOutFailure";

  if (++fetcher_retries_ < kMaxFetcherRetries) {
    fetcher_backoff_.InformOfRequest(false);
    fetcher_timer_.Start(
        FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
        base::Bind(&SigninClient::DelayNetworkCall,
                   base::Unretained(signin_client_),
                   base::Bind(&GaiaCookieManagerService::StartFetchingLogOut,
                              base::Unretained(this))));
    return;
  }

  HandleNextRequest();
}

void GaiaCookieManagerService::StartFetchingUbertoken() {
  VLOG(1) << "GaiaCookieManagerService::StartFetchingUbertoken account_id="
          << requests_.front().account_id();
  uber_token_fetcher_.reset(new UbertokenFetcher(
      token_service_, this, source_, signin_client_->GetURLRequestContext(),
      base::Bind(&SigninClient::CreateGaiaAuthFetcher,
                 base::Unretained(signin_client_))));
  if (access_token_.empty()) {
    uber_token_fetcher_->StartFetchingToken(requests_.front().account_id());
  } else {
    uber_token_fetcher_->StartFetchingTokenWithAccessToken(
        requests_.front().account_id(), access_token_);
  }
}

void GaiaCookieManagerService::StartFetchingMergeSession() {
  DCHECK(!uber_token_.empty());
  gaia_auth_fetcher_.reset(signin_client_->CreateGaiaAuthFetcher(
      this, source_, signin_client_->GetURLRequestContext()));

  gaia_auth_fetcher_->StartMergeSession(uber_token_,
      external_cc_result_fetcher_.GetExternalCcResult());
}

void GaiaCookieManagerService::StartFetchingLogOut() {
  DCHECK(requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT);
  VLOG(1) << "GaiaCookieManagerService::StartFetchingLogOut";
  gaia_auth_fetcher_.reset(signin_client_->CreateGaiaAuthFetcher(
      this, source_, signin_client_->GetURLRequestContext()));
  gaia_auth_fetcher_->StartLogOut();
}

void GaiaCookieManagerService::StartFetchingListAccounts() {
  VLOG(1) << "GaiaCookieManagerService::ListAccounts";
  gaia_auth_fetcher_.reset(signin_client_->CreateGaiaAuthFetcher(
      this, source_, signin_client_->GetURLRequestContext()));
  gaia_auth_fetcher_->StartListAccounts();
}

void GaiaCookieManagerService::HandleNextRequest() {
  VLOG(1) << "GaiaCookieManagerService::HandleNextRequest";
  if (requests_.front().request_type() ==
      GaiaCookieRequestType::LIST_ACCOUNTS) {
    // This and any directly subsequent list accounts would return the same.
    while (!requests_.empty() && requests_.front().request_type() ==
           GaiaCookieRequestType::LIST_ACCOUNTS) {
      requests_.pop_front();
    }
  } else {
    // Pop the completed request.
    requests_.pop_front();
  }

  gaia_auth_fetcher_.reset();
  fetcher_retries_ = 0;
  if (requests_.empty()) {
    VLOG(1) << "GaiaCookieManagerService::HandleNextRequest: no more";
    uber_token_fetcher_.reset();
    access_token_ = std::string();
  } else {
    switch (requests_.front().request_type()) {
      case GaiaCookieRequestType::ADD_ACCOUNT:
        signin_client_->DelayNetworkCall(
            base::Bind(&GaiaCookieManagerService::StartFetchingUbertoken,
                       base::Unretained(this)));
        break;
      case GaiaCookieRequestType::LOG_OUT:
        signin_client_->DelayNetworkCall(
            base::Bind(&GaiaCookieManagerService::StartFetchingLogOut,
                       base::Unretained(this)));
        break;
      case GaiaCookieRequestType::LIST_ACCOUNTS:
        uber_token_fetcher_.reset();
        signin_client_->DelayNetworkCall(
            base::Bind(&GaiaCookieManagerService::StartFetchingListAccounts,
                       base::Unretained(this)));
        break;
    };
  }
}
