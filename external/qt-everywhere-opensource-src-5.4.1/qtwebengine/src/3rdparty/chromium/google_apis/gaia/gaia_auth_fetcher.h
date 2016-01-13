// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

// Authenticate a user against the Google Accounts ClientLogin API
// with various capabilities and return results to a GaiaAuthConsumer.
//
// In the future, we will also issue auth tokens from this class.
// This class should be used on a single thread, but it can be whichever thread
// that you like.
//
// This class can handle one request at a time on any thread. To parallelize
// requests, create multiple GaiaAuthFetcher's.

class GaiaAuthFetcherTest;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
class URLRequestStatus;
}

class GaiaAuthFetcher : public net::URLFetcherDelegate {
 public:
  enum HostedAccountsSetting {
    HostedAccountsAllowed,
    HostedAccountsNotAllowed
  };

  // Magic string indicating that, while a second factor is still
  // needed to complete authentication, the user provided the right password.
  static const char kSecondFactor[];

  // This will later be hidden behind an auth service which caches
  // tokens.
  GaiaAuthFetcher(GaiaAuthConsumer* consumer,
                  const std::string& source,
                  net::URLRequestContextGetter* getter);
  virtual ~GaiaAuthFetcher();

  // Start a request to obtain the SID and LSID cookies for the the account
  // identified by |username| and |password|.  If |service| is not null or
  // empty, then also obtains a service token for specified service.
  //
  // If this is a second call because of captcha challenge, then the
  // |login_token| and |login_captcha| arugment should correspond to the
  // solution of the challenge.
  //
  // Either OnClientLoginSuccess or OnClientLoginFailure will be
  // called on the consumer on the original thread.
  void StartClientLogin(const std::string& username,
                        const std::string& password,
                        const char* const service,
                        const std::string& login_token,
                        const std::string& login_captcha,
                        HostedAccountsSetting allow_hosted_accounts);

  // Start a request to obtain service token for the the account identified by
  // |sid| and |lsid| and the |service|.
  //
  // Either OnIssueAuthTokenSuccess or OnIssueAuthTokenFailure will be
  // called on the consumer on the original thread.
  void StartIssueAuthToken(const std::string& sid,
                           const std::string& lsid,
                           const char* const service);

  // Start a request to obtain |service| token for the the account identified by
  // |uber_token|.
  //
  // Either OnIssueAuthTokenSuccess or OnIssueAuthTokenFailure will be
  // called on the consumer on the original thread.
  void StartTokenAuth(const std::string& uber_token,
                      const char* const service);

  // Start a request to obtain service token for the the account identified by
  // |oauth2_access_token| and the |service|.
  //
  // Either OnIssueAuthTokenSuccess or OnIssueAuthTokenFailure will be
  // called on the consumer on the original thread.
  void StartIssueAuthTokenForOAuth2(const std::string& oauth2_access_token,
                                    const char* const service);

  // Start a request to exchange an "lso" service token given by |auth_token|
  // for an OAuthLogin-scoped oauth2 token.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartLsoForOAuthLoginTokenExchange(const std::string& auth_token);

  // Start a request to revoke |auth_token|.
  //
  // OnOAuth2RevokeTokenCompleted will be called on the consumer on the original
  // thread.
  void StartRevokeOAuth2Token(const std::string& auth_token);

  // Start a request to exchange the cookies of a signed-in user session
  // for an OAuthLogin-scoped oauth2 token.  In the case of a session with
  // multiple accounts signed in, |session_index| indicate the which of accounts
  // within the session.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartCookieForOAuthLoginTokenExchange(const std::string& session_index);

  // Start a request to exchange the cookies of a signed-in user session
  // for an OAuthLogin-scoped oauth2 token. In the case of a session with
  // multiple accounts signed in, |session_index| indicate the which of accounts
  // within the session.
  // Resulting refresh token is annotated on the server with |device_id|. Format
  // of device_id on the server is at most 64 unicode characters.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartCookieForOAuthLoginTokenExchangeWithDeviceId(
      const std::string& session_index,
      const std::string& device_id);

  // Start a request to exchange the authorization code for an OAuthLogin-scoped
  // oauth2 token.
  //
  // Either OnClientOAuthSuccess or OnClientOAuthFailure will be
  // called on the consumer on the original thread.
  void StartAuthCodeForOAuth2TokenExchange(const std::string& auth_code);

  // Start a request to get user info for the account identified by |lsid|.
  //
  // Either OnGetUserInfoSuccess or OnGetUserInfoFailure will be
  // called on the consumer on the original thread.
  void StartGetUserInfo(const std::string& lsid);

  // Start a MergeSession request to pre-login the user with the given
  // credentials.
  //
  // Start a MergeSession request to fill the browsing cookie jar with
  // credentials represented by the account whose uber-auth token is
  // |uber_token|.  This method will modify the cookies of the current profile.
  //
  // Either OnMergeSessionSuccess or OnMergeSessionFailure will be
  // called on the consumer on the original thread.
  void StartMergeSession(const std::string& uber_token);

  // Start a request to exchange an OAuthLogin-scoped oauth2 access token for an
  // uber-auth token.  The returned token can be used with the method
  // StartMergeSession().
  //
  // Either OnUberAuthTokenSuccess or OnUberAuthTokenFailure will be
  // called on the consumer on the original thread.
  void StartTokenFetchForUberAuthExchange(const std::string& access_token);

  // Start a request to exchange an OAuthLogin-scoped oauth2 access token for a
  // ClientLogin-style service tokens.  The response to this request is the
  // same as the response to a ClientLogin request, except that captcha
  // challenges are never issued.
  //
  // Either OnClientLoginSuccess or OnClientLoginFailure will be
  // called on the consumer on the original thread. If |service| is empty,
  // the call will attempt to fetch uber auth token.
  void StartOAuthLogin(const std::string& access_token,
                       const std::string& service);

  // Starts a request to list the accounts in the GAIA cookie.
  void StartListAccounts();

  // Implementation of net::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // StartClientLogin been called && results not back yet?
  bool HasPendingFetch();

  // Stop any URL fetches in progress.
  void CancelRequest();

  // From a URLFetcher result, generate an appropriate error.
  // From the API documentation, both IssueAuthToken and ClientLogin have
  // the same error returns.
  static GoogleServiceAuthError GenerateOAuthLoginError(
      const std::string& data,
      const net::URLRequestStatus& status);

 private:
  // ClientLogin body constants that don't change
  static const char kCookiePersistence[];
  static const char kAccountTypeHostedOrGoogle[];
  static const char kAccountTypeGoogle[];

  // The format of the POST body for ClientLogin.
  static const char kClientLoginFormat[];
  // The format of said POST body when CAPTCHA token & answer are specified.
  static const char kClientLoginCaptchaFormat[];
  // The format of the POST body for IssueAuthToken.
  static const char kIssueAuthTokenFormat[];
  // The format of the POST body to get OAuth2 auth code from auth token.
  static const char kClientLoginToOAuth2BodyFormat[];
  // The format of the POST body to get OAuth2 auth code from auth token. This
  // format is used for request annotated with device_id.
  static const char kClientLoginToOAuth2WithDeviceTypeBodyFormat[];
  // The format of the POST body to get OAuth2 token pair from auth code.
  static const char kOAuth2CodeToTokenPairBodyFormat[];
  // The format of the POST body to revoke an OAuth2 token.
  static const char kOAuth2RevokeTokenBodyFormat[];
  // The format of the POST body for GetUserInfo.
  static const char kGetUserInfoFormat[];
  // The format of the POST body for MergeSession.
  static const char kMergeSessionFormat[];
  // The format of the URL for UberAuthToken.
  static const char kUberAuthTokenURLFormat[];
  // The format of the body for OAuthLogin.
  static const char kOAuthLoginFormat[];

  // Constants for parsing ClientLogin errors.
  static const char kAccountDeletedError[];
  static const char kAccountDeletedErrorCode[];
  static const char kAccountDisabledError[];
  static const char kAccountDisabledErrorCode[];
  static const char kBadAuthenticationError[];
  static const char kBadAuthenticationErrorCode[];
  static const char kCaptchaError[];
  static const char kCaptchaErrorCode[];
  static const char kServiceUnavailableError[];
  static const char kServiceUnavailableErrorCode[];
  static const char kErrorParam[];
  static const char kErrorUrlParam[];
  static const char kCaptchaUrlParam[];
  static const char kCaptchaTokenParam[];

  // Constants for parsing ClientOAuth errors.
  static const char kNeedsAdditional[];
  static const char kCaptcha[];
  static const char kTwoFactor[];

  // Constants for request/response for OAuth2 requests.
  static const char kAuthHeaderFormat[];
  static const char kOAuthHeaderFormat[];
  static const char kOAuth2BearerHeaderFormat[];
  static const char kDeviceIdHeaderFormat[];
  static const char kClientLoginToOAuth2CookiePartSecure[];
  static const char kClientLoginToOAuth2CookiePartHttpOnly[];
  static const char kClientLoginToOAuth2CookiePartCodePrefix[];
  static const int kClientLoginToOAuth2CookiePartCodePrefixLength;

  // Process the results of a ClientLogin fetch.
  void OnClientLoginFetched(const std::string& data,
                            const net::URLRequestStatus& status,
                            int response_code);

  void OnIssueAuthTokenFetched(const std::string& data,
                               const net::URLRequestStatus& status,
                               int response_code);

  void OnClientLoginToOAuth2Fetched(const std::string& data,
                                    const net::ResponseCookies& cookies,
                                    const net::URLRequestStatus& status,
                                    int response_code);

  void OnOAuth2TokenPairFetched(const std::string& data,
                                const net::URLRequestStatus& status,
                                int response_code);

  void OnOAuth2RevokeTokenFetched(const std::string& data,
                                  const net::URLRequestStatus& status,
                                  int response_code);

  void OnListAccountsFetched(const std::string& data,
                             const net::URLRequestStatus& status,
                             int response_code);

  void OnGetUserInfoFetched(const std::string& data,
                            const net::URLRequestStatus& status,
                            int response_code);

  void OnMergeSessionFetched(const std::string& data,
                             const net::URLRequestStatus& status,
                             int response_code);

  void OnUberAuthTokenFetch(const std::string& data,
                            const net::URLRequestStatus& status,
                            int response_code);

  void OnOAuthLoginFetched(const std::string& data,
                           const net::URLRequestStatus& status,
                           int response_code);

  // Tokenize the results of a ClientLogin fetch.
  static void ParseClientLoginResponse(const std::string& data,
                                       std::string* sid,
                                       std::string* lsid,
                                       std::string* token);

  static void ParseClientLoginFailure(const std::string& data,
                                      std::string* error,
                                      std::string* error_url,
                                      std::string* captcha_url,
                                      std::string* captcha_token);

  // Parse ClientLogin to OAuth2 response.
  static bool ParseClientLoginToOAuth2Response(
      const net::ResponseCookies& cookies,
      std::string* auth_code);

  static bool ParseClientLoginToOAuth2Cookie(const std::string& cookie,
                                             std::string* auth_code);

  // Is this a special case Gaia error for TwoFactor auth?
  static bool IsSecondFactorSuccess(const std::string& alleged_error);

  // Given parameters, create a ClientLogin request body.
  static std::string MakeClientLoginBody(
      const std::string& username,
      const std::string& password,
      const std::string& source,
      const char* const service,
      const std::string& login_token,
      const std::string& login_captcha,
      HostedAccountsSetting allow_hosted_accounts);
  // Supply the sid / lsid returned from ClientLogin in order to
  // request a long lived auth token for a service.
  static std::string MakeIssueAuthTokenBody(const std::string& sid,
                                            const std::string& lsid,
                                            const char* const service);
  // Create body to get OAuth2 auth code.
  static std::string MakeGetAuthCodeBody(bool include_device_type);
  // Given auth code, create body to get OAuth2 token pair.
  static std::string MakeGetTokenPairBody(const std::string& auth_code);
  // Given an OAuth2 token, create body to revoke the token.
  std::string MakeRevokeTokenBody(const std::string& auth_token);
  // Supply the lsid returned from ClientLogin in order to fetch
  // user information.
  static std::string MakeGetUserInfoBody(const std::string& lsid);

  // Supply the authentication token returned from StartIssueAuthToken.
  static std::string MakeMergeSessionBody(const std::string& auth_token,
                                       const std::string& continue_url,
                                       const std::string& source);

  static std::string MakeGetAuthCodeHeader(const std::string& auth_token);

  static std::string MakeOAuthLoginBody(const std::string& service,
                                        const std::string& source);

  // Create a fetcher usable for making any Gaia request.  |body| is used
  // as the body of the POST request sent to GAIA.  Any strings listed in
  // |headers| are added as extra HTTP headers in the request.
  //
  // |load_flags| are passed to directly to net::URLFetcher::Create() when
  // creating the URL fetcher.
  static net::URLFetcher* CreateGaiaFetcher(
      net::URLRequestContextGetter* getter,
      const std::string& body,
      const std::string& headers,
      const GURL& gaia_gurl,
      int load_flags,
      net::URLFetcherDelegate* delegate);

  // From a URLFetcher result, generate an appropriate error.
  // From the API documentation, both IssueAuthToken and ClientLogin have
  // the same error returns.
  static GoogleServiceAuthError GenerateAuthError(
      const std::string& data,
      const net::URLRequestStatus& status);

  // These fields are common to GaiaAuthFetcher, same every request
  GaiaAuthConsumer* const consumer_;
  net::URLRequestContextGetter* const getter_;
  std::string source_;
  const GURL client_login_gurl_;
  const GURL issue_auth_token_gurl_;
  const GURL oauth2_token_gurl_;
  const GURL oauth2_revoke_gurl_;
  const GURL get_user_info_gurl_;
  const GURL merge_session_gurl_;
  const GURL uberauth_token_gurl_;
  const GURL oauth_login_gurl_;
  const GURL list_accounts_gurl_;

  // While a fetch is going on:
  scoped_ptr<net::URLFetcher> fetcher_;
  GURL client_login_to_oauth2_gurl_;
  std::string request_body_;
  std::string requested_service_; // Currently tracked for IssueAuthToken only.
  bool fetch_pending_;

  friend class GaiaAuthFetcherTest;
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CaptchaParse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, AccountDeletedError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, AccountDisabledError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, BadAuthenticationError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, IncomprehensibleError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ServiceUnavailableError);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CheckNormalErrorCode);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, CheckTwoFactorResponse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, LoginNetFailure);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest,
      ParseClientLoginToOAuth2Response);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ParseOAuth2TokenPairResponse);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthSuccess);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthWithQuote);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthChallengeSuccess);
  FRIEND_TEST_ALL_PREFIXES(GaiaAuthFetcherTest, ClientOAuthChallengeQuote);

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcher);
};

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_FETCHER_H_
