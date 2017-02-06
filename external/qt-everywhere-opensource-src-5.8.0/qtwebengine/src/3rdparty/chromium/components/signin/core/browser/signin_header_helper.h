// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_

#include <string>

#include "build/build_config.h"  // For OS_IOS

namespace content_settings {
class CookieSettings;
}

namespace net {
class URLRequest;
}

class GURL;

namespace signin {

// Profile mode flags.
enum ProfileMode {
  PROFILE_MODE_DEFAULT = 0,
  // Incognito mode disabled by enterprise policy or by parental controls.
  PROFILE_MODE_INCOGNITO_DISABLED = 1 << 0,
  // Adding account disabled in the Android-for-EDU mode.
  PROFILE_MODE_ADD_ACCOUNT_DISABLED = 1 << 1
};

// The ServiceType specified by GAIA in the response header accompanying the 204
// response. This indicates the action Chrome is supposed to lead the user to
// perform.
enum GAIAServiceType {
  GAIA_SERVICE_TYPE_NONE = 0,    // No GAIA response header.
  GAIA_SERVICE_TYPE_SIGNOUT,     // Logout all existing sessions.
  GAIA_SERVICE_TYPE_INCOGNITO,   // Open an incognito tab.
  GAIA_SERVICE_TYPE_ADDSESSION,  // Add a secondary account.
  GAIA_SERVICE_TYPE_REAUTH,      // Re-authenticate an account.
  GAIA_SERVICE_TYPE_SIGNUP,      // Create a new account.
  GAIA_SERVICE_TYPE_DEFAULT,     // All other cases.
};

// Struct describing the paramters received in the manage account header.
struct ManageAccountsParams {
  // The requested service type such as "ADDSESSION".
  GAIAServiceType service_type;
  // The prefilled email.
  std::string email;
  // Whether |email| is a saml account.
  bool is_saml;
  // The continue URL after the requested service is completed successfully.
  // Defaults to the current URL if empty.
  std::string continue_url;
  // Whether the continue URL should be loaded in the same tab.
  bool is_same_tab;

// iOS has no notion of route and child IDs.
#if !defined(OS_IOS)
  // The child id associated with the web content of the request.
  int child_id;
  // The route id associated with the web content of the request.
  int route_id;
#endif  // !defined(OS_IOS)

  ManageAccountsParams();
  ManageAccountsParams(const ManageAccountsParams& other);
};

// Returns true if signin cookies are allowed.
bool SettingsAllowSigninCookies(
    const content_settings::CookieSettings* cookie_settings);

// Returns the X-CHROME-CONNECTED cookie, or an empty string if it should not be
// added to the request to |url|.
std::string BuildMirrorRequestCookieIfPossible(
    const GURL& url,
    const std::string& account_id,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask);

// Adds X-Chrome-Connected header to all Gaia requests from a connected profile,
// with the exception of requests from gaia webview.
// Returns true if the account management header was added to the request.
bool AppendMirrorRequestHeaderIfPossible(
    net::URLRequest* request,
    const GURL& redirect_url,
    const std::string& account_id,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask);

// Returns the parameters contained in the X-Chrome-Manage-Accounts response
// header.
ManageAccountsParams BuildManageAccountsParams(const std::string& header_value);

// Returns the parameters contained in the X-Chrome-Manage-Accounts response
// header.
// If the request does not have a response header or if the header contains
// garbage, then |service_type| is set to |GAIA_SERVICE_TYPE_NONE|.
ManageAccountsParams BuildManageAccountsParamsIfExists(net::URLRequest* request,
                                                       bool is_off_the_record);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_
