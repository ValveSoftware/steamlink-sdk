// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#endif

namespace extensions {

namespace identity_constants {
const char kInvalidClientId[] = "Invalid OAuth2 Client ID.";
const char kInvalidScopes[] = "Invalid OAuth2 scopes.";
const char kAuthFailure[] = "OAuth2 request failed: ";
const char kNoGrant[] = "OAuth2 not granted or revoked.";
const char kUserRejected[] = "The user did not approve access.";
const char kUserNotSignedIn[] = "The user is not signed in.";
const char kInteractionRequired[] = "User interaction required.";
const char kInvalidRedirect[] = "Did not redirect to the right URL.";
const char kOffTheRecord[] = "Identity API is disabled in incognito windows.";
const char kPageLoadFailure[] = "Authorization page could not be loaded.";
const char kCanceled[] = "canceled";

const int kCachedIssueAdviceTTLSeconds = 1;
}  // namespace identity_constants

namespace {

static const char kChromiumDomainRedirectUrlPattern[] =
    "https://%s.chromiumapp.org/";

#if defined(OS_CHROMEOS)
// The list of apps that are allowed to use the Identity API to retrieve the
// token from the device robot account in a public session.
const char* const kPublicSessionAllowedOrigins[] = {
    // Chrome Remote Desktop - Chromium branding.
    "chrome-extension://ljacajndfccfgnfohlgkdphmbnpkjflk/",
    // Chrome Remote Desktop - Official branding.
    "chrome-extension://gbchcmhmhahfdphkhkmpfmihenigjmpp/"};
#endif

std::string GetPrimaryAccountId(content::BrowserContext* context) {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(Profile::FromBrowserContext(context));
  return signin_manager->GetAuthenticatedAccountId();
}

}  // namespace

namespace identity = api::identity;

IdentityTokenCacheValue::IdentityTokenCacheValue()
    : status_(CACHE_STATUS_NOTFOUND) {}

IdentityTokenCacheValue::IdentityTokenCacheValue(
    const IssueAdviceInfo& issue_advice)
    : status_(CACHE_STATUS_ADVICE), issue_advice_(issue_advice) {
  expiration_time_ =
      base::Time::Now() + base::TimeDelta::FromSeconds(
                              identity_constants::kCachedIssueAdviceTTLSeconds);
}

IdentityTokenCacheValue::IdentityTokenCacheValue(const std::string& token,
                                                 base::TimeDelta time_to_live)
    : status_(CACHE_STATUS_TOKEN), token_(token) {
  // Remove 20 minutes from the ttl so cached tokens will have some time
  // to live any time they are returned.
  time_to_live -= base::TimeDelta::FromMinutes(20);

  base::TimeDelta zero_delta;
  if (time_to_live < zero_delta)
    time_to_live = zero_delta;

  expiration_time_ = base::Time::Now() + time_to_live;
}

IdentityTokenCacheValue::IdentityTokenCacheValue(
    const IdentityTokenCacheValue& other) = default;

IdentityTokenCacheValue::~IdentityTokenCacheValue() {}

IdentityTokenCacheValue::CacheValueStatus IdentityTokenCacheValue::status()
    const {
  if (is_expired())
    return IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND;
  else
    return status_;
}

const IssueAdviceInfo& IdentityTokenCacheValue::issue_advice() const {
  return issue_advice_;
}

const std::string& IdentityTokenCacheValue::token() const { return token_; }

bool IdentityTokenCacheValue::is_expired() const {
  return status_ == CACHE_STATUS_NOTFOUND ||
         expiration_time_ < base::Time::Now();
}

const base::Time& IdentityTokenCacheValue::expiration_time() const {
  return expiration_time_;
}

IdentityAPI::IdentityAPI(content::BrowserContext* context)
    : browser_context_(context),
      profile_identity_provider_(
          SigninManagerFactory::GetForProfile(
              Profile::FromBrowserContext(context)),
          ProfileOAuth2TokenServiceFactory::GetForProfile(
              Profile::FromBrowserContext(context)),
          LoginUIServiceFactory::GetShowLoginPopupCallbackForProfile(
              Profile::FromBrowserContext(context))),
      account_tracker_(&profile_identity_provider_,
                       g_browser_process->system_request_context()) {
  account_tracker_.AddObserver(this);
}

IdentityAPI::~IdentityAPI() {}

IdentityMintRequestQueue* IdentityAPI::mint_queue() { return &mint_queue_; }

void IdentityAPI::SetCachedToken(const ExtensionTokenKey& key,
                                 const IdentityTokenCacheValue& token_data) {
  CachedTokens::iterator it = token_cache_.find(key);
  if (it != token_cache_.end() && it->second.status() <= token_data.status())
    token_cache_.erase(it);

  token_cache_.insert(std::make_pair(key, token_data));
}

void IdentityAPI::EraseCachedToken(const std::string& extension_id,
                                   const std::string& token) {
  CachedTokens::iterator it;
  for (it = token_cache_.begin(); it != token_cache_.end(); ++it) {
    if (it->first.extension_id == extension_id &&
        it->second.status() == IdentityTokenCacheValue::CACHE_STATUS_TOKEN &&
        it->second.token() == token) {
      token_cache_.erase(it);
      break;
    }
  }
}

void IdentityAPI::EraseAllCachedTokens() { token_cache_.clear(); }

const IdentityTokenCacheValue& IdentityAPI::GetCachedToken(
    const ExtensionTokenKey& key) {
  return token_cache_[key];
}

const IdentityAPI::CachedTokens& IdentityAPI::GetAllCachedTokens() {
  return token_cache_;
}

std::vector<std::string> IdentityAPI::GetAccounts() const {
  const std::string primary_account_id = GetPrimaryAccountId(browser_context_);
  const std::vector<gaia::AccountIds> ids = account_tracker_.GetAccounts();
  std::vector<std::string> gaia_ids;

  if (switches::IsExtensionsMultiAccount()) {
    for (std::vector<gaia::AccountIds>::const_iterator it = ids.begin();
         it != ids.end();
         ++it) {
      gaia_ids.push_back(it->gaia);
    }
  } else if (ids.size() >= 1) {
    gaia_ids.push_back(ids[0].gaia);
  }

  return gaia_ids;
}

std::string IdentityAPI::FindAccountKeyByGaiaId(const std::string& gaia_id) {
  return account_tracker_.FindAccountIdsByGaiaId(gaia_id).account_key;
}

void IdentityAPI::Shutdown() {
  FOR_EACH_OBSERVER(ShutdownObserver, shutdown_observer_list_, OnShutdown());
  account_tracker_.RemoveObserver(this);
  account_tracker_.Shutdown();
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<IdentityAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<IdentityAPI>* IdentityAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void IdentityAPI::OnAccountAdded(const gaia::AccountIds& ids) {
}

void IdentityAPI::OnAccountRemoved(const gaia::AccountIds& ids) {
}

void IdentityAPI::OnAccountSignInChanged(const gaia::AccountIds& ids,
                                         bool is_signed_in) {
  api::identity::AccountInfo account_info;
  account_info.id = ids.gaia;

  std::unique_ptr<base::ListValue> args =
      api::identity::OnSignInChanged::Create(account_info, is_signed_in);
  std::unique_ptr<Event> event(
      new Event(events::IDENTITY_ON_SIGN_IN_CHANGED,
                api::identity::OnSignInChanged::kEventName, std::move(args),
                browser_context_));

  EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
}

void IdentityAPI::AddShutdownObserver(ShutdownObserver* observer) {
  shutdown_observer_list_.AddObserver(observer);
}

void IdentityAPI::RemoveShutdownObserver(ShutdownObserver* observer) {
  shutdown_observer_list_.RemoveObserver(observer);
}

void IdentityAPI::SetAccountStateForTest(gaia::AccountIds ids,
                                         bool is_signed_in) {
  account_tracker_.SetAccountStateForTest(ids, is_signed_in);
}

template <>
void BrowserContextKeyedAPIFactory<IdentityAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

IdentityGetAccountsFunction::IdentityGetAccountsFunction() {
}

IdentityGetAccountsFunction::~IdentityGetAccountsFunction() {
}

ExtensionFunction::ResponseAction IdentityGetAccountsFunction::Run() {
  if (GetProfile()->IsOffTheRecord()) {
    return RespondNow(Error(identity_constants::kOffTheRecord));
  }

  std::vector<std::string> gaia_ids =
      IdentityAPI::GetFactoryInstance()->Get(GetProfile())->GetAccounts();
  DCHECK(gaia_ids.size() < 2 || switches::IsExtensionsMultiAccount());

  std::unique_ptr<base::ListValue> infos(new base::ListValue());

  for (std::vector<std::string>::const_iterator it = gaia_ids.begin();
       it != gaia_ids.end();
       ++it) {
    api::identity::AccountInfo account_info;
    account_info.id = *it;
    infos->Append(account_info.ToValue());
  }

  return RespondNow(OneArgument(std::move(infos)));
}

IdentityGetAuthTokenFunction::IdentityGetAuthTokenFunction()
    : OAuth2TokenService::Consumer("extensions_identity_api"),
      interactive_(false),
      should_prompt_for_scopes_(false),
      should_prompt_for_signin_(false) {
}

IdentityGetAuthTokenFunction::~IdentityGetAuthTokenFunction() {
  TRACE_EVENT_ASYNC_END0("identity", "IdentityGetAuthTokenFunction", this);
}

bool IdentityGetAuthTokenFunction::RunAsync() {
  TRACE_EVENT_ASYNC_BEGIN1("identity",
                           "IdentityGetAuthTokenFunction",
                           this,
                           "extension",
                           extension()->id());

  if (GetProfile()->IsOffTheRecord()) {
    error_ = identity_constants::kOffTheRecord;
    return false;
  }

  std::unique_ptr<identity::GetAuthToken::Params> params(
      identity::GetAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  interactive_ = params->details.get() &&
      params->details->interactive.get() &&
      *params->details->interactive;

  should_prompt_for_scopes_ = interactive_;
  should_prompt_for_signin_ = interactive_;

  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension());

  // Check that the necessary information is present in the manifest.
  oauth2_client_id_ = GetOAuth2ClientId();
  if (oauth2_client_id_.empty()) {
    error_ = identity_constants::kInvalidClientId;
    return false;
  }

  std::set<std::string> scopes(oauth2_info.scopes.begin(),
                               oauth2_info.scopes.end());

  std::string account_key = GetPrimaryAccountId(GetProfile());

  if (params->details.get()) {
    if (params->details->account.get()) {
      std::string detail_key =
          extensions::IdentityAPI::GetFactoryInstance()
              ->Get(GetProfile())
              ->FindAccountKeyByGaiaId(params->details->account->id);

      if (detail_key != account_key) {
        if (detail_key.empty() || !switches::IsExtensionsMultiAccount()) {
          // TODO(courage): should this be a different error?
          error_ = identity_constants::kUserNotSignedIn;
          return false;
        }

        account_key = detail_key;
      }
    }

    if (params->details->scopes.get()) {
      scopes = std::set<std::string>(params->details->scopes->begin(),
                                     params->details->scopes->end());
    }
  }

  if (scopes.size() == 0) {
    error_ = identity_constants::kInvalidScopes;
    return false;
  }

  token_key_.reset(
      new ExtensionTokenKey(extension()->id(), account_key, scopes));

  // From here on out, results must be returned asynchronously.
  StartAsyncRun();

#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool is_kiosk = user_manager::UserManager::Get()->IsLoggedInAsKioskApp();
  bool is_public_session =
      user_manager::UserManager::Get()->IsLoggedInAsPublicAccount();

  if (connector->IsEnterpriseManaged() && (is_kiosk || is_public_session)) {
    if (is_public_session && !IsOriginWhitelistedInPublicSession()) {
      CompleteFunctionWithError(identity_constants::kUserNotSignedIn);
      return true;
    }

    StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
    return true;
  }
#endif

  if (!HasLoginToken()) {
    if (!should_prompt_for_signin_) {
      CompleteFunctionWithError(identity_constants::kUserNotSignedIn);
      return true;
    }
    // Display a login prompt.
    StartSigninFlow();
  } else {
    StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
  }

  return true;
}

void IdentityGetAuthTokenFunction::StartAsyncRun() {
  // Balanced in CompleteAsyncRun
  AddRef();
  extensions::IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->AddShutdownObserver(this);
}

void IdentityGetAuthTokenFunction::CompleteAsyncRun(bool success) {
  extensions::IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->RemoveShutdownObserver(this);

  SendResponse(success);
  Release();  // Balanced in StartAsyncRun
}

void IdentityGetAuthTokenFunction::CompleteFunctionWithResult(
    const std::string& access_token) {
  SetResult(base::MakeUnique<base::StringValue>(access_token));
  CompleteAsyncRun(true);
}

void IdentityGetAuthTokenFunction::CompleteFunctionWithError(
    const std::string& error) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "CompleteFunctionWithError",
                               "error",
                               error);
  error_ = error;
  CompleteAsyncRun(false);
}

void IdentityGetAuthTokenFunction::StartSigninFlow() {
  // All cached tokens are invalid because the user is not signed in.
  IdentityAPI* id_api =
      extensions::IdentityAPI::GetFactoryInstance()->Get(GetProfile());
  id_api->EraseAllCachedTokens();
  // Display a login prompt. If the subsequent mint fails, don't display the
  // login prompt again.
  should_prompt_for_signin_ = false;
  ShowLoginPopup();
}

void IdentityGetAuthTokenFunction::StartMintTokenFlow(
    IdentityMintRequestQueue::MintType type) {
  mint_token_flow_type_ = type;

  // Flows are serialized to prevent excessive traffic to GAIA, and
  // to consolidate UI pop-ups.
  IdentityAPI* id_api =
      extensions::IdentityAPI::GetFactoryInstance()->Get(GetProfile());

  if (!should_prompt_for_scopes_) {
    // Caller requested no interaction.

    if (type == IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE) {
      // GAIA told us to do a consent UI.
      CompleteFunctionWithError(identity_constants::kNoGrant);
      return;
    }
    if (!id_api->mint_queue()->empty(
            IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE, *token_key_)) {
      // Another call is going through a consent UI.
      CompleteFunctionWithError(identity_constants::kNoGrant);
      return;
    }
  }
  id_api->mint_queue()->RequestStart(type, *token_key_, this);
}

void IdentityGetAuthTokenFunction::CompleteMintTokenFlow() {
  IdentityMintRequestQueue::MintType type = mint_token_flow_type_;

  extensions::IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->mint_queue()
      ->RequestComplete(type, *token_key_, this);
}

void IdentityGetAuthTokenFunction::StartMintToken(
    IdentityMintRequestQueue::MintType type) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "StartMintToken",
                               "type",
                               type);

  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension());
  IdentityAPI* id_api = IdentityAPI::GetFactoryInstance()->Get(GetProfile());
  IdentityTokenCacheValue cache_entry = id_api->GetCachedToken(*token_key_);
  IdentityTokenCacheValue::CacheValueStatus cache_status =
      cache_entry.status();

  if (type == IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE) {
    switch (cache_status) {
      case IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND:
#if defined(OS_CHROMEOS)
        // Always force minting token for ChromeOS kiosk app and public session.
        if (user_manager::UserManager::Get()->IsLoggedInAsPublicAccount() &&
            !IsOriginWhitelistedInPublicSession()) {
          CompleteFunctionWithError(identity_constants::kUserNotSignedIn);
          return;
        }

        if (user_manager::UserManager::Get()->IsLoggedInAsKioskApp() ||
            user_manager::UserManager::Get()->IsLoggedInAsPublicAccount()) {
          gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE;
          policy::BrowserPolicyConnectorChromeOS* connector =
              g_browser_process->platform_part()
                  ->browser_policy_connector_chromeos();
          if (connector->IsEnterpriseManaged()) {
            StartDeviceLoginAccessTokenRequest();
          } else {
            StartLoginAccessTokenRequest();
          }
          return;
        }
#endif

        if (oauth2_info.auto_approve)
          // oauth2_info.auto_approve is protected by a whitelist in
          // _manifest_features.json hence only selected extensions take
          // advantage of forcefully minting the token.
          gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE;
        else
          gaia_mint_token_mode_ = OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE;
        StartLoginAccessTokenRequest();
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_TOKEN:
        CompleteMintTokenFlow();
        CompleteFunctionWithResult(cache_entry.token());
        break;

      case IdentityTokenCacheValue::CACHE_STATUS_ADVICE:
        CompleteMintTokenFlow();
        should_prompt_for_signin_ = false;
        issue_advice_ = cache_entry.issue_advice();
        StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
        break;
    }
  } else {
    DCHECK(type == IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);

    if (cache_status == IdentityTokenCacheValue::CACHE_STATUS_TOKEN) {
      CompleteMintTokenFlow();
      CompleteFunctionWithResult(cache_entry.token());
    } else {
      ShowOAuthApprovalDialog(issue_advice_);
    }
  }
}

void IdentityGetAuthTokenFunction::OnMintTokenSuccess(
    const std::string& access_token, int time_to_live) {
  TRACE_EVENT_ASYNC_STEP_PAST0("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "OnMintTokenSuccess");

  IdentityTokenCacheValue token(access_token,
                                base::TimeDelta::FromSeconds(time_to_live));
  IdentityAPI::GetFactoryInstance()->Get(GetProfile())->SetCachedToken(
      *token_key_, token);

  CompleteMintTokenFlow();
  CompleteFunctionWithResult(access_token);
}

void IdentityGetAuthTokenFunction::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "OnMintTokenFailure",
                               "error",
                               error.ToString());
  CompleteMintTokenFlow();
  switch (error.state()) {
    case GoogleServiceAuthError::SERVICE_ERROR:
      if (interactive_) {
        StartSigninFlow();
        return;
      }
      break;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      // TODO(courage): flush ticket and retry once
      if (should_prompt_for_signin_) {
        // Display a login prompt and try again (once).
        StartSigninFlow();
        return;
      }
      break;
    default:
      // Return error to caller.
      break;
  }

  CompleteFunctionWithError(
      std::string(identity_constants::kAuthFailure) + error.ToString());
}

void IdentityGetAuthTokenFunction::OnIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  TRACE_EVENT_ASYNC_STEP_PAST0("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "OnIssueAdviceSuccess");

  IdentityAPI::GetFactoryInstance()->Get(GetProfile())->SetCachedToken(
      *token_key_, IdentityTokenCacheValue(issue_advice));
  CompleteMintTokenFlow();

  should_prompt_for_signin_ = false;
  // Existing grant was revoked and we used NO_FORCE, so we got info back
  // instead. Start a consent UI if we can.
  issue_advice_ = issue_advice;
  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE);
}

void IdentityGetAuthTokenFunction::SigninSuccess() {
  TRACE_EVENT_ASYNC_STEP_PAST0("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "SigninSuccess");

  // If there was no account associated this profile before the
  // sign-in, we may not have an account_id in the token_key yet.
  if (token_key_->account_id.empty()) {
    token_key_->account_id = GetPrimaryAccountId(GetProfile());
  }

  StartMintTokenFlow(IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE);
}

void IdentityGetAuthTokenFunction::SigninFailed() {
  TRACE_EVENT_ASYNC_STEP_PAST0("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "SigninFailed");
  CompleteFunctionWithError(identity_constants::kUserNotSignedIn);
}

void IdentityGetAuthTokenFunction::OnGaiaFlowFailure(
    GaiaWebAuthFlow::Failure failure,
    GoogleServiceAuthError service_error,
    const std::string& oauth_error) {
  CompleteMintTokenFlow();
  std::string error;

  switch (failure) {
    case GaiaWebAuthFlow::WINDOW_CLOSED:
      error = identity_constants::kUserRejected;
      break;

    case GaiaWebAuthFlow::INVALID_REDIRECT:
      error = identity_constants::kInvalidRedirect;
      break;

    case GaiaWebAuthFlow::SERVICE_AUTH_ERROR:
      // If this is really an authentication error and not just a transient
      // network error, and this is an interactive request for a signed-in
      // user, then we show signin UI instead of failing.
      if (service_error.state() != GoogleServiceAuthError::CONNECTION_FAILED &&
          service_error.state() !=
              GoogleServiceAuthError::SERVICE_UNAVAILABLE &&
          interactive_ && HasLoginToken()) {
        StartSigninFlow();
        return;
      }
      error = std::string(identity_constants::kAuthFailure) +
          service_error.ToString();
      break;

    case GaiaWebAuthFlow::OAUTH_ERROR:
      error = MapOAuth2ErrorToDescription(oauth_error);
      break;

    case GaiaWebAuthFlow::LOAD_FAILED:
      error = identity_constants::kPageLoadFailure;
      break;

    default:
      NOTREACHED() << "Unexpected error from gaia web auth flow: " << failure;
      error = identity_constants::kInvalidRedirect;
      break;
  }

  CompleteFunctionWithError(error);
}

void IdentityGetAuthTokenFunction::OnGaiaFlowCompleted(
    const std::string& access_token,
    const std::string& expiration) {
  TRACE_EVENT_ASYNC_STEP_PAST0("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "OnGaiaFlowCompleted");
  int time_to_live;
  if (!expiration.empty() && base::StringToInt(expiration, &time_to_live)) {
    IdentityTokenCacheValue token_value(
        access_token, base::TimeDelta::FromSeconds(time_to_live));
    IdentityAPI::GetFactoryInstance()->Get(GetProfile())->SetCachedToken(
        *token_key_, token_value);
  }

  CompleteMintTokenFlow();
  CompleteFunctionWithResult(access_token);
}

void IdentityGetAuthTokenFunction::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "OnGetTokenSuccess",
                               "account",
                               request->GetAccountId());
  login_token_request_.reset();
  StartGaiaRequest(access_token);
}

void IdentityGetAuthTokenFunction::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity",
                               "IdentityGetAuthTokenFunction",
                               this,
                               "OnGetTokenFailure",
                               "error",
                               error.ToString());
  login_token_request_.reset();
  OnGaiaFlowFailure(GaiaWebAuthFlow::SERVICE_AUTH_ERROR, error, std::string());
}

void IdentityGetAuthTokenFunction::OnShutdown() {
  gaia_web_auth_flow_.reset();
  signin_flow_.reset();
  login_token_request_.reset();
  extensions::IdentityAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->mint_queue()
      ->RequestCancel(*token_key_, this);
  CompleteFunctionWithError(identity_constants::kCanceled);
}

#if defined(OS_CHROMEOS)
void IdentityGetAuthTokenFunction::StartDeviceLoginAccessTokenRequest() {
  chromeos::DeviceOAuth2TokenService* service =
      chromeos::DeviceOAuth2TokenServiceFactory::Get();
  // Since robot account refresh tokens are scoped down to [any-api] only,
  // request access token for [any-api] instead of login.
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kAnyApiOAuth2Scope);
  login_token_request_ =
      service->StartRequest(service->GetRobotAccountId(),
                            scopes,
                            this);
}

bool IdentityGetAuthTokenFunction::IsOriginWhitelistedInPublicSession() {
  DCHECK(extension());
  GURL extension_url = extension()->url();
  for (size_t i = 0; i < arraysize(kPublicSessionAllowedOrigins); i++) {
    URLPattern allowed_origin(URLPattern::SCHEME_ALL,
                              kPublicSessionAllowedOrigins[i]);
    if (allowed_origin.MatchesSecurityOrigin(extension_url)) {
      return true;
    }
  }
  return false;
}
#endif

void IdentityGetAuthTokenFunction::StartLoginAccessTokenRequest() {
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(GetProfile());
#if defined(OS_CHROMEOS)
  if (chrome::IsRunningInForcedAppMode()) {
    std::string app_client_id;
    std::string app_client_secret;
    if (chromeos::UserSessionManager::GetInstance()->
            GetAppModeChromeClientOAuthInfo(&app_client_id,
                                            &app_client_secret)) {
      login_token_request_ =
          service->StartRequestForClient(token_key_->account_id,
                                         app_client_id,
                                         app_client_secret,
                                         OAuth2TokenService::ScopeSet(),
                                         this);
      return;
    }
  }
#endif
  login_token_request_ = service->StartRequest(
      token_key_->account_id, OAuth2TokenService::ScopeSet(), this);
}

void IdentityGetAuthTokenFunction::StartGaiaRequest(
    const std::string& login_access_token) {
  DCHECK(!login_access_token.empty());
  mint_token_flow_.reset(CreateMintTokenFlow());
  mint_token_flow_->Start(GetProfile()->GetRequestContext(),
                          login_access_token);
}

void IdentityGetAuthTokenFunction::ShowLoginPopup() {
  signin_flow_.reset(new IdentitySigninFlow(this, GetProfile()));
  signin_flow_->Start();
}

void IdentityGetAuthTokenFunction::ShowOAuthApprovalDialog(
    const IssueAdviceInfo& issue_advice) {
  const std::string locale = extension_l10n_util::CurrentLocaleOrDefault();

  gaia_web_auth_flow_.reset(new GaiaWebAuthFlow(
      this, GetProfile(), token_key_.get(), oauth2_client_id_, locale));
  gaia_web_auth_flow_->Start();
}

OAuth2MintTokenFlow* IdentityGetAuthTokenFunction::CreateMintTokenFlow() {
  SigninClient* signin_client =
      ChromeSigninClientFactory::GetForProfile(GetProfile());
  std::string signin_scoped_device_id =
      signin_client->GetSigninScopedDeviceId();
  OAuth2MintTokenFlow* mint_token_flow = new OAuth2MintTokenFlow(
      this,
      OAuth2MintTokenFlow::Parameters(
          extension()->id(),
          oauth2_client_id_,
          std::vector<std::string>(token_key_->scopes.begin(),
                                   token_key_->scopes.end()),
          signin_scoped_device_id,
          gaia_mint_token_mode_));
  return mint_token_flow;
}

bool IdentityGetAuthTokenFunction::HasLoginToken() const {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(GetProfile());
  return token_service->RefreshTokenIsAvailable(token_key_->account_id);
}

std::string IdentityGetAuthTokenFunction::MapOAuth2ErrorToDescription(
    const std::string& error) {
  const char kOAuth2ErrorAccessDenied[] = "access_denied";
  const char kOAuth2ErrorInvalidScope[] = "invalid_scope";

  if (error == kOAuth2ErrorAccessDenied)
    return std::string(identity_constants::kUserRejected);
  else if (error == kOAuth2ErrorInvalidScope)
    return std::string(identity_constants::kInvalidScopes);
  else
    return std::string(identity_constants::kAuthFailure) + error;
}

std::string IdentityGetAuthTokenFunction::GetOAuth2ClientId() const {
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension());
  std::string client_id = oauth2_info.client_id;

  // Component apps using auto_approve may use Chrome's client ID by
  // omitting the field.
  if (client_id.empty() && extension()->location() == Manifest::COMPONENT &&
      oauth2_info.auto_approve) {
    client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  }
  return client_id;
}

IdentityGetProfileUserInfoFunction::IdentityGetProfileUserInfoFunction() {
}

IdentityGetProfileUserInfoFunction::~IdentityGetProfileUserInfoFunction() {
}

ExtensionFunction::ResponseAction IdentityGetProfileUserInfoFunction::Run() {
  if (GetProfile()->IsOffTheRecord()) {
    return RespondNow(Error(identity_constants::kOffTheRecord));
  }

  AccountInfo account =
      AccountTrackerServiceFactory::GetForProfile(GetProfile())
          ->GetAccountInfo(GetPrimaryAccountId(GetProfile()));
  api::identity::ProfileUserInfo profile_user_info;
  if (extension()->permissions_data()->HasAPIPermission(
          APIPermission::kIdentityEmail)) {
    profile_user_info.email = account.email;
    profile_user_info.id = account.gaia;
  }

  return RespondNow(OneArgument(profile_user_info.ToValue()));
}

IdentityRemoveCachedAuthTokenFunction::IdentityRemoveCachedAuthTokenFunction() {
}

IdentityRemoveCachedAuthTokenFunction::
    ~IdentityRemoveCachedAuthTokenFunction() {
}

bool IdentityRemoveCachedAuthTokenFunction::RunSync() {
  if (GetProfile()->IsOffTheRecord()) {
    error_ = identity_constants::kOffTheRecord;
    return false;
  }

  std::unique_ptr<identity::RemoveCachedAuthToken::Params> params(
      identity::RemoveCachedAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  IdentityAPI::GetFactoryInstance()->Get(GetProfile())->EraseCachedToken(
      extension()->id(), params->details.token);
  return true;
}

IdentityLaunchWebAuthFlowFunction::IdentityLaunchWebAuthFlowFunction() {}

IdentityLaunchWebAuthFlowFunction::~IdentityLaunchWebAuthFlowFunction() {
  if (auth_flow_)
    auth_flow_.release()->DetachDelegateAndDelete();
}

bool IdentityLaunchWebAuthFlowFunction::RunAsync() {
  if (GetProfile()->IsOffTheRecord()) {
    error_ = identity_constants::kOffTheRecord;
    return false;
  }

  std::unique_ptr<identity::LaunchWebAuthFlow::Params> params(
      identity::LaunchWebAuthFlow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL auth_url(params->details.url);
  WebAuthFlow::Mode mode =
      params->details.interactive && *params->details.interactive ?
      WebAuthFlow::INTERACTIVE : WebAuthFlow::SILENT;

  // Set up acceptable target URLs. (Does not include chrome-extension
  // scheme for this version of the API.)
  InitFinalRedirectURLPrefix(extension()->id());

  AddRef();  // Balanced in OnAuthFlowSuccess/Failure.

  auth_flow_.reset(new WebAuthFlow(this, GetProfile(), auth_url, mode));
  auth_flow_->Start();
  return true;
}

void IdentityLaunchWebAuthFlowFunction::InitFinalRedirectURLPrefixForTest(
    const std::string& extension_id) {
  InitFinalRedirectURLPrefix(extension_id);
}

void IdentityLaunchWebAuthFlowFunction::InitFinalRedirectURLPrefix(
    const std::string& extension_id) {
  if (final_url_prefix_.is_empty()) {
    final_url_prefix_ = GURL(base::StringPrintf(
        kChromiumDomainRedirectUrlPattern, extension_id.c_str()));
  }
}

void IdentityLaunchWebAuthFlowFunction::OnAuthFlowFailure(
    WebAuthFlow::Failure failure) {
  switch (failure) {
    case WebAuthFlow::WINDOW_CLOSED:
      error_ = identity_constants::kUserRejected;
      break;
    case WebAuthFlow::INTERACTION_REQUIRED:
      error_ = identity_constants::kInteractionRequired;
      break;
    case WebAuthFlow::LOAD_FAILED:
      error_ = identity_constants::kPageLoadFailure;
      break;
    default:
      NOTREACHED() << "Unexpected error from web auth flow: " << failure;
      error_ = identity_constants::kInvalidRedirect;
      break;
  }
  SendResponse(false);
  if (auth_flow_)
    auth_flow_.release()->DetachDelegateAndDelete();
  Release();  // Balanced in RunAsync.
}

void IdentityLaunchWebAuthFlowFunction::OnAuthFlowURLChange(
    const GURL& redirect_url) {
  if (redirect_url.GetWithEmptyPath() == final_url_prefix_) {
    SetResult(base::MakeUnique<base::StringValue>(redirect_url.spec()));
    SendResponse(true);
    if (auth_flow_)
      auth_flow_.release()->DetachDelegateAndDelete();
    Release();  // Balanced in RunAsync.
  }
}

}  // namespace extensions
