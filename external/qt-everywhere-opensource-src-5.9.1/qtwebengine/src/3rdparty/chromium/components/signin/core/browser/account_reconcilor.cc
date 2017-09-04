// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_reconcilor.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"

namespace {

// String used for source parameter in GAIA cookie manager calls.
const char kSource[] = "ChromiumAccountReconcilor";

class AccountEqualToFunc {
 public:
  explicit AccountEqualToFunc(const gaia::ListedAccount& account)
      : account_(account) {}
  bool operator()(const gaia::ListedAccount& other) const;

 private:
  gaia::ListedAccount account_;
};

bool AccountEqualToFunc::operator()(const gaia::ListedAccount& other) const {
  return account_.valid == other.valid && account_.id == other.id;
}

gaia::ListedAccount AccountForId(const std::string& account_id) {
  gaia::ListedAccount account;
  account.id = account_id;
  account.gaia_id = std::string();
  account.email = std::string();
  account.valid = true;
  return account;
}

}  // namespace


AccountReconcilor::AccountReconcilor(
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    SigninClient* client,
    GaiaCookieManagerService* cookie_manager_service)
    : token_service_(token_service),
      signin_manager_(signin_manager),
      client_(client),
      cookie_manager_service_(cookie_manager_service),
      registered_with_token_service_(false),
      registered_with_cookie_manager_service_(false),
      registered_with_content_settings_(false),
      is_reconcile_started_(false),
      first_execution_(true),
      error_during_last_reconcile_(false),
      chrome_accounts_changed_(false) {
  VLOG(1) << "AccountReconcilor::AccountReconcilor";
}

AccountReconcilor::~AccountReconcilor() {
  VLOG(1) << "AccountReconcilor::~AccountReconcilor";
  // Make sure shutdown was called first.
  DCHECK(!registered_with_token_service_);
  DCHECK(!registered_with_cookie_manager_service_);
}

void AccountReconcilor::Initialize(bool start_reconcile_if_tokens_available) {
  VLOG(1) << "AccountReconcilor::Initialize";
  RegisterWithSigninManager();

  // If this user is not signed in, the reconcilor should do nothing but
  // wait for signin.
  if (IsProfileConnected()) {
    RegisterWithCookieManagerService();
    RegisterWithContentSettings();
    RegisterWithTokenService();

    // Start a reconcile if the tokens are already loaded.
    if (start_reconcile_if_tokens_available &&
        token_service_->GetAccounts().size() > 0) {
      StartReconcile();
    }
  }
}

void AccountReconcilor::Shutdown() {
  VLOG(1) << "AccountReconcilor::Shutdown";
  UnregisterWithCookieManagerService();
  UnregisterWithSigninManager();
  UnregisterWithTokenService();
  UnregisterWithContentSettings();
}

void AccountReconcilor::RegisterWithSigninManager() {
  signin_manager_->AddObserver(this);
}

void AccountReconcilor::UnregisterWithSigninManager() {
  signin_manager_->RemoveObserver(this);
}

void AccountReconcilor::RegisterWithContentSettings() {
  VLOG(1) << "AccountReconcilor::RegisterWithContentSettings";
  // During re-auth, the reconcilor will get a callback about successful signin
  // even when the profile is already connected.  Avoid re-registering
  // with the token service since this will DCHECK.
  if (registered_with_content_settings_)
    return;

  client_->AddContentSettingsObserver(this);
  registered_with_content_settings_ = true;
}

void AccountReconcilor::UnregisterWithContentSettings() {
  VLOG(1) << "AccountReconcilor::UnregisterWithContentSettings";
  if (!registered_with_content_settings_)
    return;

  client_->RemoveContentSettingsObserver(this);
  registered_with_content_settings_ = false;
}

void AccountReconcilor::RegisterWithTokenService() {
  VLOG(1) << "AccountReconcilor::RegisterWithTokenService";
  // During re-auth, the reconcilor will get a callback about successful signin
  // even when the profile is already connected.  Avoid re-registering
  // with the token service since this will DCHECK.
  if (registered_with_token_service_)
    return;

  token_service_->AddObserver(this);
  registered_with_token_service_ = true;
}

void AccountReconcilor::UnregisterWithTokenService() {
  VLOG(1) << "AccountReconcilor::UnregisterWithTokenService";
  if (!registered_with_token_service_)
    return;

  token_service_->RemoveObserver(this);
  registered_with_token_service_ = false;
}

void AccountReconcilor::RegisterWithCookieManagerService() {
  VLOG(1) << "AccountReconcilor::RegisterWithCookieManagerService";
  // During re-auth, the reconcilor will get a callback about successful signin
  // even when the profile is already connected.  Avoid re-registering
  // with the helper since this will DCHECK.
  if (registered_with_cookie_manager_service_)
    return;

  cookie_manager_service_->AddObserver(this);
  registered_with_cookie_manager_service_ = true;
}
void AccountReconcilor::UnregisterWithCookieManagerService() {
  VLOG(1) << "AccountReconcilor::UnregisterWithCookieManagerService";
  if (!registered_with_cookie_manager_service_)
    return;

  cookie_manager_service_->RemoveObserver(this);
  registered_with_cookie_manager_service_ = false;
}

bool AccountReconcilor::IsProfileConnected() {
  return signin_manager_->IsAuthenticated();
}

signin_metrics::AccountReconcilorState AccountReconcilor::GetState() {
  if (!is_reconcile_started_) {
    return error_during_last_reconcile_
               ? signin_metrics::ACCOUNT_RECONCILOR_ERROR
               : signin_metrics::ACCOUNT_RECONCILOR_OK;
  }

  return signin_metrics::ACCOUNT_RECONCILOR_RUNNING;
}

void AccountReconcilor::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  // If this is not a change to cookie settings, just ignore.
  if (content_type != CONTENT_SETTINGS_TYPE_COOKIES)
    return;

  // If this does not affect GAIA, just ignore.  If the primary pattern is
  // invalid, then assume it could affect GAIA.  The secondary pattern is
  // not needed.
  if (primary_pattern.IsValid() &&
      !primary_pattern.Matches(GaiaUrls::GetInstance()->gaia_url())) {
    return;
  }

  VLOG(1) << "AccountReconcilor::OnContentSettingChanged";
  StartReconcile();
}

void AccountReconcilor::OnEndBatchChanges() {
  VLOG(1) << "AccountReconcilor::OnEndBatchChanges. "
          << "Reconcilor state: " << is_reconcile_started_;
  // Remember that accounts have changed if a reconcile is already started.
  chrome_accounts_changed_ = is_reconcile_started_;
  StartReconcile();
}

void AccountReconcilor::GoogleSigninSucceeded(const std::string& account_id,
                                              const std::string& username,
                                              const std::string& password) {
  VLOG(1) << "AccountReconcilor::GoogleSigninSucceeded: signed in";
  RegisterWithCookieManagerService();
  RegisterWithContentSettings();
  RegisterWithTokenService();
}

void AccountReconcilor::GoogleSignedOut(const std::string& account_id,
                                        const std::string& username) {
  VLOG(1) << "AccountReconcilor::GoogleSignedOut: signed out";
  AbortReconcile();
  UnregisterWithCookieManagerService();
  UnregisterWithTokenService();
  UnregisterWithContentSettings();
  PerformLogoutAllAccountsAction();
}

void AccountReconcilor::PerformMergeAction(const std::string& account_id) {
  if (!switches::IsEnableAccountConsistency()) {
    MarkAccountAsAddedToCookie(account_id);
    return;
  }
  VLOG(1) << "AccountReconcilor::PerformMergeAction: " << account_id;
  cookie_manager_service_->AddAccountToCookie(account_id, kSource);
}

void AccountReconcilor::PerformLogoutAllAccountsAction() {
  if (!switches::IsEnableAccountConsistency())
    return;
  VLOG(1) << "AccountReconcilor::PerformLogoutAllAccountsAction";
  cookie_manager_service_->LogOutAllAccounts(kSource);
}

void AccountReconcilor::StartReconcile() {
  reconcile_start_time_ = base::Time::Now();

  if (!IsProfileConnected() || !client_->AreSigninCookiesAllowed()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: !connected or no cookies";
    return;
  }

  if (is_reconcile_started_)
    return;

  // Reset state for validating gaia cookie.
  gaia_accounts_.clear();

  // Reset state for validating oauth2 tokens.
  primary_account_.clear();
  chrome_accounts_.clear();
  add_to_cookie_.clear();
  ValidateAccountsFromTokenService();

  if (primary_account_.empty()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: primary has error";
    return;
  }

  is_reconcile_started_ = true;
  error_during_last_reconcile_ = false;

  // ListAccounts() also gets signed out accounts but this class doesn't use
  // them.
  std::vector<gaia::ListedAccount> signed_out_accounts;

  // Rely on the GCMS to manage calls to and responses from ListAccounts.
  if (cookie_manager_service_->ListAccounts(&gaia_accounts_,
                                            &signed_out_accounts,
                                            kSource)) {
    OnGaiaAccountsInCookieUpdated(
        gaia_accounts_,
        signed_out_accounts,
        GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }
}

void AccountReconcilor::OnGaiaAccountsInCookieUpdated(
        const std::vector<gaia::ListedAccount>& accounts,
        const std::vector<gaia::ListedAccount>& signed_out_accounts,
        const GoogleServiceAuthError& error) {
  VLOG(1) << "AccountReconcilor::OnGaiaAccountsInCookieUpdated: "
          << "CookieJar " << accounts.size() << " accounts, "
          << "Reconcilor's state is " << is_reconcile_started_ << ", "
          << "Error was " << error.ToString();
  if (error.state() == GoogleServiceAuthError::NONE) {
    gaia_accounts_ = accounts;

    // It is possible that O2RT is not available at this moment.
    if (token_service_->GetAccounts().empty())
      return;

    is_reconcile_started_ ? FinishReconcile() : StartReconcile();
  } else {
    if (is_reconcile_started_)
      error_during_last_reconcile_ = true;
    AbortReconcile();
  }
}

void AccountReconcilor::ValidateAccountsFromTokenService() {
  primary_account_ = signin_manager_->GetAuthenticatedAccountId();
  DCHECK(!primary_account_.empty());

  chrome_accounts_ = token_service_->GetAccounts();

  // Remove any accounts that have an error.  There is no point in trying to
  // reconcile them, since it won't work anyway.  If the list ends up being
  // empty, or if the primary account is in error, then don't reconcile any
  // accounts.
  for (auto i = chrome_accounts_.begin(); i != chrome_accounts_.end(); ++i) {
    if (token_service_->GetDelegate()->RefreshTokenHasError(*i)) {
      if (primary_account_ == *i) {
        primary_account_.clear();
        chrome_accounts_.clear();
        break;
      } else {
        VLOG(1) << "AccountReconcilor::ValidateAccountsFromTokenService: "
                << *i << " has error, won't reconcile";
        i->clear();
      }
    }
  }
  chrome_accounts_.erase(std::remove(chrome_accounts_.begin(),
                                     chrome_accounts_.end(),
                                     std::string()),
                         chrome_accounts_.end());

  VLOG(1) << "AccountReconcilor::ValidateAccountsFromTokenService: "
          << "Chrome " << chrome_accounts_.size() << " accounts, "
          << "Primary is '" << primary_account_ << "'";
}

void AccountReconcilor::OnNewProfileManagementFlagChanged(
    bool new_flag_status) {
  if (new_flag_status) {
    // The reconciler may have been newly created just before this call, or may
    // have already existed and in mid-reconcile. To err on the safe side, force
    // a restart.
    Shutdown();
    Initialize(true);
  } else {
    Shutdown();
  }
}

void AccountReconcilor::OnReceivedManageAccountsResponse(
    signin::GAIAServiceType service_type) {
  if (service_type == signin::GAIA_SERVICE_TYPE_ADDSESSION) {
    cookie_manager_service_->TriggerListAccounts(kSource);
  }
}

void AccountReconcilor::FinishReconcile() {
  VLOG(1) << "AccountReconcilor::FinishReconcile";
  DCHECK(add_to_cookie_.empty());
  int number_gaia_accounts = gaia_accounts_.size();
  bool are_primaries_equal = number_gaia_accounts > 0 &&
      primary_account_ == gaia_accounts_[0].id;

  // If there are any accounts in the gaia cookie but not in chrome, then
  // those accounts need to be removed from the cookie.  This means we need
  // to blow the cookie away.
  int removed_from_cookie = 0;
  for (size_t i = 0; i < gaia_accounts_.size(); ++i) {
    if (gaia_accounts_[i].valid &&
        chrome_accounts_.end() == std::find(chrome_accounts_.begin(),
                                            chrome_accounts_.end(),
                                            gaia_accounts_[i].id)) {
      ++removed_from_cookie;
    }
  }

  bool rebuild_cookie = !are_primaries_equal || removed_from_cookie > 0;
  std::vector<gaia::ListedAccount> original_gaia_accounts =
      gaia_accounts_;
  if (rebuild_cookie) {
    VLOG(1) << "AccountReconcilor::FinishReconcile: rebuild cookie";
    // Really messed up state.  Blow away the gaia cookie completely and
    // rebuild it, making sure the primary account as specified by the
    // SigninManager is the first session in the gaia cookie.
    PerformLogoutAllAccountsAction();
    gaia_accounts_.clear();
  }

  // Create a list of accounts that need to be added to the gaia cookie.
  // The primary account must be first to make sure it becomes the default
  // account in the case where chrome is completely rebuilding the cookie.
  add_to_cookie_.push_back(primary_account_);
  for (size_t i = 0; i < chrome_accounts_.size(); ++i) {
    if (chrome_accounts_[i] != primary_account_)
      add_to_cookie_.push_back(chrome_accounts_[i]);
  }

  // For each account known to chrome, PerformMergeAction() if the account is
  // not already in the cookie jar or its state is invalid, or signal merge
  // completed otherwise.  Make a copy of |add_to_cookie_| since calls to
  // SignalComplete() will change the array.
  std::vector<std::string> add_to_cookie_copy = add_to_cookie_;
  int added_to_cookie = 0;
  for (size_t i = 0; i < add_to_cookie_copy.size(); ++i) {
    if (gaia_accounts_.end() !=
        std::find_if(gaia_accounts_.begin(), gaia_accounts_.end(),
                     AccountEqualToFunc(AccountForId(add_to_cookie_copy[i])))) {
      cookie_manager_service_->SignalComplete(
          add_to_cookie_copy[i],
          GoogleServiceAuthError::AuthErrorNone());
    } else {
      PerformMergeAction(add_to_cookie_copy[i]);
      if (original_gaia_accounts.end() ==
          std::find_if(
              original_gaia_accounts.begin(), original_gaia_accounts.end(),
              AccountEqualToFunc(AccountForId(add_to_cookie_copy[i])))) {
        added_to_cookie++;
      }
    }
  }

  signin_metrics::LogSigninAccountReconciliation(chrome_accounts_.size(),
                                                 added_to_cookie,
                                                 removed_from_cookie,
                                                 are_primaries_equal,
                                                 first_execution_,
                                                 number_gaia_accounts);
  first_execution_ = false;
  CalculateIfReconcileIsDone();
  ScheduleStartReconcileIfChromeAccountsChanged();
}

void AccountReconcilor::AbortReconcile() {
  VLOG(1) << "AccountReconcilor::AbortReconcile: we'll try again later";
  add_to_cookie_.clear();
  CalculateIfReconcileIsDone();
}

void AccountReconcilor::CalculateIfReconcileIsDone() {
  base::TimeDelta duration = base::Time::Now() - reconcile_start_time_;
  // Record the duration if reconciliation was underway and now it is over.
  if (is_reconcile_started_ && add_to_cookie_.empty()) {
    signin_metrics::LogSigninAccountReconciliationDuration(duration,
        !error_during_last_reconcile_);
  }

  is_reconcile_started_ = !add_to_cookie_.empty();
  if (!is_reconcile_started_)
    VLOG(1) << "AccountReconcilor::CalculateIfReconcileIsDone: done";
}

void AccountReconcilor::ScheduleStartReconcileIfChromeAccountsChanged() {
  if (is_reconcile_started_)
    return;

  // Start a reconcile as the token accounts have changed.
  VLOG(1) << "AccountReconcilor::StartReconcileIfChromeAccountsChanged";
  if (chrome_accounts_changed_) {
    chrome_accounts_changed_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&AccountReconcilor::StartReconcile, base::Unretained(this)));
  }
}

// Remove the account from the list that is being merged.
bool AccountReconcilor::MarkAccountAsAddedToCookie(
    const std::string& account_id) {
  for (std::vector<std::string>::iterator i = add_to_cookie_.begin();
       i != add_to_cookie_.end();
       ++i) {
    if (account_id == *i) {
      add_to_cookie_.erase(i);
      return true;
    }
  }
  return false;
}

void AccountReconcilor::OnAddAccountToCookieCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "AccountReconcilor::OnAddAccountToCookieCompleted: "
          << "Account added: " << account_id << ", "
          << "Error was " << error.ToString();
  // Always listens to GaiaCookieManagerService. Only proceed if reconciling.
  if (is_reconcile_started_ && MarkAccountAsAddedToCookie(account_id)) {
    if (error.state() != GoogleServiceAuthError::State::NONE)
      error_during_last_reconcile_ = true;
    CalculateIfReconcileIsDone();
    ScheduleStartReconcileIfChromeAccountsChanged();
  }
}
