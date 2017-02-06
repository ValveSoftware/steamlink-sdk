// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/web_history_service.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/sync_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/protocol/history_status.pb.h"
#include "ui/base/device_form_factor.h"
#include "url/gurl.h"

namespace history {

namespace {

const char kHistoryOAuthScope[] =
    "https://www.googleapis.com/auth/chromesync";

const char kHistoryQueryHistoryUrl[] =
    "https://history.google.com/history/api/lookup?client=chrome";

const char kHistoryDeleteHistoryUrl[] =
    "https://history.google.com/history/api/delete?client=chrome";

const char kHistoryAudioHistoryUrl[] =
    "https://history.google.com/history/api/lookup?client=audio";

const char kHistoryAudioHistoryChangeUrl[] =
    "https://history.google.com/history/api/change";

const char kQueryWebAndAppActivityUrl[] =
    "https://history.google.com/history/api/lookup?client=web_app";

const char kQueryOtherFormsOfBrowsingHistoryUrlSuffix[] = "/historystatus";

const char kPostDataMimeType[] = "text/plain";

const char kSyncProtoMimeType[] = "application/octet-stream";

// The maximum number of retries for the URLFetcher requests.
const size_t kMaxRetries = 1;

class RequestImpl : public WebHistoryService::Request,
                    private OAuth2TokenService::Consumer,
                    private net::URLFetcherDelegate {
 public:
  ~RequestImpl() override {}

  // Returns the response code received from the server, which will only be
  // valid if the request succeeded.
  int GetResponseCode() override { return response_code_; }

  // Returns the contents of the response body received from the server.
  const std::string& GetResponseBody() override { return response_body_; }

  bool IsPending() override { return is_pending_; }

 private:
  friend class history::WebHistoryService;

  RequestImpl(
      OAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const GURL& url,
      const WebHistoryService::CompletionCallback& callback)
      : OAuth2TokenService::Consumer("web_history"),
        token_service_(token_service),
        signin_manager_(signin_manager),
        request_context_(request_context),
        url_(url),
        post_data_mime_type_(kPostDataMimeType),
        response_code_(0),
        auth_retry_count_(0),
        callback_(callback),
        is_pending_(false) {
    DCHECK(token_service_);
    DCHECK(signin_manager_);
    DCHECK(request_context_);
  }

  // Tells the request to do its thang.
  void Start() override {
    OAuth2TokenService::ScopeSet oauth_scopes;
    oauth_scopes.insert(kHistoryOAuthScope);

    token_request_ = token_service_->StartRequest(
        signin_manager_->GetAuthenticatedAccountId(), oauth_scopes, this);
    is_pending_ = true;
  }

  // content::URLFetcherDelegate interface.
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    DCHECK_EQ(source, url_fetcher_.get());
    response_code_ = url_fetcher_->GetResponseCode();

    UMA_HISTOGRAM_CUSTOM_ENUMERATION("WebHistory.OAuthTokenResponseCode",
        net::HttpUtil::MapStatusCodeForHistogram(response_code_),
        net::HttpUtil::GetStatusCodesForHistogram());

    // If the response code indicates that the token might not be valid,
    // invalidate the token and try again.
    if (response_code_ == net::HTTP_UNAUTHORIZED && ++auth_retry_count_ <= 1) {
      OAuth2TokenService::ScopeSet oauth_scopes;
      oauth_scopes.insert(kHistoryOAuthScope);
      token_service_->InvalidateAccessToken(
          signin_manager_->GetAuthenticatedAccountId(), oauth_scopes,
          access_token_);

      access_token_.clear();
      Start();
      return;
    }
    url_fetcher_->GetResponseAsString(&response_body_);
    url_fetcher_.reset();
    is_pending_ = false;
    callback_.Run(this, true);
    // It is valid for the callback to delete |this|, so do not access any
    // members below here.
  }

  // OAuth2TokenService::Consumer interface.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override {
    token_request_.reset();
    DCHECK(!access_token.empty());
    access_token_ = access_token;

    UMA_HISTOGRAM_BOOLEAN("WebHistory.OAuthTokenCompletion", true);

    // Got an access token -- start the actual API request.
    url_fetcher_ = CreateUrlFetcher(access_token);
    url_fetcher_->Start();
  }

  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override {
    token_request_.reset();
    is_pending_ = false;

    UMA_HISTOGRAM_BOOLEAN("WebHistory.OAuthTokenCompletion", false);

    callback_.Run(this, false);
    // It is valid for the callback to delete |this|, so do not access any
    // members below here.
  }

  // Helper for creating a new URLFetcher for the API request.
  std::unique_ptr<net::URLFetcher> CreateUrlFetcher(
      const std::string& access_token) {
    net::URLFetcher::RequestType request_type = post_data_ ?
        net::URLFetcher::POST : net::URLFetcher::GET;
    std::unique_ptr<net::URLFetcher> fetcher =
        net::URLFetcher::Create(url_, request_type, this);
    fetcher->SetRequestContext(request_context_.get());
    fetcher->SetMaxRetriesOn5xx(kMaxRetries);
    fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                          net::LOAD_DO_NOT_SAVE_COOKIES);
    fetcher->AddExtraRequestHeader("Authorization: Bearer " + access_token);
    fetcher->AddExtraRequestHeader("X-Developer-Key: " +
        GaiaUrls::GetInstance()->oauth2_chrome_client_id());

    if (!user_agent_.empty()) {
      fetcher->AddExtraRequestHeader(
          std::string(net::HttpRequestHeaders::kUserAgent) +
          ": " + user_agent_);
    }

    if (post_data_)
      fetcher->SetUploadData(post_data_mime_type_, post_data_.value());
    return fetcher;
  }

  void SetPostData(const std::string& post_data) override {
    SetPostDataAndType(post_data, kPostDataMimeType);
  }

  void SetPostDataAndType(const std::string& post_data,
                          const std::string& mime_type) override {
    post_data_ = post_data;
    post_data_mime_type_ = mime_type;
  }

  void SetUserAgent(const std::string& user_agent) override {
    user_agent_ = user_agent;
  }

  OAuth2TokenService* token_service_;
  SigninManagerBase* signin_manager_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // The URL of the API endpoint.
  GURL url_;

  // POST data to be sent with the request (may be empty).
  base::Optional<std::string> post_data_;

  // MIME type of the post requests. Defaults to text/plain.
  std::string post_data_mime_type_;

  // The user agent header used with this request.
  std::string user_agent_;

  // The OAuth2 access token request.
  std::unique_ptr<OAuth2TokenService::Request> token_request_;

  // The current OAuth2 access token.
  std::string access_token_;

  // Handles the actual API requests after the OAuth token is acquired.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // Holds the response code received from the server.
  int response_code_;

  // Holds the response body received from the server.
  std::string response_body_;

  // The number of times this request has already been retried due to
  // authorization problems.
  int auth_retry_count_;

  // The callback to execute when the query is complete.
  WebHistoryService::CompletionCallback callback_;

  // True if the request was started and has not yet completed, otherwise false.
  bool is_pending_;
};

// Converts a time into a string for use as a parameter in a request to the
// history server.
std::string ServerTimeString(base::Time time) {
  if (time < base::Time::UnixEpoch()) {
    return base::Int64ToString(0);
  } else {
    return base::Int64ToString(
        (time - base::Time::UnixEpoch()).InMicroseconds());
  }
}

// Returns a URL for querying the history server for a query specified by
// |options|. |version_info|, if not empty, should be a token that was received
// from the server in response to a write operation. It is used to help ensure
// read consistency after a write.
GURL GetQueryUrl(const base::string16& text_query,
                 const QueryOptions& options,
                 const std::string& version_info) {
  GURL url = GURL(kHistoryQueryHistoryUrl);
  url = net::AppendQueryParameter(url, "titles", "1");

  // Take |begin_time|, |end_time|, and |max_count| from the original query
  // options, and convert them to the equivalent URL parameters.

  base::Time end_time =
      std::min(base::Time::FromInternalValue(options.EffectiveEndTime()),
               base::Time::Now());
  url = net::AppendQueryParameter(url, "max", ServerTimeString(end_time));

  if (!options.begin_time.is_null()) {
    url = net::AppendQueryParameter(
        url, "min", ServerTimeString(options.begin_time));
  }

  if (options.max_count) {
    url = net::AppendQueryParameter(
        url, "num", base::IntToString(options.max_count));
  }

  if (!text_query.empty())
    url = net::AppendQueryParameter(url, "q", base::UTF16ToUTF8(text_query));

  if (!version_info.empty())
    url = net::AppendQueryParameter(url, "kvi", version_info);

  return url;
}

// Creates a DictionaryValue to hold the parameters for a deletion.
// Ownership is passed to the caller.
// |url| may be empty, indicating a time-range deletion.
base::DictionaryValue* CreateDeletion(
    const std::string& min_time,
    const std::string& max_time,
    const GURL& url) {
  base::DictionaryValue* deletion = new base::DictionaryValue;
  deletion->SetString("type", "CHROME_HISTORY");
  if (url.is_valid())
    deletion->SetString("url", url.spec());
  deletion->SetString("min_timestamp_usec", min_time);
  deletion->SetString("max_timestamp_usec", max_time);
  return deletion;
}

}  // namespace

WebHistoryService::Request::Request() {
}

WebHistoryService::Request::~Request() {
}

WebHistoryService::WebHistoryService(
    OAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : token_service_(token_service),
      signin_manager_(signin_manager),
      request_context_(request_context),
      weak_ptr_factory_(this) {
}

WebHistoryService::~WebHistoryService() {
  STLDeleteElements(&pending_expire_requests_);
  STLDeleteElements(&pending_audio_history_requests_);
  STLDeleteElements(&pending_web_and_app_activity_requests_);
  STLDeleteElements(&pending_other_forms_of_browsing_history_requests_);
}

WebHistoryService::Request* WebHistoryService::CreateRequest(
    const GURL& url,
    const CompletionCallback& callback) {
  return new RequestImpl(token_service_, signin_manager_, request_context_, url,
                         callback);
}

// static
std::unique_ptr<base::DictionaryValue> WebHistoryService::ReadResponse(
    WebHistoryService::Request* request) {
  std::unique_ptr<base::DictionaryValue> result;
  if (request->GetResponseCode() == net::HTTP_OK) {
    std::unique_ptr<base::Value> value =
        base::JSONReader::Read(request->GetResponseBody());
    if (value.get() && value.get()->IsType(base::Value::TYPE_DICTIONARY))
      result.reset(static_cast<base::DictionaryValue*>(value.release()));
    else
      DLOG(WARNING) << "Non-JSON response received from history server.";
  }
  return result;
}

std::unique_ptr<WebHistoryService::Request> WebHistoryService::QueryHistory(
    const base::string16& text_query,
    const QueryOptions& options,
    const WebHistoryService::QueryWebHistoryCallback& callback) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback = base::Bind(
      &WebHistoryService::QueryHistoryCompletionCallback, callback);

  GURL url = GetQueryUrl(text_query, options, server_version_info_);
  std::unique_ptr<Request> request(CreateRequest(url, completion_callback));
  request->Start();
  return request;
}

void WebHistoryService::ExpireHistory(
    const std::vector<ExpireHistoryArgs>& expire_list,
    const ExpireWebHistoryCallback& callback) {
  base::DictionaryValue delete_request;
  std::unique_ptr<base::ListValue> deletions(new base::ListValue);
  base::Time now = base::Time::Now();

  for (const auto& expire : expire_list) {
    // Convert the times to server timestamps.
    std::string min_timestamp = ServerTimeString(expire.begin_time);
    // TODO(dubroy): Use sane time (crbug.com/146090) here when it's available.
    base::Time end_time = expire.end_time;
    if (end_time.is_null() || end_time > now)
      end_time = now;
    std::string max_timestamp = ServerTimeString(end_time);

    for (const auto& url : expire.urls) {
      deletions->Append(
          CreateDeletion(min_timestamp, max_timestamp, url));
    }
    // If no URLs were specified, delete everything in the time range.
    if (expire.urls.empty())
      deletions->Append(CreateDeletion(min_timestamp, max_timestamp, GURL()));
  }
  delete_request.Set("del", deletions.release());
  std::string post_data;
  base::JSONWriter::Write(delete_request, &post_data);

  GURL url(kHistoryDeleteHistoryUrl);

  // Append the version info token, if it is available, to help ensure
  // consistency with any previous deletions.
  if (!server_version_info_.empty())
    url = net::AppendQueryParameter(url, "kvi", server_version_info_);

  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback =
      base::Bind(&WebHistoryService::ExpireHistoryCompletionCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback);

  std::unique_ptr<Request> request(CreateRequest(url, completion_callback));
  request->SetPostData(post_data);
  request->Start();
  pending_expire_requests_.insert(request.release());
}

void WebHistoryService::ExpireHistoryBetween(
    const std::set<GURL>& restrict_urls,
    base::Time begin_time,
    base::Time end_time,
    const ExpireWebHistoryCallback& callback) {
  std::vector<ExpireHistoryArgs> expire_list(1);
  expire_list.back().urls = restrict_urls;
  expire_list.back().begin_time = begin_time;
  expire_list.back().end_time = end_time;
  ExpireHistory(expire_list, callback);
}

void WebHistoryService::GetAudioHistoryEnabled(
    const AudioWebHistoryCallback& callback) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback =
    base::Bind(&WebHistoryService::AudioHistoryCompletionCallback,
    weak_ptr_factory_.GetWeakPtr(),
    callback);

  GURL url(kHistoryAudioHistoryUrl);
  std::unique_ptr<Request> request(CreateRequest(url, completion_callback));
  request->Start();
  pending_audio_history_requests_.insert(request.release());
}

void WebHistoryService::SetAudioHistoryEnabled(
  bool new_enabled_value,
  const AudioWebHistoryCallback& callback) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback =
      base::Bind(&WebHistoryService::AudioHistoryCompletionCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback);

  GURL url(kHistoryAudioHistoryChangeUrl);
  std::unique_ptr<Request> request(CreateRequest(url, completion_callback));

  base::DictionaryValue enable_audio_history;
  enable_audio_history.SetBoolean("enable_history_recording",
                                  new_enabled_value);
  enable_audio_history.SetString("client", "audio");
  std::string post_data;
  base::JSONWriter::Write(enable_audio_history, &post_data);
  request->SetPostData(post_data);

  request->Start();
  pending_audio_history_requests_.insert(request.release());
}

size_t WebHistoryService::GetNumberOfPendingAudioHistoryRequests() {
  return pending_audio_history_requests_.size();
}

void WebHistoryService::QueryWebAndAppActivity(
    const QueryWebAndAppActivityCallback& callback) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback =
      base::Bind(&WebHistoryService::QueryWebAndAppActivityCompletionCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback);

  GURL url(kQueryWebAndAppActivityUrl);
  Request* request = CreateRequest(url, completion_callback);
  pending_web_and_app_activity_requests_.insert(request);
  request->Start();
}

void WebHistoryService::QueryOtherFormsOfBrowsingHistory(
    version_info::Channel channel,
    const QueryOtherFormsOfBrowsingHistoryCallback& callback) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback = base::Bind(
      &WebHistoryService::QueryOtherFormsOfBrowsingHistoryCompletionCallback,
      weak_ptr_factory_.GetWeakPtr(),
      callback);

  // Find the Sync request URL.
  GURL url =
      GetSyncServiceURL(*base::CommandLine::ForCurrentProcess(), channel);
  GURL::Replacements replace_path;
  std::string new_path =
      url.path() + kQueryOtherFormsOfBrowsingHistoryUrlSuffix;
  replace_path.SetPathStr(new_path);
  url = url.ReplaceComponents(replace_path);
  DCHECK(url.is_valid());

  Request* request = CreateRequest(url, completion_callback);

  // Set the Sync-specific user agent.
  std::string user_agent = MakeUserAgentForSync(
      channel, ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET);
  request->SetUserAgent(user_agent);

  pending_other_forms_of_browsing_history_requests_.insert(request);

  // Set the request protobuf.
  sync_pb::HistoryStatusRequest request_proto;
  std::string post_data;
  request_proto.SerializeToString(&post_data);
  request->SetPostDataAndType(post_data, kSyncProtoMimeType);

  request->Start();
}

// static
void WebHistoryService::QueryHistoryCompletionCallback(
    const WebHistoryService::QueryWebHistoryCallback& callback,
    WebHistoryService::Request* request,
    bool success) {
  std::unique_ptr<base::DictionaryValue> response_value;
  if (success)
    response_value = ReadResponse(request);
  callback.Run(request, response_value.get());
}

void WebHistoryService::ExpireHistoryCompletionCallback(
    const WebHistoryService::ExpireWebHistoryCallback& callback,
    WebHistoryService::Request* request,
    bool success) {
  pending_expire_requests_.erase(request);
  std::unique_ptr<Request> request_ptr(request);

  std::unique_ptr<base::DictionaryValue> response_value;
  if (success) {
    response_value = ReadResponse(request);
    if (response_value)
      response_value->GetString("version_info", &server_version_info_);
  }
  callback.Run(response_value.get() && success);
}

void WebHistoryService::AudioHistoryCompletionCallback(
    const WebHistoryService::AudioWebHistoryCallback& callback,
    WebHistoryService::Request* request,
    bool success) {
  pending_audio_history_requests_.erase(request);
  std::unique_ptr<WebHistoryService::Request> request_ptr(request);

  std::unique_ptr<base::DictionaryValue> response_value;
  bool enabled_value = false;
  if (success) {
    response_value = ReadResponse(request_ptr.get());
    if (response_value)
      response_value->GetBoolean("history_recording_enabled", &enabled_value);
  }

  // If there is no response_value, then for our purposes, the request has
  // failed, despite receiving a true |success| value. This can happen if
  // the user is offline.
  callback.Run(success && response_value, enabled_value);
}

void WebHistoryService::QueryWebAndAppActivityCompletionCallback(
    const WebHistoryService::QueryWebAndAppActivityCallback& callback,
    WebHistoryService::Request* request,
    bool success) {
  pending_web_and_app_activity_requests_.erase(request);
  std::unique_ptr<Request> request_ptr(request);

  std::unique_ptr<base::DictionaryValue> response_value;
  bool web_and_app_activity_enabled = false;

  if (success) {
    response_value = ReadResponse(request);
    if (response_value) {
      response_value->GetBoolean(
          "history_recording_enabled", &web_and_app_activity_enabled);
    }
  }

  callback.Run(web_and_app_activity_enabled);
}

void WebHistoryService::QueryOtherFormsOfBrowsingHistoryCompletionCallback(
    const WebHistoryService::QueryOtherFormsOfBrowsingHistoryCallback& callback,
    WebHistoryService::Request* request,
    bool success) {
  pending_other_forms_of_browsing_history_requests_.erase(request);
  std::unique_ptr<Request> request_ptr(request);

  bool has_other_forms_of_browsing_history = false;
  if (success && request->GetResponseCode() == net::HTTP_OK) {
    sync_pb::HistoryStatusResponse history_status;
    if (history_status.ParseFromString(request->GetResponseBody()))
      has_other_forms_of_browsing_history = history_status.has_derived_data();
  }

  callback.Run(has_other_forms_of_browsing_history);
}

}  // namespace history
