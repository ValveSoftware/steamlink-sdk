// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_gaia.h"

#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/url_parse.h"

#define REGISTER_RESPONSE_HANDLER(url, method) \
  request_handlers_.insert(std::make_pair( \
        url.path(), base::Bind(&FakeGaia::method, base::Unretained(this))))

#define REGISTER_PATH_RESPONSE_HANDLER(path, method) \
  request_handlers_.insert(std::make_pair( \
        path, base::Bind(&FakeGaia::method, base::Unretained(this))))

using namespace net::test_server;

namespace {

const base::FilePath::CharType kServiceLogin[] =
    FILE_PATH_LITERAL("google_apis/test/service_login.html");

// OAuth2 Authentication header value prefix.
const char kAuthHeaderBearer[] = "Bearer ";
const char kAuthHeaderOAuth[] = "OAuth ";

const char kListAccountsResponseFormat[] =
    "[\"gaia.l.a.r\",[[\"gaia.l.a\",1,\"\",\"%s\",\"\",1,1,0]]]";

typedef std::map<std::string, std::string> CookieMap;

// Parses cookie name-value map our of |request|.
CookieMap GetRequestCookies(const HttpRequest& request) {
  CookieMap result;
  std::map<std::string, std::string>::const_iterator iter =
           request.headers.find("Cookie");
  if (iter != request.headers.end()) {
    std::vector<std::string> cookie_nv_pairs;
    base::SplitString(iter->second, ' ', &cookie_nv_pairs);
    for(std::vector<std::string>::const_iterator cookie_line =
            cookie_nv_pairs.begin();
        cookie_line != cookie_nv_pairs.end();
        ++cookie_line) {
      std::vector<std::string> name_value;
      base::SplitString(*cookie_line, '=', &name_value);
      if (name_value.size() != 2)
        continue;

      std::string value = name_value[1];
      if (value.size() && value[value.size() - 1] == ';')
        value = value.substr(0, value.size() -1);

      result.insert(std::make_pair(name_value[0], value));
    }
  }
  return result;
}

// Extracts the |access_token| from authorization header of |request|.
bool GetAccessToken(const HttpRequest& request,
                    const char* auth_token_prefix,
                    std::string* access_token) {
  std::map<std::string, std::string>::const_iterator auth_header_entry =
      request.headers.find("Authorization");
  if (auth_header_entry != request.headers.end()) {
    if (StartsWithASCII(auth_header_entry->second, auth_token_prefix, true)) {
      *access_token = auth_header_entry->second.substr(
          strlen(auth_token_prefix));
      return true;
    }
  }

  return false;
}

void SetCookies(BasicHttpResponse* http_response,
                const std::string& sid_cookie,
                const std::string& lsid_cookie) {
  http_response->AddCustomHeader(
      "Set-Cookie",
      base::StringPrintf("SID=%s; Path=/; HttpOnly;", sid_cookie.c_str()));
  http_response->AddCustomHeader(
      "Set-Cookie",
      base::StringPrintf("LSID=%s; Path=/; HttpOnly;", lsid_cookie.c_str()));
}

}  // namespace

FakeGaia::AccessTokenInfo::AccessTokenInfo()
  : expires_in(3600) {}

FakeGaia::AccessTokenInfo::~AccessTokenInfo() {}

FakeGaia::MergeSessionParams::MergeSessionParams() {
}

FakeGaia::MergeSessionParams::~MergeSessionParams() {
}

FakeGaia::FakeGaia() {
  base::FilePath source_root_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
  CHECK(base::ReadFileToString(
      source_root_dir.Append(base::FilePath(kServiceLogin)),
      &service_login_response_));
}

FakeGaia::~FakeGaia() {}

void FakeGaia::SetMergeSessionParams(
    const MergeSessionParams& params) {
  merge_session_params_ = params;
}

void FakeGaia::Initialize() {
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  // Handles /MergeSession GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->merge_session_url(), HandleMergeSession);

  // Handles /o/oauth2/programmatic_auth GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->client_login_to_oauth2_url(), HandleProgramaticAuth);

  // Handles /ServiceLogin GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->service_login_url(), HandleServiceLogin);

  // Handles /OAuthLogin GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->oauth1_login_url(), HandleOAuthLogin);

  // Handles /ServiceLoginAuth GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->service_login_auth_url(), HandleServiceLoginAuth);

  // Handles /SSO GAIA call (not GAIA, made up for SAML tests).
  REGISTER_PATH_RESPONSE_HANDLER("/SSO", HandleSSO);

  // Handles /o/oauth2/token GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->oauth2_token_url(), HandleAuthToken);

  // Handles /oauth2/v2/tokeninfo GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->oauth2_token_info_url(), HandleTokenInfo);

  // Handles /oauth2/v2/IssueToken GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->oauth2_issue_token_url(), HandleIssueToken);

  // Handles /ListAccounts GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->list_accounts_url(), HandleListAccounts);
}

scoped_ptr<HttpResponse> FakeGaia::HandleRequest(const HttpRequest& request) {
  // The scheme and host of the URL is actually not important but required to
  // get a valid GURL in order to parse |request.relative_url|.
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_path = request_url.path();
  scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
  RequestHandlerMap::iterator iter = request_handlers_.find(request_path);
  if (iter != request_handlers_.end()) {
    LOG(WARNING) << "Serving request " << request_path;
    iter->second.Run(request, http_response.get());
  } else {
    LOG(ERROR) << "Unhandled request " << request_path;
    return scoped_ptr<HttpResponse>();      // Request not understood.
  }

  return http_response.PassAs<HttpResponse>();
}

void FakeGaia::IssueOAuthToken(const std::string& auth_token,
                               const AccessTokenInfo& token_info) {
  access_token_info_map_.insert(std::make_pair(auth_token, token_info));
}

void FakeGaia::RegisterSamlUser(const std::string& account_id,
                                const GURL& saml_idp) {
  saml_account_idp_map_[account_id] = saml_idp;
}

// static
bool FakeGaia::GetQueryParameter(const std::string& query,
                                 const std::string& key,
                                 std::string* value) {
  // Name and scheme actually don't matter, but are required to get a valid URL
  // for parsing.
  GURL query_url("http://localhost?" + query);
  return net::GetValueForKeyInQuery(query_url, key, value);
}

void FakeGaia::HandleMergeSession(const HttpRequest& request,
                                  BasicHttpResponse* http_response) {
  http_response->set_code(net::HTTP_UNAUTHORIZED);
  if (merge_session_params_.session_sid_cookie.empty() ||
      merge_session_params_.session_lsid_cookie.empty()) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return;
  }

  std::string uber_token;
  if (!GetQueryParameter(request.content, "uberauth", &uber_token) ||
      uber_token != merge_session_params_.gaia_uber_token) {
    LOG(ERROR) << "Missing or invalid 'uberauth' param in /MergeSession call";
    return;
  }

  std::string continue_url;
  if (!GetQueryParameter(request.content, "continue", &continue_url)) {
    LOG(ERROR) << "Missing or invalid 'continue' param in /MergeSession call";
    return;
  }

  std::string source;
  if (!GetQueryParameter(request.content, "source", &source)) {
    LOG(ERROR) << "Missing or invalid 'source' param in /MergeSession call";
    return;
  }

  SetCookies(http_response,
             merge_session_params_.session_sid_cookie,
             merge_session_params_.session_lsid_cookie);
  // TODO(zelidrag): Not used now.
  http_response->set_content("OK");
  http_response->set_code(net::HTTP_OK);
}

void FakeGaia::HandleProgramaticAuth(
    const HttpRequest& request,
    BasicHttpResponse* http_response) {
  http_response->set_code(net::HTTP_UNAUTHORIZED);
  if (merge_session_params_.auth_code.empty()) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return;
  }

  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  std::string scope;
  if (!GetQueryParameter(request.content, "scope", &scope) ||
      GaiaConstants::kOAuth1LoginScope != scope) {
    return;
  }

  CookieMap cookies = GetRequestCookies(request);
  CookieMap::const_iterator sid_iter = cookies.find("SID");
  if (sid_iter == cookies.end() ||
      sid_iter->second != merge_session_params_.auth_sid_cookie) {
    LOG(ERROR) << "/o/oauth2/programmatic_auth missing SID cookie";
    return;
  }
  CookieMap::const_iterator lsid_iter = cookies.find("LSID");
  if (lsid_iter == cookies.end() ||
      lsid_iter->second != merge_session_params_.auth_lsid_cookie) {
    LOG(ERROR) << "/o/oauth2/programmatic_auth missing LSID cookie";
    return;
  }

  std::string client_id;
  if (!GetQueryParameter(request.content, "client_id", &client_id) ||
      gaia_urls->oauth2_chrome_client_id() != client_id) {
    return;
  }

  http_response->AddCustomHeader(
      "Set-Cookie",
      base::StringPrintf(
          "oauth_code=%s; Path=/o/GetOAuth2Token; Secure; HttpOnly;",
          merge_session_params_.auth_code.c_str()));
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");
}

void FakeGaia::FormatJSONResponse(const base::DictionaryValue& response_dict,
                                  BasicHttpResponse* http_response) {
  std::string response_json;
  base::JSONWriter::Write(&response_dict, &response_json);
  http_response->set_content(response_json);
  http_response->set_code(net::HTTP_OK);
}

const FakeGaia::AccessTokenInfo* FakeGaia::FindAccessTokenInfo(
    const std::string& auth_token,
    const std::string& client_id,
    const std::string& scope_string) const {
  if (auth_token.empty() || client_id.empty())
    return NULL;

  std::vector<std::string> scope_list;
  base::SplitString(scope_string, ' ', &scope_list);
  ScopeSet scopes(scope_list.begin(), scope_list.end());

  for (AccessTokenInfoMap::const_iterator entry(
           access_token_info_map_.lower_bound(auth_token));
       entry != access_token_info_map_.upper_bound(auth_token);
       ++entry) {
    if (entry->second.audience == client_id &&
        (scope_string.empty() || entry->second.scopes == scopes)) {
      return &(entry->second);
    }
  }

  return NULL;
}

void FakeGaia::HandleServiceLogin(const HttpRequest& request,
                                  BasicHttpResponse* http_response) {
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(service_login_response_);
  http_response->set_content_type("text/html");
}

void FakeGaia::HandleOAuthLogin(const HttpRequest& request,
                                BasicHttpResponse* http_response) {
  http_response->set_code(net::HTTP_UNAUTHORIZED);
  if (merge_session_params_.gaia_uber_token.empty()) {
    http_response->set_code(net::HTTP_FORBIDDEN);
    return;
  }

  std::string access_token;
  if (!GetAccessToken(request, kAuthHeaderOAuth, &access_token)) {
    LOG(ERROR) << "/OAuthLogin missing access token in the header";
    return;
  }

  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_query = request_url.query();

  std::string source;
  if (!GetQueryParameter(request_query, "source", &source)) {
    LOG(ERROR) << "Missing 'source' param in /OAuthLogin call";
    return;
  }

  std::string issue_uberauth;
  if (GetQueryParameter(request_query, "issueuberauth", &issue_uberauth) &&
      issue_uberauth == "1") {
    http_response->set_content(merge_session_params_.gaia_uber_token);
    http_response->set_code(net::HTTP_OK);
    // Issue GAIA uber token.
  } else {
    LOG(FATAL) << "/OAuthLogin for SID/LSID is not supported";
  }
}

void FakeGaia::HandleServiceLoginAuth(const HttpRequest& request,
                                      BasicHttpResponse* http_response) {
  std::string continue_url =
      GaiaUrls::GetInstance()->service_login_url().spec();
  GetQueryParameter(request.content, "continue", &continue_url);

  std::string redirect_url = continue_url;

  std::string email;
  if (GetQueryParameter(request.content, "Email", &email) &&
      saml_account_idp_map_.find(email) != saml_account_idp_map_.end()) {
    GURL url(saml_account_idp_map_[email]);
    url = net::AppendQueryParameter(url, "SAMLRequest", "fake_request");
    url = net::AppendQueryParameter(url, "RelayState", continue_url);
    redirect_url = url.spec();
  } else if (!merge_session_params_.auth_sid_cookie.empty() &&
             !merge_session_params_.auth_lsid_cookie.empty()) {
    SetCookies(http_response,
               merge_session_params_.auth_sid_cookie,
               merge_session_params_.auth_lsid_cookie);
  }

  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url);
}

void FakeGaia::HandleSSO(const HttpRequest& request,
                         BasicHttpResponse* http_response) {
  if (!merge_session_params_.auth_sid_cookie.empty() &&
      !merge_session_params_.auth_lsid_cookie.empty()) {
    SetCookies(http_response,
               merge_session_params_.auth_sid_cookie,
               merge_session_params_.auth_lsid_cookie);
  }
  std::string relay_state;
  GetQueryParameter(request.content, "RelayState", &relay_state);
  std::string redirect_url = relay_state;
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url);
}

void FakeGaia::HandleAuthToken(const HttpRequest& request,
                               BasicHttpResponse* http_response) {
  std::string grant_type;
  std::string refresh_token;
  std::string client_id;
  std::string scope;
  std::string auth_code;
  const AccessTokenInfo* token_info = NULL;
  GetQueryParameter(request.content, "scope", &scope);

  if (!GetQueryParameter(request.content, "grant_type", &grant_type)) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    LOG(ERROR) << "No 'grant_type' param in /o/oauth2/token";
    return;
  }

  if (grant_type == "authorization_code") {
    if (!GetQueryParameter(request.content, "code", &auth_code) ||
        auth_code != merge_session_params_.auth_code) {
      http_response->set_code(net::HTTP_BAD_REQUEST);
      LOG(ERROR) << "No 'code' param in /o/oauth2/token";
      return;
    }

    if (GaiaConstants::kOAuth1LoginScope != scope) {
      http_response->set_code(net::HTTP_BAD_REQUEST);
      LOG(ERROR) << "Invalid scope for /o/oauth2/token - " << scope;
      return;
    }

    base::DictionaryValue response_dict;
    response_dict.SetString("refresh_token",
                            merge_session_params_.refresh_token);
    response_dict.SetString("access_token",
                            merge_session_params_.access_token);
    response_dict.SetInteger("expires_in", 3600);
    FormatJSONResponse(response_dict, http_response);
  } else if (GetQueryParameter(request.content,
                               "refresh_token",
                               &refresh_token) &&
             GetQueryParameter(request.content,
                               "client_id",
                               &client_id) &&
             (token_info = FindAccessTokenInfo(refresh_token,
                                              client_id,
                                              scope))) {
    base::DictionaryValue response_dict;
    response_dict.SetString("access_token", token_info->token);
    response_dict.SetInteger("expires_in", 3600);
    FormatJSONResponse(response_dict, http_response);
  } else {
    LOG(ERROR) << "Bad request for /o/oauth2/token - "
               << "refresh_token = " << refresh_token
               << ", scope = " << scope
               << ", client_id = " << client_id;
    http_response->set_code(net::HTTP_BAD_REQUEST);
  }
}

void FakeGaia::HandleTokenInfo(const HttpRequest& request,
                               BasicHttpResponse* http_response) {
  const AccessTokenInfo* token_info = NULL;
  std::string access_token;
  if (GetQueryParameter(request.content, "access_token", &access_token)) {
    for (AccessTokenInfoMap::const_iterator entry(
             access_token_info_map_.begin());
         entry != access_token_info_map_.end();
         ++entry) {
      if (entry->second.token == access_token) {
        token_info = &(entry->second);
        break;
      }
    }
  }

  if (token_info) {
    base::DictionaryValue response_dict;
    response_dict.SetString("issued_to", token_info->issued_to);
    response_dict.SetString("audience", token_info->audience);
    response_dict.SetString("user_id", token_info->user_id);
    std::vector<std::string> scope_vector(token_info->scopes.begin(),
                                          token_info->scopes.end());
    response_dict.SetString("scope", JoinString(scope_vector, " "));
    response_dict.SetInteger("expires_in", token_info->expires_in);
    response_dict.SetString("email", token_info->email);
    FormatJSONResponse(response_dict, http_response);
  } else {
    http_response->set_code(net::HTTP_BAD_REQUEST);
  }
}

void FakeGaia::HandleIssueToken(const HttpRequest& request,
                                BasicHttpResponse* http_response) {
  std::string access_token;
  std::string scope;
  std::string client_id;
  const AccessTokenInfo* token_info = NULL;
  if (GetAccessToken(request, kAuthHeaderBearer, &access_token) &&
      GetQueryParameter(request.content, "scope", &scope) &&
      GetQueryParameter(request.content, "client_id", &client_id) &&
      (token_info = FindAccessTokenInfo(access_token, client_id, scope))) {
    base::DictionaryValue response_dict;
    response_dict.SetString("issueAdvice", "auto");
    response_dict.SetString("expiresIn",
                            base::IntToString(token_info->expires_in));
    response_dict.SetString("token", token_info->token);
    FormatJSONResponse(response_dict, http_response);
  } else {
    http_response->set_code(net::HTTP_BAD_REQUEST);
  }
}

void FakeGaia::HandleListAccounts(const HttpRequest& request,
                                 BasicHttpResponse* http_response) {
  http_response->set_content(base::StringPrintf(
      kListAccountsResponseFormat, merge_session_params_.email.c_str()));
  http_response->set_code(net::HTTP_OK);
}
