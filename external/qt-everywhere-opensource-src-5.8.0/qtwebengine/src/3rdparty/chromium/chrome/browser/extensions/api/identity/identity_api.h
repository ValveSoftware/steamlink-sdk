// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/identity/extension_token_key.h"
#include "chrome/browser/extensions/api/identity/gaia_web_auth_flow.h"
#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"
#include "chrome/browser/extensions/api/identity/identity_signin_flow.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "components/signin/core/browser/profile_identity_provider.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "google_apis/gaia/account_tracker.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "google_apis/gaia/oauth2_token_service.h"

class GoogleServiceAuthError;
class MockGetAuthTokenFunction;

namespace content {
class BrowserContext;
}

namespace extensions {

class GetAuthTokenFunctionTest;
class MockGetAuthTokenFunction;

namespace identity_constants {
extern const char kInvalidClientId[];
extern const char kInvalidScopes[];
extern const char kAuthFailure[];
extern const char kNoGrant[];
extern const char kUserRejected[];
extern const char kUserNotSignedIn[];
extern const char kInteractionRequired[];
extern const char kInvalidRedirect[];
extern const char kOffTheRecord[];
extern const char kPageLoadFailure[];
extern const char kCanceled[];
}  // namespace identity_constants

class IdentityTokenCacheValue {
 public:
  IdentityTokenCacheValue();
  explicit IdentityTokenCacheValue(const IssueAdviceInfo& issue_advice);
  IdentityTokenCacheValue(const std::string& token,
                          base::TimeDelta time_to_live);
  IdentityTokenCacheValue(const IdentityTokenCacheValue& other);
  ~IdentityTokenCacheValue();

  // Order of these entries is used to determine whether or not new
  // entries supercede older ones in SetCachedToken.
  enum CacheValueStatus {
    CACHE_STATUS_NOTFOUND,
    CACHE_STATUS_ADVICE,
    CACHE_STATUS_TOKEN
  };

  CacheValueStatus status() const;
  const IssueAdviceInfo& issue_advice() const;
  const std::string& token() const;
  const base::Time& expiration_time() const;

 private:
  bool is_expired() const;

  CacheValueStatus status_;
  IssueAdviceInfo issue_advice_;
  std::string token_;
  base::Time expiration_time_;
};

class IdentityAPI : public BrowserContextKeyedAPI,
                    public gaia::AccountTracker::Observer {
 public:
  typedef std::map<ExtensionTokenKey, IdentityTokenCacheValue> CachedTokens;

  class ShutdownObserver {
   public:
    virtual void OnShutdown() = 0;
  };

  explicit IdentityAPI(content::BrowserContext* context);
  ~IdentityAPI() override;

  // Request serialization queue for getAuthToken.
  IdentityMintRequestQueue* mint_queue();

  // Token cache
  void SetCachedToken(const ExtensionTokenKey& key,
                      const IdentityTokenCacheValue& token_data);
  void EraseCachedToken(const std::string& extension_id,
                        const std::string& token);
  void EraseAllCachedTokens();
  const IdentityTokenCacheValue& GetCachedToken(const ExtensionTokenKey& key);

  const CachedTokens& GetAllCachedTokens();

  // Account queries.
  std::vector<std::string> GetAccounts() const;
  std::string FindAccountKeyByGaiaId(const std::string& gaia_id);

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;
  static BrowserContextKeyedAPIFactory<IdentityAPI>* GetFactoryInstance();

  // gaia::AccountTracker::Observer implementation:
  void OnAccountAdded(const gaia::AccountIds& ids) override;
  void OnAccountRemoved(const gaia::AccountIds& ids) override;
  void OnAccountSignInChanged(const gaia::AccountIds& ids,
                              bool is_signed_in) override;

  void AddShutdownObserver(ShutdownObserver* observer);
  void RemoveShutdownObserver(ShutdownObserver* observer);

  void SetAccountStateForTest(gaia::AccountIds ids, bool is_signed_in);

 private:
  friend class BrowserContextKeyedAPIFactory<IdentityAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "IdentityAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* browser_context_;
  IdentityMintRequestQueue mint_queue_;
  CachedTokens token_cache_;
  ProfileIdentityProvider profile_identity_provider_;
  gaia::AccountTracker account_tracker_;
  base::ObserverList<ShutdownObserver> shutdown_observer_list_;
};

template <>
void BrowserContextKeyedAPIFactory<IdentityAPI>::DeclareFactoryDependencies();

class IdentityGetAccountsFunction : public ChromeUIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.getAccounts",
                             IDENTITY_GETACCOUNTS);

  IdentityGetAccountsFunction();

 private:
  ~IdentityGetAccountsFunction() override;

  // UIThreadExtensionFunction implementation.
  ExtensionFunction::ResponseAction Run() override;
};

// identity.getAuthToken fetches an OAuth 2 function for the
// caller. The request has three sub-flows: non-interactive,
// interactive, and sign-in.
//
// In the non-interactive flow, getAuthToken requests a token from
// GAIA. GAIA may respond with a token, an error, or "consent
// required". In the consent required cases, getAuthToken proceeds to
// the second, interactive phase.
//
// The interactive flow presents a scope approval dialog to the
// user. If the user approves the request, a grant will be recorded on
// the server, and an access token will be returned to the caller.
//
// In some cases we need to display a sign-in dialog. Normally the
// profile will be signed in already, but if it turns out we need a
// new login token, there is a sign-in flow. If that flow completes
// successfully, getAuthToken proceeds to the non-interactive flow.
class IdentityGetAuthTokenFunction : public ChromeAsyncExtensionFunction,
                                     public GaiaWebAuthFlow::Delegate,
                                     public IdentityMintRequestQueue::Request,
                                     public OAuth2MintTokenFlow::Delegate,
                                     public IdentitySigninFlow::Delegate,
                                     public OAuth2TokenService::Consumer,
                                     public IdentityAPI::ShutdownObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.getAuthToken",
                             EXPERIMENTAL_IDENTITY_GETAUTHTOKEN);

  IdentityGetAuthTokenFunction();

  const ExtensionTokenKey* GetExtensionTokenKeyForTest() {
    return token_key_.get();
  }

 protected:
  ~IdentityGetAuthTokenFunction() override;

  // IdentitySigninFlow::Delegate implementation:
  void SigninSuccess() override;
  void SigninFailed() override;

  // GaiaWebAuthFlow::Delegate implementation:
  void OnGaiaFlowFailure(GaiaWebAuthFlow::Failure failure,
                         GoogleServiceAuthError service_error,
                         const std::string& oauth_error) override;
  void OnGaiaFlowCompleted(const std::string& access_token,
                           const std::string& expiration) override;

  // Starts a login access token request.
  virtual void StartLoginAccessTokenRequest();

  // OAuth2TokenService::Consumer implementation:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Starts a mint token request to GAIA.
  // Exposed for testing.
  virtual void StartGaiaRequest(const std::string& login_access_token);

  // Caller owns the returned instance.
  // Exposed for testing.
  virtual OAuth2MintTokenFlow* CreateMintTokenFlow();

  std::unique_ptr<OAuth2TokenService::Request> login_token_request_;

 private:
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest,
                           ComponentWithChromeClientId);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest,
                           ComponentWithNormalClientId);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest, InteractiveQueueShutdown);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest, NoninteractiveShutdown);

  // ExtensionFunction:
  bool RunAsync() override;

  // Helpers to report async function results to the caller.
  void StartAsyncRun();
  void CompleteAsyncRun(bool success);
  void CompleteFunctionWithResult(const std::string& access_token);
  void CompleteFunctionWithError(const std::string& error);

  // Initiate/complete the sub-flows.
  void StartSigninFlow();
  void StartMintTokenFlow(IdentityMintRequestQueue::MintType type);
  void CompleteMintTokenFlow();

  // IdentityMintRequestQueue::Request implementation:
  void StartMintToken(IdentityMintRequestQueue::MintType type) override;

  // OAuth2MintTokenFlow::Delegate implementation:
  void OnMintTokenSuccess(const std::string& access_token,
                          int time_to_live) override;
  void OnMintTokenFailure(const GoogleServiceAuthError& error) override;
  void OnIssueAdviceSuccess(const IssueAdviceInfo& issue_advice) override;

  // IdentityAPI::ShutdownObserver implementation:
  void OnShutdown() override;

#if defined(OS_CHROMEOS)
  // Starts a login access token request for device robot account. This method
  // will be called only in Chrome OS for:
  // 1. Enterprise kiosk mode.
  // 2. Whitelisted first party apps in public session.
  virtual void StartDeviceLoginAccessTokenRequest();

  bool IsOriginWhitelistedInPublicSession();
#endif

  // Methods for invoking UI. Overridable for testing.
  virtual void ShowLoginPopup();
  virtual void ShowOAuthApprovalDialog(const IssueAdviceInfo& issue_advice);

  // Checks if there is a master login token to mint tokens for the extension.
  bool HasLoginToken() const;

  // Maps OAuth2 protocol errors to an error message returned to the
  // developer in chrome.runtime.lastError.
  std::string MapOAuth2ErrorToDescription(const std::string& error);

  std::string GetOAuth2ClientId() const;

  bool interactive_;
  bool should_prompt_for_scopes_;
  IdentityMintRequestQueue::MintType mint_token_flow_type_;
  std::unique_ptr<OAuth2MintTokenFlow> mint_token_flow_;
  OAuth2MintTokenFlow::Mode gaia_mint_token_mode_;
  bool should_prompt_for_signin_;

  std::unique_ptr<ExtensionTokenKey> token_key_;
  std::string oauth2_client_id_;
  // When launched in interactive mode, and if there is no existing grant,
  // a permissions prompt will be popped up to the user.
  IssueAdviceInfo issue_advice_;
  std::unique_ptr<GaiaWebAuthFlow> gaia_web_auth_flow_;
  std::unique_ptr<IdentitySigninFlow> signin_flow_;
};

class IdentityGetProfileUserInfoFunction
    : public ChromeUIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.getProfileUserInfo",
                             IDENTITY_GETPROFILEUSERINFO);

  IdentityGetProfileUserInfoFunction();

 private:
  ~IdentityGetProfileUserInfoFunction() override;

  // UIThreadExtensionFunction implementation.
  ExtensionFunction::ResponseAction Run() override;
};

class IdentityRemoveCachedAuthTokenFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.removeCachedAuthToken",
                             EXPERIMENTAL_IDENTITY_REMOVECACHEDAUTHTOKEN)
  IdentityRemoveCachedAuthTokenFunction();

 protected:
  ~IdentityRemoveCachedAuthTokenFunction() override;

  // SyncExtensionFunction implementation:
  bool RunSync() override;
};

class IdentityLaunchWebAuthFlowFunction : public ChromeAsyncExtensionFunction,
                                          public WebAuthFlow::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.launchWebAuthFlow",
                             EXPERIMENTAL_IDENTITY_LAUNCHWEBAUTHFLOW);

  IdentityLaunchWebAuthFlowFunction();

  // Tests may override extension_id.
  void InitFinalRedirectURLPrefixForTest(const std::string& extension_id);

 private:
  ~IdentityLaunchWebAuthFlowFunction() override;
  bool RunAsync() override;

  // WebAuthFlow::Delegate implementation.
  void OnAuthFlowFailure(WebAuthFlow::Failure failure) override;
  void OnAuthFlowURLChange(const GURL& redirect_url) override;
  void OnAuthFlowTitleChange(const std::string& title) override {}

  // Helper to initialize final URL prefix.
  void InitFinalRedirectURLPrefix(const std::string& extension_id);

  std::unique_ptr<WebAuthFlow> auth_flow_;
  GURL final_url_prefix_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
