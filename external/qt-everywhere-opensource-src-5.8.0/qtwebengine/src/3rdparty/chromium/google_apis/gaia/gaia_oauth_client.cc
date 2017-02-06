// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_oauth_client.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace {
const char kAccessTokenValue[] = "access_token";
const char kRefreshTokenValue[] = "refresh_token";
const char kExpiresInValue[] = "expires_in";
}

namespace gaia {

// Use a non-zero number, so unit tests can differentiate the URLFetcher used by
// this class from other fetchers (most other code just hardcodes the ID to 0).
const int GaiaOAuthClient::kUrlFetcherId = 17109006;

class GaiaOAuthClient::Core
    : public base::RefCountedThreadSafe<GaiaOAuthClient::Core>,
      public net::URLFetcherDelegate {
 public:
  Core(net::URLRequestContextGetter* request_context_getter)
      : num_retries_(0),
        request_context_getter_(request_context_getter),
        delegate_(NULL),
        request_type_(NO_PENDING_REQUEST) {
  }

  void GetTokensFromAuthCode(const OAuthClientInfo& oauth_client_info,
                             const std::string& auth_code,
                             int max_retries,
                             GaiaOAuthClient::Delegate* delegate);
  void RefreshToken(const OAuthClientInfo& oauth_client_info,
                    const std::string& refresh_token,
                    const std::vector<std::string>& scopes,
                    int max_retries,
                    GaiaOAuthClient::Delegate* delegate);
  void GetUserEmail(const std::string& oauth_access_token,
                    int max_retries,
                    Delegate* delegate);
  void GetUserId(const std::string& oauth_access_token,
                 int max_retries,
                 Delegate* delegate);
  void GetUserInfo(const std::string& oauth_access_token,
                   int max_retries,
                   Delegate* delegate);
  void GetTokenInfo(const std::string& qualifier,
                    const std::string& query,
                    int max_retries,
                    Delegate* delegate);

  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 private:
  friend class base::RefCountedThreadSafe<Core>;

  enum RequestType {
    NO_PENDING_REQUEST,
    TOKENS_FROM_AUTH_CODE,
    REFRESH_TOKEN,
    TOKEN_INFO,
    USER_EMAIL,
    USER_ID,
    USER_INFO,
  };

  ~Core() override {}

  void GetUserInfoImpl(RequestType type,
                       const std::string& oauth_access_token,
                       int max_retries,
                       Delegate* delegate);
  void MakeGaiaRequest(const GURL& url,
                       const std::string& post_body,
                       int max_retries,
                       GaiaOAuthClient::Delegate* delegate);
  void HandleResponse(const net::URLFetcher* source,
                      bool* should_retry_request);

  int num_retries_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  GaiaOAuthClient::Delegate* delegate_;
  std::unique_ptr<net::URLFetcher> request_;
  RequestType request_type_;
};

void GaiaOAuthClient::Core::GetTokensFromAuthCode(
    const OAuthClientInfo& oauth_client_info,
    const std::string& auth_code,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate) {
  DCHECK_EQ(request_type_, NO_PENDING_REQUEST);
  request_type_ = TOKENS_FROM_AUTH_CODE;
  std::string post_body =
      "code=" + net::EscapeUrlEncodedData(auth_code, true) +
      "&client_id=" + net::EscapeUrlEncodedData(oauth_client_info.client_id,
                                                true) +
      "&client_secret=" +
      net::EscapeUrlEncodedData(oauth_client_info.client_secret, true) +
      "&redirect_uri=" +
      net::EscapeUrlEncodedData(oauth_client_info.redirect_uri, true) +
      "&grant_type=authorization_code";
  MakeGaiaRequest(GURL(GaiaUrls::GetInstance()->oauth2_token_url()),
                  post_body, max_retries, delegate);
}

void GaiaOAuthClient::Core::RefreshToken(
    const OAuthClientInfo& oauth_client_info,
    const std::string& refresh_token,
    const std::vector<std::string>& scopes,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate) {
  DCHECK_EQ(request_type_, NO_PENDING_REQUEST);
  request_type_ = REFRESH_TOKEN;
  std::string post_body =
      "refresh_token=" + net::EscapeUrlEncodedData(refresh_token, true) +
      "&client_id=" + net::EscapeUrlEncodedData(oauth_client_info.client_id,
                                                true) +
      "&client_secret=" +
      net::EscapeUrlEncodedData(oauth_client_info.client_secret, true) +
      "&grant_type=refresh_token";

  if (!scopes.empty()) {
    std::string scopes_string = base::JoinString(scopes, " ");
    post_body += "&scope=" + net::EscapeUrlEncodedData(scopes_string, true);
  }

  MakeGaiaRequest(GURL(GaiaUrls::GetInstance()->oauth2_token_url()),
                  post_body, max_retries, delegate);
}

void GaiaOAuthClient::Core::GetUserEmail(const std::string& oauth_access_token,
                                         int max_retries,
                                         Delegate* delegate) {
  GetUserInfoImpl(USER_EMAIL, oauth_access_token, max_retries, delegate);
}

void GaiaOAuthClient::Core::GetUserId(const std::string& oauth_access_token,
                                      int max_retries,
                                      Delegate* delegate) {
  GetUserInfoImpl(USER_ID, oauth_access_token, max_retries, delegate);
}

void GaiaOAuthClient::Core::GetUserInfo(const std::string& oauth_access_token,
                                      int max_retries,
                                      Delegate* delegate) {
  GetUserInfoImpl(USER_INFO, oauth_access_token, max_retries, delegate);
}

void GaiaOAuthClient::Core::GetUserInfoImpl(
    RequestType type,
    const std::string& oauth_access_token,
    int max_retries,
    Delegate* delegate) {
  DCHECK_EQ(request_type_, NO_PENDING_REQUEST);
  DCHECK(!request_.get());
  request_type_ = type;
  delegate_ = delegate;
  num_retries_ = 0;
  request_ = net::URLFetcher::Create(
      kUrlFetcherId, GURL(GaiaUrls::GetInstance()->oauth_user_info_url()),
      net::URLFetcher::GET, this);
  request_->SetRequestContext(request_context_getter_.get());
  request_->AddExtraRequestHeader("Authorization: OAuth " + oauth_access_token);
  request_->SetMaxRetriesOn5xx(max_retries);
  request_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES);

  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS. Retrying once should
  // be enough in those cases; let the fetcher retry up to 3 times just in case.
  // http://crbug.com/163710
  request_->SetAutomaticallyRetryOnNetworkChanges(3);
  request_->Start();
}

void GaiaOAuthClient::Core::GetTokenInfo(const std::string& qualifier,
                                         const std::string& query,
                                         int max_retries,
                                         Delegate* delegate) {
  DCHECK_EQ(request_type_, NO_PENDING_REQUEST);
  DCHECK(!request_.get());
  request_type_ = TOKEN_INFO;
  std::string post_body =
      qualifier + "=" + net::EscapeUrlEncodedData(query, true);
  MakeGaiaRequest(GURL(GaiaUrls::GetInstance()->oauth2_token_info_url()),
                  post_body,
                  max_retries,
                  delegate);
}

void GaiaOAuthClient::Core::MakeGaiaRequest(
    const GURL& url,
    const std::string& post_body,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate) {
  DCHECK(!request_.get()) << "Tried to fetch two things at once!";
  delegate_ = delegate;
  num_retries_ = 0;
  request_ =
      net::URLFetcher::Create(kUrlFetcherId, url, net::URLFetcher::POST, this);
  request_->SetRequestContext(request_context_getter_.get());
  request_->SetUploadData("application/x-www-form-urlencoded", post_body);
  request_->SetMaxRetriesOn5xx(max_retries);
  request_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  // See comment on SetAutomaticallyRetryOnNetworkChanges() above.
  request_->SetAutomaticallyRetryOnNetworkChanges(3);
  request_->Start();
}

// URLFetcher::Delegate implementation.
void GaiaOAuthClient::Core::OnURLFetchComplete(
    const net::URLFetcher* source) {
  bool should_retry = false;
  HandleResponse(source, &should_retry);
  if (should_retry) {
    // Explicitly call ReceivedContentWasMalformed() to ensure the current
    // request gets counted as a failure for calculation of the back-off
    // period.  If it was already a failure by status code, this call will
    // be ignored.
    request_->ReceivedContentWasMalformed();
    num_retries_++;
    // We must set our request_context_getter_ again because
    // URLFetcher::Core::RetryOrCompleteUrlFetch resets it to NULL...
    request_->SetRequestContext(request_context_getter_.get());
    request_->Start();
  }
}

void GaiaOAuthClient::Core::HandleResponse(
    const net::URLFetcher* source,
    bool* should_retry_request) {
  // Move ownership of the request fetcher into a local scoped_ptr which
  // will be nuked when we're done handling the request, unless we need
  // to retry, in which case ownership will be returned to request_.
  std::unique_ptr<net::URLFetcher> old_request = std::move(request_);
  DCHECK_EQ(source, old_request.get());

  // HTTP_BAD_REQUEST means the arguments are invalid.  HTTP_UNAUTHORIZED means
  // the access or refresh token is invalid. No point retrying. We are
  // done here.
  int response_code = source->GetResponseCode();
  if (response_code == net::HTTP_BAD_REQUEST ||
      response_code == net::HTTP_UNAUTHORIZED) {
    delegate_->OnOAuthError();
    return;
  }

  std::unique_ptr<base::DictionaryValue> response_dict;
  if (source->GetResponseCode() == net::HTTP_OK) {
    std::string data;
    source->GetResponseAsString(&data);
    std::unique_ptr<base::Value> message_value = base::JSONReader::Read(data);
    if (message_value.get() &&
        message_value->IsType(base::Value::TYPE_DICTIONARY)) {
      response_dict.reset(
          static_cast<base::DictionaryValue*>(message_value.release()));
    }
  }

  if (!response_dict.get()) {
    // If we don't have an access token yet and the the error was not
    // RC_BAD_REQUEST, we may need to retry.
    if ((source->GetMaxRetriesOn5xx() != -1) &&
        (num_retries_ >= source->GetMaxRetriesOn5xx())) {
      // Retry limit reached. Give up.
      delegate_->OnNetworkError(source->GetResponseCode());
    } else {
      request_ = std::move(old_request);
      *should_retry_request = true;
    }
    return;
  }

  RequestType type = request_type_;
  request_type_ = NO_PENDING_REQUEST;

  switch (type) {
    case USER_EMAIL: {
      std::string email;
      response_dict->GetString("email", &email);
      delegate_->OnGetUserEmailResponse(email);
      break;
    }

    case USER_ID: {
      std::string id;
      response_dict->GetString("id", &id);
      delegate_->OnGetUserIdResponse(id);
      break;
    }

    case USER_INFO: {
      delegate_->OnGetUserInfoResponse(std::move(response_dict));
      break;
    }

    case TOKEN_INFO: {
      delegate_->OnGetTokenInfoResponse(std::move(response_dict));
      break;
    }

    case TOKENS_FROM_AUTH_CODE:
    case REFRESH_TOKEN: {
      std::string access_token;
      std::string refresh_token;
      int expires_in_seconds = 0;
      response_dict->GetString(kAccessTokenValue, &access_token);
      response_dict->GetString(kRefreshTokenValue, &refresh_token);
      response_dict->GetInteger(kExpiresInValue, &expires_in_seconds);

      if (access_token.empty()) {
        delegate_->OnOAuthError();
        return;
      }

      if (type == REFRESH_TOKEN) {
        delegate_->OnRefreshTokenResponse(access_token, expires_in_seconds);
      } else {
        delegate_->OnGetTokensResponse(refresh_token,
                                       access_token,
                                       expires_in_seconds);
      }
      break;
    }

    default:
      NOTREACHED();
  }
}

GaiaOAuthClient::GaiaOAuthClient(net::URLRequestContextGetter* context_getter) {
  core_ = new Core(context_getter);
}

GaiaOAuthClient::~GaiaOAuthClient() {
}

void GaiaOAuthClient::GetTokensFromAuthCode(
    const OAuthClientInfo& oauth_client_info,
    const std::string& auth_code,
    int max_retries,
    Delegate* delegate) {
  return core_->GetTokensFromAuthCode(oauth_client_info,
                                      auth_code,
                                      max_retries,
                                      delegate);
}

void GaiaOAuthClient::RefreshToken(
    const OAuthClientInfo& oauth_client_info,
    const std::string& refresh_token,
    const std::vector<std::string>& scopes,
    int max_retries,
    Delegate* delegate) {
  return core_->RefreshToken(oauth_client_info,
                             refresh_token,
                             scopes,
                             max_retries,
                             delegate);
}

void GaiaOAuthClient::GetUserEmail(const std::string& access_token,
                                  int max_retries,
                                  Delegate* delegate) {
  return core_->GetUserEmail(access_token, max_retries, delegate);
}

void GaiaOAuthClient::GetUserId(const std::string& access_token,
                                int max_retries,
                                Delegate* delegate) {
  return core_->GetUserId(access_token, max_retries, delegate);
}

void GaiaOAuthClient::GetUserInfo(const std::string& access_token,
                                  int max_retries,
                                  Delegate* delegate) {
  return core_->GetUserInfo(access_token, max_retries, delegate);
}

void GaiaOAuthClient::GetTokenInfo(const std::string& access_token,
                                   int max_retries,
                                   Delegate* delegate) {
  return core_->GetTokenInfo("access_token", access_token, max_retries,
                             delegate);
}

void GaiaOAuthClient::GetTokenHandleInfo(const std::string& token_handle,
                                         int max_retries,
                                         Delegate* delegate) {
  return core_->GetTokenInfo("token_handle", token_handle, max_retries,
                             delegate);
}

}  // namespace gaia
