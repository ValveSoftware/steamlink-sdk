// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_fetcher_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/profiler/scoped_tracker.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_info_fetcher.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/child_account_info_fetcher.h"
#include "components/signin/core/browser/refresh_token_annotation_request.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/common/signin_switches.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

const base::TimeDelta kRefreshFromTokenServiceDelay =
    base::TimeDelta::FromHours(24);

bool AccountSupportsUserInfo(const std::string& account_id) {
  // Supervised users use a specially scoped token which when used for general
  // purposes causes the token service to raise spurious auth errors.
  // TODO(treib): this string is also used in supervised_user_constants.cc.
  // Should put in a common place.
  return account_id != "managed_user@localhost";
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// IsRefreshTokenDeviceIdExperimentEnabled is called from
// SendRefreshTokenAnnotationRequest only on desktop platforms.
bool IsRefreshTokenDeviceIdExperimentEnabled() {
  const std::string group_name =
      base::FieldTrialList::FindFullName("RefreshTokenDeviceId");
  return group_name == "Enabled";
}
#endif

}

// This pref used to be in the AccountTrackerService, hence its string value.
const char AccountFetcherService::kLastUpdatePref[] =
    "account_tracker_service_last_update";

// AccountFetcherService implementation
AccountFetcherService::AccountFetcherService()
    : account_tracker_service_(nullptr),
      token_service_(nullptr),
      signin_client_(nullptr),
      invalidation_service_(nullptr),
      network_fetches_enabled_(false),
      profile_loaded_(false),
      refresh_tokens_loaded_(false),
      shutdown_called_(false),
      scheduled_refresh_enabled_(true),
      child_info_request_(nullptr) {}

AccountFetcherService::~AccountFetcherService() {
  DCHECK(shutdown_called_);
}

// static
void AccountFetcherService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterInt64Pref(kLastUpdatePref, 0);
}

void AccountFetcherService::Initialize(
    SigninClient* signin_client,
    OAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service) {
  DCHECK(signin_client);
  DCHECK(!signin_client_);
  signin_client_ = signin_client;
  DCHECK(account_tracker_service);
  DCHECK(!account_tracker_service_);
  account_tracker_service_ = account_tracker_service;
  DCHECK(token_service);
  DCHECK(!token_service_);
  token_service_ = token_service;
  token_service_->AddObserver(this);

  last_updated_ = base::Time::FromInternalValue(
      signin_client_->GetPrefs()->GetInt64(kLastUpdatePref));
}

void AccountFetcherService::Shutdown() {
  token_service_->RemoveObserver(this);
  // child_info_request_ is an invalidation handler and needs to be
  // unregistered during the lifetime of the invalidation service.
  child_info_request_.reset();
  invalidation_service_ = nullptr;
  shutdown_called_ = true;
}

bool AccountFetcherService::IsAllUserInfoFetched() const {
  return user_info_requests_.empty();
}

void AccountFetcherService::FetchUserInfoBeforeSignin(
    const std::string& account_id) {
  DCHECK(network_fetches_enabled_);
  RefreshAccountInfo(account_id, false);
}

void AccountFetcherService::SetupInvalidationsOnProfileLoad(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK(!invalidation_service_);
  DCHECK(!profile_loaded_);
  DCHECK(!network_fetches_enabled_);
  DCHECK(!child_info_request_);
  invalidation_service_ = invalidation_service;
  profile_loaded_ = true;
  MaybeEnableNetworkFetches();
}

void AccountFetcherService::EnableNetworkFetchesForTest() {
  SetupInvalidationsOnProfileLoad(nullptr);
  OnRefreshTokensLoaded();
}

void AccountFetcherService::RefreshAllAccountInfo(bool only_fetch_if_invalid) {
  std::vector<std::string> accounts = token_service_->GetAccounts();
  for (std::vector<std::string>::const_iterator it = accounts.begin();
       it != accounts.end(); ++it) {
    RefreshAccountInfo(*it, only_fetch_if_invalid);
  }
}

// Child account status is refreshed through invalidations which are only
// available for the primary account. Finding the primary account requires a
// dependency on signin_manager which we get around by only allowing a single
// account. This is possible since we only support a single account to be a
// child anyway.
void AccountFetcherService::UpdateChildInfo() {
  DCHECK(CalledOnValidThread());
  std::vector<std::string> accounts = token_service_->GetAccounts();
  if (accounts.size() == 1) {
    const std::string& candidate = accounts[0];
    if (candidate == child_request_account_id_)
      return;
    if (!child_request_account_id_.empty())
      ResetChildInfo();
    if (!AccountSupportsUserInfo(candidate))
      return;
    child_request_account_id_ = candidate;
    StartFetchingChildInfo(candidate);
  } else {
    ResetChildInfo();
  }
}

void AccountFetcherService::MaybeEnableNetworkFetches() {
  DCHECK(CalledOnValidThread());
  if (!profile_loaded_ || !refresh_tokens_loaded_)
    return;
  if (!network_fetches_enabled_) {
    network_fetches_enabled_ = true;
    ScheduleNextRefresh();
  }
  RefreshAllAccountInfo(true);
  UpdateChildInfo();
}

void AccountFetcherService::RefreshAllAccountsAndScheduleNext() {
  DCHECK(network_fetches_enabled_);
  RefreshAllAccountInfo(false);
  last_updated_ = base::Time::Now();
  signin_client_->GetPrefs()->SetInt64(kLastUpdatePref,
                                       last_updated_.ToInternalValue());
  ScheduleNextRefresh();
}

void AccountFetcherService::ScheduleNextRefresh() {
  if (!scheduled_refresh_enabled_)
    return;
  DCHECK(!timer_.IsRunning());
  DCHECK(network_fetches_enabled_);

  const base::TimeDelta time_since_update = base::Time::Now() - last_updated_;
  if(time_since_update > kRefreshFromTokenServiceDelay) {
    RefreshAllAccountsAndScheduleNext();
  } else {
    timer_.Start(FROM_HERE, kRefreshFromTokenServiceDelay - time_since_update,
                 this,
                 &AccountFetcherService::RefreshAllAccountsAndScheduleNext);
  }
}

// Starts fetching user information. This is called periodically to refresh.
void AccountFetcherService::StartFetchingUserInfo(
    const std::string& account_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(network_fetches_enabled_);

  if (!ContainsKey(user_info_requests_, account_id)) {
    DVLOG(1) << "StartFetching " << account_id;
    std::unique_ptr<AccountInfoFetcher> fetcher(new AccountInfoFetcher(
        token_service_, signin_client_->GetURLRequestContext(), this,
        account_id));
    user_info_requests_.set(account_id, std::move(fetcher));
    user_info_requests_.get(account_id)->Start();
  }
}

// Starts fetching whether this is a child account. Handles refresh internally.
void AccountFetcherService::StartFetchingChildInfo(
    const std::string& account_id) {
  child_info_request_.reset(ChildAccountInfoFetcher::CreateFrom(
      child_request_account_id_, this, token_service_,
      signin_client_->GetURLRequestContext(), invalidation_service_));
}

void AccountFetcherService::ResetChildInfo() {
  if (!child_request_account_id_.empty())
    SetIsChildAccount(child_request_account_id_, false);
  child_request_account_id_.clear();
  child_info_request_.reset();
}

void AccountFetcherService::RefreshAccountInfo(const std::string& account_id,
                                               bool only_fetch_if_invalid) {
  DCHECK(network_fetches_enabled_);
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 AccountFetcherService::RefreshAccountInfo"));

  account_tracker_service_->StartTrackingAccount(account_id);
  const AccountInfo& info =
      account_tracker_service_->GetAccountInfo(account_id);
  if (!AccountSupportsUserInfo(account_id))
    return;

// |only_fetch_if_invalid| is false when the service is due for a timed update.
#if defined(OS_ANDROID)
  // TODO(mlerman): Change this condition back to info.IsValid() and ensure the
  // Fetch doesn't occur until after ProfileImpl::OnPrefsLoaded().
  if (!only_fetch_if_invalid || info.gaia.empty())
#else
  if (!only_fetch_if_invalid || !info.IsValid())
#endif
    StartFetchingUserInfo(account_id);

  SendRefreshTokenAnnotationRequest(account_id);
}

void AccountFetcherService::SendRefreshTokenAnnotationRequest(
    const std::string& account_id) {
// We only need to send RefreshTokenAnnotationRequest from desktop platforms.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (IsRefreshTokenDeviceIdExperimentEnabled() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableRefreshTokenAnnotationRequest)) {
    std::unique_ptr<RefreshTokenAnnotationRequest> request =
        RefreshTokenAnnotationRequest::SendIfNeeded(
            signin_client_->GetPrefs(), token_service_, signin_client_,
            signin_client_->GetURLRequestContext(), account_id,
            base::Bind(
                &AccountFetcherService::RefreshTokenAnnotationRequestDone,
                base::Unretained(this), account_id));
    // If request was sent AccountFetcherService needs to own request till it
    // finishes.
    if (request)
      refresh_token_annotation_requests_.set(account_id, std::move(request));
  }
#endif
}

void AccountFetcherService::RefreshTokenAnnotationRequestDone(
    const std::string& account_id) {
  refresh_token_annotation_requests_.erase(account_id);
}

void AccountFetcherService::OnUserInfoFetchSuccess(
    const std::string& account_id,
    std::unique_ptr<base::DictionaryValue> user_info) {
  account_tracker_service_->SetAccountStateFromUserInfo(account_id,
                                                        user_info.get());
  user_info_requests_.erase(account_id);
}

void AccountFetcherService::SetIsChildAccount(const std::string& account_id,
                                              bool is_child_account) {
  if (child_request_account_id_ == account_id)
    account_tracker_service_->SetIsChildAccount(account_id, is_child_account);
}

void AccountFetcherService::OnUserInfoFetchFailure(
    const std::string& account_id) {
  LOG(WARNING) << "Failed to get UserInfo for " << account_id;
  account_tracker_service_->NotifyAccountUpdateFailed(account_id);
  user_info_requests_.erase(account_id);
}

void AccountFetcherService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  TRACE_EVENT1("AccountFetcherService",
               "AccountFetcherService::OnRefreshTokenAvailable",
               "account_id",
               account_id);
  DVLOG(1) << "AVAILABLE " << account_id;

  // The SigninClient needs a "final init" in order to perform some actions
  // (such as fetching the signin token "handle" in order to look for password
  // changes) once everything is initialized and the refresh token is present.
  signin_client_->DoFinalInit();

  if (!network_fetches_enabled_)
    return;
  RefreshAccountInfo(account_id, true);
  UpdateChildInfo();
}

void AccountFetcherService::OnRefreshTokenRevoked(
    const std::string& account_id) {
  TRACE_EVENT1("AccountFetcherService",
               "AccountFetcherService::OnRefreshTokenRevoked",
               "account_id",
               account_id);
  DVLOG(1) << "REVOKED " << account_id;

  if (!network_fetches_enabled_)
    return;
  user_info_requests_.erase(account_id);
  UpdateChildInfo();
  account_tracker_service_->StopTrackingAccount(account_id);
}

void AccountFetcherService::OnRefreshTokensLoaded() {
  DCHECK(CalledOnValidThread());
  refresh_tokens_loaded_ = true;
  MaybeEnableNetworkFetches();
}
