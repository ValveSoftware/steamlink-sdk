// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_fetcher.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/common/unicode/utypes.h"

using net::URLFetcher;
using net::URLRequestContextGetter;
using net::HttpRequestHeaders;
using net::URLRequestStatus;

namespace ntp_snippets {

namespace {

const char kChromeReaderApiScope[] =
    "https://www.googleapis.com/auth/webhistory";
const char kContentSuggestionsApiScope[] =
    "https://www.googleapis.com/auth/chrome-content-suggestions";
const char kChromeReaderServer[] =
    "https://chromereader-pa.googleapis.com/v1/fetch";
const char kContentSuggestionsServer[] =
    "https://chromecontentsuggestions-pa.googleapis.com/v1/snippets/list";
const char kContentSuggestionsSandboxServer[] =
    "https://chromecontentsuggestions-pa.sandbox.googleapis.com/v1/snippets/"
    "list";
const char kSnippetsServerNonAuthorizedFormat[] = "%s?key=%s";
const char kAuthorizationRequestHeaderFormat[] = "Bearer %s";

// Variation parameter for personalizing fetching of snippets.
const char kPersonalizationName[] = "fetching_personalization";
// Variation parameter for setting whether to restrict to a passed set of hosts.
const char kHostRestrictionName[] = "fetching_host_restrict";

// Variation parameter for chrome-content-suggestions backend.
const char kContentSuggestionsBackend[] = "content_suggestions_backend";

// Constants for possible values of the "fetching_personalization" parameter.
const char kPersonalizationPersonalString[] = "personal";
const char kPersonalizationNonPersonalString[] = "non_personal";
const char kPersonalizationBothString[] = "both";  // the default value

// Constants for possible values of the "fetching_host_restrict" parameter.
const char kHostRestrictionOnString[] = "on";  // the default value
const char kHostRestrictionOffString[] = "off";

std::string FetchResultToString(NTPSnippetsFetcher::FetchResult result) {
  switch (result) {
    case NTPSnippetsFetcher::FetchResult::SUCCESS:
      return "OK";
    case NTPSnippetsFetcher::FetchResult::EMPTY_HOSTS:
      return "Cannot fetch for empty hosts list.";
    case NTPSnippetsFetcher::FetchResult::URL_REQUEST_STATUS_ERROR:
      return "URLRequestStatus error";
    case NTPSnippetsFetcher::FetchResult::HTTP_ERROR:
      return "HTTP error";
    case NTPSnippetsFetcher::FetchResult::JSON_PARSE_ERROR:
      return "Received invalid JSON";
    case NTPSnippetsFetcher::FetchResult::INVALID_SNIPPET_CONTENT_ERROR:
      return "Invalid / empty list.";
    case NTPSnippetsFetcher::FetchResult::OAUTH_TOKEN_ERROR:
      return "Error in obtaining an OAuth2 access token.";
    case NTPSnippetsFetcher::FetchResult::RESULT_MAX:
      break;
  }
  NOTREACHED();
  return "Unknown error";
}

std::string GetFetchEndpoint() {
  std::string endpoint = variations::GetVariationParamValue(
      ntp_snippets::kStudyName, kContentSuggestionsBackend);
  return endpoint.empty() ? kChromeReaderServer : endpoint;
}

bool UsesChromeContentSuggestionsAPI(const GURL& endpoint) {
  if (endpoint == GURL(kChromeReaderServer)) {
    return false;
  } else if (endpoint != GURL(kContentSuggestionsServer) &&
             endpoint != GURL(kContentSuggestionsSandboxServer)) {
    LOG(WARNING) << "Unknown value for " << kContentSuggestionsBackend << ": "
      << "assuming chromecontentsuggestions-style API";
  }
  return true;
}

// Creates snippets from dictionary values in |list| and adds them to
// |snippets|. Returns true on success, false if anything went wrong.
bool AddSnippetsFromListValue(bool content_suggestions_api,
                              const base::ListValue& list,
                              NTPSnippet::PtrVector* snippets) {
  for (const auto& value : list) {
    const base::DictionaryValue* dict = nullptr;
    if (!value->GetAsDictionary(&dict))
      return false;

    std::unique_ptr<NTPSnippet> snippet;
    if (content_suggestions_api) {
      snippet = NTPSnippet::CreateFromContentSuggestionsDictionary(*dict);
    } else {
      snippet = NTPSnippet::CreateFromChromeReaderDictionary(*dict);
    }
    if (!snippet)
      return false;

    snippets->push_back(std::move(snippet));
  }
  return true;
}

// Translate the BCP 47 |language_code| into a posix locale string.
std::string PosixLocaleFromBCP47Language(const std::string& language_code) {
  char locale[ULOC_FULLNAME_CAPACITY];
  UErrorCode error = U_ZERO_ERROR;
  // Translate the input to a posix locale.
  uloc_forLanguageTag(language_code.c_str(), locale, ULOC_FULLNAME_CAPACITY,
                      nullptr, &error);
  DLOG_IF(WARNING, U_ZERO_ERROR != error)
      << "Error in translating language code to a locale string: " << error;
  return locale;
}

}  // namespace

NTPSnippetsFetcher::NTPSnippetsFetcher(
    SigninManagerBase* signin_manager,
    OAuth2TokenService* token_service,
    scoped_refptr<URLRequestContextGetter> url_request_context_getter,
    const ParseJSONCallback& parse_json_callback,
    bool is_stable_channel)
    : OAuth2TokenService::Consumer("ntp_snippets"),
      signin_manager_(signin_manager),
      token_service_(token_service),
      waiting_for_refresh_token_(false),
      url_request_context_getter_(url_request_context_getter),
      parse_json_callback_(parse_json_callback),
      fetch_url_(GetFetchEndpoint()),
      fetch_api_(UsesChromeContentSuggestionsAPI(fetch_url_)
                     ? CHROME_CONTENT_SUGGESTIONS_API
                     : CHROME_READER_API),
      is_stable_channel_(is_stable_channel),
      tick_clock_(new base::DefaultTickClock()),
      weak_ptr_factory_(this) {
  // Parse the variation parameters and set the defaults if missing.
  std::string personalization = variations::GetVariationParamValue(
      ntp_snippets::kStudyName, kPersonalizationName);
  if (personalization == kPersonalizationNonPersonalString) {
    personalization_ = Personalization::kNonPersonal;
  } else if (personalization == kPersonalizationPersonalString) {
    personalization_ = Personalization::kPersonal;
  } else {
    personalization_ = Personalization::kBoth;
    LOG_IF(WARNING, !personalization.empty() &&
                        personalization != kPersonalizationBothString)
        << "Unknown value for " << kPersonalizationName << ": "
        << personalization;
  }

  std::string host_restriction = variations::GetVariationParamValue(
      ntp_snippets::kStudyName, kHostRestrictionName);
  if (host_restriction == kHostRestrictionOnString) {
    use_host_restriction_ = true;
  } else {
    use_host_restriction_ = false;
    LOG_IF(WARNING, !host_restriction.empty() &&
                        host_restriction != kHostRestrictionOffString)
        << "Unknown value for " << kHostRestrictionName << ": "
        << host_restriction;
  }
}

NTPSnippetsFetcher::~NTPSnippetsFetcher() {
  if (waiting_for_refresh_token_)
    token_service_->RemoveObserver(this);
}

void NTPSnippetsFetcher::SetCallback(
    const SnippetsAvailableCallback& callback) {
  snippets_available_callback_ = callback;
}

void NTPSnippetsFetcher::FetchSnippetsFromHosts(
    const std::set<std::string>& hosts,
    const std::string& language_code,
    int count) {
  hosts_ = hosts;
  fetch_start_time_ = tick_clock_->NowTicks();

  if (UsesHostRestrictions() && hosts_.empty()) {
    FetchFinished(OptionalSnippets(), FetchResult::EMPTY_HOSTS,
                  /*extra_message=*/std::string());
    return;
  }

  locale_ = PosixLocaleFromBCP47Language(language_code);
  count_to_fetch_ = count;

  bool use_authentication = UsesAuthentication();

  if (use_authentication && signin_manager_->IsAuthenticated()) {
    // Signed-in: get OAuth token --> fetch snippets.
    StartTokenRequest();
  } else if (use_authentication && signin_manager_->AuthInProgress()) {
    // Currently signing in: wait for auth to finish (the refresh token) -->
    //     get OAuth token --> fetch snippets.
    if (!waiting_for_refresh_token_) {
      // Wait until we get a refresh token.
      waiting_for_refresh_token_ = true;
      token_service_->AddObserver(this);
    }
  } else {
    // Not signed in: fetch snippets (without authentication).
    FetchSnippetsNonAuthenticated();
  }
}

NTPSnippetsFetcher::RequestParams::RequestParams()
    : fetch_api(),
      obfuscated_gaia_id(),
      only_return_personalized_results(),
      user_locale(),
      host_restricts(),
      count_to_fetch() {}

NTPSnippetsFetcher::RequestParams::~RequestParams() = default;

std::string NTPSnippetsFetcher::RequestParams::BuildRequest() {
  auto request = base::MakeUnique<base::DictionaryValue>();
  if (fetch_api == CHROME_READER_API) {
    auto content_params = base::MakeUnique<base::DictionaryValue>();
    content_params->SetBoolean("only_return_personalized_results",
                               only_return_personalized_results);

    auto content_restricts = base::MakeUnique<base::ListValue>();
    for (const auto& metadata : {"TITLE", "SNIPPET", "THUMBNAIL"}) {
      auto entry = base::MakeUnique<base::DictionaryValue>();
      entry->SetString("type", "METADATA");
      entry->SetString("value", metadata);
      content_restricts->Append(std::move(entry));
    }

    auto content_selectors = base::MakeUnique<base::ListValue>();
    for (const auto& host : host_restricts) {
      auto entry = base::MakeUnique<base::DictionaryValue>();
      entry->SetString("type", "HOST_RESTRICT");
      entry->SetString("value", host);
      content_selectors->Append(std::move(entry));
    }

    auto local_scoring_params = base::MakeUnique<base::DictionaryValue>();
    local_scoring_params->Set("content_params", std::move(content_params));
    local_scoring_params->Set("content_restricts",
                              std::move(content_restricts));
    local_scoring_params->Set("content_selectors",
                              std::move(content_selectors));

    auto global_scoring_params = base::MakeUnique<base::DictionaryValue>();
    global_scoring_params->SetInteger("num_to_return", count_to_fetch);
    global_scoring_params->SetInteger("sort_type", 1);

    auto advanced = base::MakeUnique<base::DictionaryValue>();
    advanced->Set("local_scoring_params", std::move(local_scoring_params));
    advanced->Set("global_scoring_params", std::move(global_scoring_params));

    request->SetString("response_detail_level", "STANDARD");
    request->Set("advanced_options", std::move(advanced));
    if (!obfuscated_gaia_id.empty()) {
      request->SetString("obfuscated_gaia_id", obfuscated_gaia_id);
    }
    if (!user_locale.empty()) {
      request->SetString("user_locale", user_locale);
    }
  } else {
    if (!user_locale.empty()) {
      request->SetString("uiLanguage", user_locale);
    }
    auto regular_hosts = base::MakeUnique<base::ListValue>();
    for (const auto& host : host_restricts) {
      regular_hosts->AppendString(host);
    }
    request->Set("regularlyVisitedHostName", std::move(regular_hosts));

    // TODO(sfiera): support authentication and personalization
    // TODO(sfiera): support count_to_fetch
  }

  std::string request_json;
  bool success = base::JSONWriter::WriteWithOptions(
      *request, base::JSONWriter::OPTIONS_PRETTY_PRINT, &request_json);
  DCHECK(success);
  return request_json;
}

void NTPSnippetsFetcher::FetchSnippetsImpl(const GURL& url,
                                           const std::string& auth_header,
                                           const std::string& request) {
  url_fetcher_ = URLFetcher::Create(url, URLFetcher::POST, this);

  url_fetcher_->SetRequestContext(url_request_context_getter_.get());
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES);

  data_use_measurement::DataUseUserData::AttachToFetcher(
      url_fetcher_.get(), data_use_measurement::DataUseUserData::NTP_SNIPPETS);

  HttpRequestHeaders headers;
  if (!auth_header.empty())
    headers.SetHeader("Authorization", auth_header);
  headers.SetHeader("Content-Type", "application/json; charset=UTF-8");
  // Add X-Client-Data header with experiment IDs from field trials.
  variations::AppendVariationHeaders(url,
                                     false,  // incognito
                                     false,  // uma_enabled
                                     &headers);
  url_fetcher_->SetExtraRequestHeaders(headers.ToString());
  url_fetcher_->SetUploadData("application/json", request);
  // Log the request for debugging network issues.
  VLOG(1) << "Sending a NTP snippets request to " << url << ":" << std::endl
          << headers.ToString() << std::endl << request;
  // Fetchers are sometimes cancelled because a network change was detected.
  url_fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);
  // Try to make fetching the files bit more robust even with poor connection.
  url_fetcher_->SetMaxRetriesOn5xx(3);
  url_fetcher_->Start();
}

bool NTPSnippetsFetcher::UsesHostRestrictions() const {
  return use_host_restriction_ &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDontRestrict);
}

bool NTPSnippetsFetcher::UsesAuthentication() const {
  return (personalization_ == Personalization::kPersonal ||
          personalization_ == Personalization::kBoth);
}

void NTPSnippetsFetcher::FetchSnippetsNonAuthenticated() {
  // When not providing OAuth token, we need to pass the Google API key.
  const std::string& key = is_stable_channel_
                               ? google_apis::GetAPIKey()
                               : google_apis::GetNonStableAPIKey();
  GURL url(base::StringPrintf(kSnippetsServerNonAuthorizedFormat,
                              fetch_url_.spec().c_str(), key.c_str()));

  RequestParams params;
  params.fetch_api = fetch_api_;
  params.host_restricts =
      UsesHostRestrictions() ? hosts_ : std::set<std::string>();
  params.count_to_fetch = count_to_fetch_;
  FetchSnippetsImpl(url, std::string(), params.BuildRequest());
}

void NTPSnippetsFetcher::FetchSnippetsAuthenticated(
    const std::string& account_id,
    const std::string& oauth_access_token) {
  RequestParams params;
  params.fetch_api = fetch_api_;
  params.obfuscated_gaia_id = account_id;
  params.only_return_personalized_results =
      personalization_ == Personalization::kPersonal;
  params.user_locale = locale_;
  params.host_restricts =
      UsesHostRestrictions() ? hosts_ : std::set<std::string>();
  params.count_to_fetch = count_to_fetch_;
  FetchSnippetsImpl(fetch_url_,
                    base::StringPrintf(kAuthorizationRequestHeaderFormat,
                                       oauth_access_token.c_str()),
                    params.BuildRequest());
}

void NTPSnippetsFetcher::StartTokenRequest() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(fetch_api_ == CHROME_CONTENT_SUGGESTIONS_API
                    ? kContentSuggestionsApiScope
                    : kChromeReaderApiScope);
  oauth_request_ = token_service_->StartRequest(
      signin_manager_->GetAuthenticatedAccountId(), scopes, this);
}

////////////////////////////////////////////////////////////////////////////////
// OAuth2TokenService::Consumer overrides
void NTPSnippetsFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  // Delete the request after we leave this method.
  std::unique_ptr<OAuth2TokenService::Request> oauth_request(
      std::move(oauth_request_));
  DCHECK_EQ(oauth_request.get(), request)
      << "Got tokens from some previous request";

  FetchSnippetsAuthenticated(oauth_request->GetAccountId(), access_token);
}

void NTPSnippetsFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  oauth_request_.reset();
  DLOG(ERROR) << "Unable to get token: " << error.ToString()
              << " - fetching the snippets without authentication.";
  FetchFinished(
      OptionalSnippets(), FetchResult::OAUTH_TOKEN_ERROR,
      /*extra_message=*/base::StringPrintf(" (%s)", error.ToString().c_str()));
}

////////////////////////////////////////////////////////////////////////////////
// OAuth2TokenService::Observer overrides
void NTPSnippetsFetcher::OnRefreshTokenAvailable(
    const std::string& account_id) {
  // Only react on tokens for the account the user has signed in with.
  if (account_id != signin_manager_->GetAuthenticatedAccountId())
    return;

  token_service_->RemoveObserver(this);
  waiting_for_refresh_token_ = false;
  StartTokenRequest();
}

////////////////////////////////////////////////////////////////////////////////
// URLFetcherDelegate overrides
void NTPSnippetsFetcher::OnURLFetchComplete(const URLFetcher* source) {
  DCHECK_EQ(url_fetcher_.get(), source);

  const URLRequestStatus& status = source->GetStatus();

  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "NewTabPage.Snippets.FetchHttpResponseOrErrorCode",
      status.is_success() ? source->GetResponseCode() : status.error());

  if (!status.is_success()) {
    FetchFinished(OptionalSnippets(), FetchResult::URL_REQUEST_STATUS_ERROR,
                  /*extra_message=*/base::StringPrintf(" %d", status.error()));
  } else if (source->GetResponseCode() != net::HTTP_OK) {
    // TODO(jkrcal): https://crbug.com/609084
    // We need to deal with the edge case again where the auth
    // token expires just before we send the request (in which case we need to
    // fetch a new auth token). We should extract that into a common class
    // instead of adding it to every single class that uses auth tokens.
    FetchFinished(
        OptionalSnippets(), FetchResult::HTTP_ERROR,
        /*extra_message=*/base::StringPrintf(" %d", source->GetResponseCode()));
  } else {
    bool stores_result_to_string = source->GetResponseAsString(
        &last_fetch_json_);
    DCHECK(stores_result_to_string);

    parse_json_callback_.Run(
        last_fetch_json_,
        base::Bind(&NTPSnippetsFetcher::OnJsonParsed,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&NTPSnippetsFetcher::OnJsonError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void NTPSnippetsFetcher::OnJsonParsed(std::unique_ptr<base::Value> parsed) {
  const base::DictionaryValue* top_dict = nullptr;
  const base::ListValue* list = nullptr;
  NTPSnippet::PtrVector snippets;
  const std::string list_key =
      fetch_api_ == CHROME_CONTENT_SUGGESTIONS_API ? "snippet" : "recos";
  if (!parsed->GetAsDictionary(&top_dict) ||
      !top_dict->GetList(list_key, &list) ||
      !AddSnippetsFromListValue(fetch_api_ == CHROME_CONTENT_SUGGESTIONS_API,
                                *list, &snippets)) {
    LOG(WARNING) << "Received invalid snippets: " << last_fetch_json_;
    FetchFinished(OptionalSnippets(),
                  FetchResult::INVALID_SNIPPET_CONTENT_ERROR,
                  /*extra_message=*/std::string());
  } else {
    FetchFinished(OptionalSnippets(std::move(snippets)), FetchResult::SUCCESS,
                  /*extra_message=*/std::string());
  }
}

void NTPSnippetsFetcher::OnJsonError(const std::string& error) {
  LOG(WARNING) << "Received invalid JSON (" << error << "): "
               << last_fetch_json_;
  FetchFinished(
      OptionalSnippets(), FetchResult::JSON_PARSE_ERROR,
      /*extra_message=*/base::StringPrintf(" (error %s)", error.c_str()));
}

void NTPSnippetsFetcher::FetchFinished(OptionalSnippets snippets,
                                       FetchResult result,
                                       const std::string& extra_message) {
  DCHECK(result == FetchResult::SUCCESS || !snippets);
  last_status_ = FetchResultToString(result) + extra_message;

  UMA_HISTOGRAM_TIMES("NewTabPage.Snippets.FetchTime",
                      tick_clock_->NowTicks() - fetch_start_time_);
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Snippets.FetchResult",
                            static_cast<int>(result),
                            static_cast<int>(FetchResult::RESULT_MAX));

  if (!snippets_available_callback_.is_null())
    snippets_available_callback_.Run(std::move(snippets));
}

}  // namespace ntp_snippets
