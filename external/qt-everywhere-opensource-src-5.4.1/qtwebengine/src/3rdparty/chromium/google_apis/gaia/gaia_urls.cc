// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_urls.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/google_api_keys.h"

namespace {

// Gaia service constants
const char kDefaultGaiaUrl[] = "https://accounts.google.com";
const char kDefaultGoogleApisBaseUrl[] = "https://www.googleapis.com";

// API calls from accounts.google.com
const char kClientLoginUrlSuffix[] = "ClientLogin";
const char kServiceLoginUrlSuffix[] = "ServiceLogin";
const char kServiceLoginAuthUrlSuffix[] = "ServiceLoginAuth";
const char kServiceLogoutUrlSuffix[] = "Logout";
const char kIssueAuthTokenUrlSuffix[] = "IssueAuthToken";
const char kGetUserInfoUrlSuffix[] = "GetUserInfo";
const char kTokenAuthUrlSuffix[] = "TokenAuth";
const char kMergeSessionUrlSuffix[] = "MergeSession";
const char kOAuthGetAccessTokenUrlSuffix[] = "OAuthGetAccessToken";
const char kOAuthWrapBridgeUrlSuffix[] = "OAuthWrapBridge";
const char kOAuth1LoginUrlSuffix[] = "OAuthLogin";
const char kOAuthRevokeTokenUrlSuffix[] = "AuthSubRevokeToken";
const char kListAccountsSuffix[] = "ListAccounts?json=standard";
const char kEmbeddedSigninSuffix[] = "EmbeddedSignIn";
const char kAddAccountSuffix[] = "AddSession";

// API calls from accounts.google.com (LSO)
const char kGetOAuthTokenUrlSuffix[] = "o/oauth/GetOAuthToken/";
const char kClientLoginToOAuth2UrlSuffix[] = "o/oauth2/programmatic_auth";
const char kOAuth2AuthUrlSuffix[] = "o/oauth2/auth";
const char kOAuth2RevokeUrlSuffix[] = "o/oauth2/revoke";
const char kOAuth2TokenUrlSuffix[] = "o/oauth2/token";

// API calls from www.googleapis.com
const char kOAuth2IssueTokenUrlSuffix[] = "oauth2/v2/IssueToken";
const char kOAuth2TokenInfoUrlSuffix[] = "oauth2/v2/tokeninfo";
const char kOAuthUserInfoUrlSuffix[] = "oauth2/v1/userinfo";

void GetSwitchValueWithDefault(const char* switch_value,
                               const char* default_value,
                               std::string* output_value) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_value)) {
    *output_value = command_line->GetSwitchValueASCII(switch_value);
  } else {
    *output_value = default_value;
  }
}

GURL GetURLSwitchValueWithDefault(const char* switch_value,
                                  const char* default_value) {
  std::string string_value;
  GetSwitchValueWithDefault(switch_value, default_value, &string_value);
  const GURL result(string_value);
  DCHECK(result.is_valid());
  return result;
}


}  // namespace

GaiaUrls* GaiaUrls::GetInstance() {
  return Singleton<GaiaUrls>::get();
}

GaiaUrls::GaiaUrls() {
  gaia_url_ = GetURLSwitchValueWithDefault(switches::kGaiaUrl, kDefaultGaiaUrl);
  lso_origin_url_ =
      GetURLSwitchValueWithDefault(switches::kLsoUrl, kDefaultGaiaUrl);
  google_apis_origin_url_ = GetURLSwitchValueWithDefault(
      switches::kGoogleApisUrl, kDefaultGoogleApisBaseUrl);

  captcha_base_url_ =
      GURL("http://" + gaia_url_.host() +
           (gaia_url_.has_port() ? ":" + gaia_url_.port() : ""));

  oauth2_chrome_client_id_ =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  oauth2_chrome_client_secret_ =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);

  // URLs from accounts.google.com.
  client_login_url_ = gaia_url_.Resolve(kClientLoginUrlSuffix);
  service_login_url_ = gaia_url_.Resolve(kServiceLoginUrlSuffix);
  service_login_auth_url_ = gaia_url_.Resolve(kServiceLoginAuthUrlSuffix);
  service_logout_url_ = gaia_url_.Resolve(kServiceLogoutUrlSuffix);
  issue_auth_token_url_ = gaia_url_.Resolve(kIssueAuthTokenUrlSuffix);
  get_user_info_url_ = gaia_url_.Resolve(kGetUserInfoUrlSuffix);
  token_auth_url_ = gaia_url_.Resolve(kTokenAuthUrlSuffix);
  merge_session_url_ = gaia_url_.Resolve(kMergeSessionUrlSuffix);
  oauth_get_access_token_url_ =
      gaia_url_.Resolve(kOAuthGetAccessTokenUrlSuffix);
  oauth_wrap_bridge_url_ = gaia_url_.Resolve(kOAuthWrapBridgeUrlSuffix);
  oauth_revoke_token_url_ = gaia_url_.Resolve(kOAuthRevokeTokenUrlSuffix);
  oauth1_login_url_ = gaia_url_.Resolve(kOAuth1LoginUrlSuffix);
  list_accounts_url_ = gaia_url_.Resolve(kListAccountsSuffix);
  embedded_signin_url_ = gaia_url_.Resolve(kEmbeddedSigninSuffix);
  add_account_url_ = gaia_url_.Resolve(kAddAccountSuffix);

  // URLs from accounts.google.com (LSO).
  get_oauth_token_url_ = lso_origin_url_.Resolve(kGetOAuthTokenUrlSuffix);
  client_login_to_oauth2_url_ =
      lso_origin_url_.Resolve(kClientLoginToOAuth2UrlSuffix);
  oauth2_auth_url_ = lso_origin_url_.Resolve(kOAuth2AuthUrlSuffix);
  oauth2_token_url_ = lso_origin_url_.Resolve(kOAuth2TokenUrlSuffix);
  oauth2_revoke_url_ = lso_origin_url_.Resolve(kOAuth2RevokeUrlSuffix);

  // URLs from www.googleapis.com.
  oauth2_issue_token_url_ =
      google_apis_origin_url_.Resolve(kOAuth2IssueTokenUrlSuffix);
  oauth2_token_info_url_ =
      google_apis_origin_url_.Resolve(kOAuth2TokenInfoUrlSuffix);
  oauth_user_info_url_ =
      google_apis_origin_url_.Resolve(kOAuthUserInfoUrlSuffix);

  gaia_login_form_realm_ = gaia_url_;
}

GaiaUrls::~GaiaUrls() {
}

const GURL& GaiaUrls::gaia_url() const {
  return gaia_url_;
}

const GURL& GaiaUrls::captcha_base_url() const {
  return captcha_base_url_;
}

const GURL& GaiaUrls::client_login_url() const {
  return client_login_url_;
}

const GURL& GaiaUrls::service_login_url() const {
  return service_login_url_;
}

const GURL& GaiaUrls::service_login_auth_url() const {
  return service_login_auth_url_;
}

const GURL& GaiaUrls::service_logout_url() const {
  return service_logout_url_;
}

const GURL& GaiaUrls::issue_auth_token_url() const {
  return issue_auth_token_url_;
}

const GURL& GaiaUrls::get_user_info_url() const {
  return get_user_info_url_;
}

const GURL& GaiaUrls::token_auth_url() const {
  return token_auth_url_;
}

const GURL& GaiaUrls::merge_session_url() const {
  return merge_session_url_;
}

const GURL& GaiaUrls::get_oauth_token_url() const {
  return get_oauth_token_url_;
}

const GURL& GaiaUrls::oauth_get_access_token_url() const {
  return oauth_get_access_token_url_;
}

const GURL& GaiaUrls::oauth_wrap_bridge_url() const {
  return oauth_wrap_bridge_url_;
}

const GURL& GaiaUrls::oauth_user_info_url() const {
  return oauth_user_info_url_;
}

const GURL& GaiaUrls::oauth_revoke_token_url() const {
  return oauth_revoke_token_url_;
}

const GURL& GaiaUrls::oauth1_login_url() const {
  return oauth1_login_url_;
}

const GURL& GaiaUrls::list_accounts_url() const {
  return list_accounts_url_;
}

const GURL& GaiaUrls::embedded_signin_url() const {
  return embedded_signin_url_;
}

const GURL& GaiaUrls::add_account_url() const {
  return add_account_url_;
}

const std::string& GaiaUrls::oauth2_chrome_client_id() const {
  return oauth2_chrome_client_id_;
}

const std::string& GaiaUrls::oauth2_chrome_client_secret() const {
  return oauth2_chrome_client_secret_;
}

const GURL& GaiaUrls::client_login_to_oauth2_url() const {
  return client_login_to_oauth2_url_;
}

const GURL& GaiaUrls::oauth2_auth_url() const {
  return oauth2_auth_url_;
}

const GURL& GaiaUrls::oauth2_token_url() const {
  return oauth2_token_url_;
}

const GURL& GaiaUrls::oauth2_issue_token_url() const {
  return oauth2_issue_token_url_;
}

const GURL& GaiaUrls::oauth2_token_info_url() const {
  return oauth2_token_info_url_;
}

const GURL& GaiaUrls::oauth2_revoke_url() const {
  return oauth2_revoke_url_;
}

const GURL& GaiaUrls::gaia_login_form_realm() const {
  return gaia_url_;
}
