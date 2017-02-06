// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

class SigninManagerBase;

namespace base {
class Value;
}  // namespace base

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace ntp_snippets {

// Fetches snippet data for the NTP from the server.
class NTPSnippetsFetcher : public OAuth2TokenService::Consumer,
                           public OAuth2TokenService::Observer,
                           public net::URLFetcherDelegate {
 public:
  // Callbacks for JSON parsing, needed because the parsing is platform-
  // dependent.
  using SuccessCallback = base::Callback<void(std::unique_ptr<base::Value>)>;
  using ErrorCallback = base::Callback<void(const std::string&)>;
  using ParseJSONCallback = base::Callback<
      void(const std::string&, const SuccessCallback&, const ErrorCallback&)>;

  using OptionalSnippets = base::Optional<NTPSnippet::PtrVector>;
  // |snippets| contains parsed snippets if a fetch succeeded. If problems
  // occur, |snippets| contains no value (no actual vector in base::Optional).
  // Error details can be retrieved using last_status().
  using SnippetsAvailableCallback =
      base::Callback<void(OptionalSnippets snippets)>;

  // Enumeration listing all possible outcomes for fetch attempts. Used for UMA
  // histograms, so do not change existing values. Insert new values at the end,
  // and update the histogram definition.
  enum class FetchResult {
    SUCCESS,
    EMPTY_HOSTS,
    URL_REQUEST_STATUS_ERROR,
    HTTP_ERROR,
    JSON_PARSE_ERROR,
    INVALID_SNIPPET_CONTENT_ERROR,
    OAUTH_TOKEN_ERROR,
    RESULT_MAX
  };

  // Enumeration listing all possible variants of dealing with personalization.
  enum class Personalization {
    kPersonal,
    kNonPersonal,
    kBoth
  };

  NTPSnippetsFetcher(
      SigninManagerBase* signin_manager,
      OAuth2TokenService* oauth2_token_service,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      const ParseJSONCallback& parse_json_callback,
      bool is_stable_channel);
  ~NTPSnippetsFetcher() override;

  // Set a callback that is called when a new set of snippets are downloaded,
  // overriding any previously set callback.
  void SetCallback(const SnippetsAvailableCallback& callback);

  // Fetches snippets from the server. |hosts| restricts the results to a set of
  // hosts, e.g. "www.google.com". An empty host set produces an error.
  //
  // If an ongoing fetch exists, it will be cancelled and a new one started,
  // without triggering an additional callback (i.e. not noticeable by
  // subscriber of SetCallback()).
  void FetchSnippetsFromHosts(const std::set<std::string>& hosts,
                              const std::string& language_code,
                              int count);

  // Debug string representing the status/result of the last fetch attempt.
  const std::string& last_status() const { return last_status_; }

  // Returns the last JSON fetched from the server.
  const std::string& last_json() const {
    return last_fetch_json_;
  }

  // Returns the personalization setting of the fetcher.
  Personalization personalization() const { return personalization_; }

  // Does the fetcher use host restrictions?
  bool UsesHostRestrictions() const;
  // Does the fetcher use authentication to get personalized results?
  bool UsesAuthentication() const;

  // Overrides internal clock for testing purposes.
  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock) {
    tick_clock_ = std::move(tick_clock);
  }

  void SetPersonalizationForTesting(Personalization personalization) {
    personalization_ = personalization;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsFetcherTest, BuildRequestAuthenticated);
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsFetcherTest, BuildRequestUnauthenticated);

  enum FetchAPI {
    CHROME_READER_API,
    CHROME_CONTENT_SUGGESTIONS_API,
  };

  struct RequestParams {
    FetchAPI fetch_api;
    std::string obfuscated_gaia_id;
    bool only_return_personalized_results;
    std::string user_locale;
    std::set<std::string> host_restricts;
    int count_to_fetch;

    RequestParams();
    ~RequestParams();

    std::string BuildRequest();
  };

  void FetchSnippetsImpl(const GURL& url,
                         const std::string& auth_header,
                         const std::string& request);
  void FetchSnippetsNonAuthenticated();
  void FetchSnippetsAuthenticated(const std::string& account_id,
                                  const std::string& oauth_access_token);
  void StartTokenRequest();

  // OAuth2TokenService::Consumer overrides:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // OAuth2TokenService::Observer overrides:
  void OnRefreshTokenAvailable(const std::string& account_id) override;

  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void OnJsonParsed(std::unique_ptr<base::Value> parsed);
  void OnJsonError(const std::string& error);
  void FetchFinished(OptionalSnippets snippets,
                     FetchResult result,
                     const std::string& extra_message);

  // Authorization for signed-in users.
  SigninManagerBase* signin_manager_;
  OAuth2TokenService* token_service_;
  std::unique_ptr<OAuth2TokenService::Request> oauth_request_;
  bool waiting_for_refresh_token_;

  // Holds the URL request context.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  const ParseJSONCallback parse_json_callback_;
  base::TimeTicks fetch_start_time_;
  std::string last_status_;
  std::string last_fetch_json_;

  // Hosts to restrict the snippets to.
  std::set<std::string> hosts_;

  // Count of snippets to fetch.
  int count_to_fetch_;

  // Language code to restrict to for personalized results.
  std::string locale_;

  // API endpoint for fetching snippets.
  const GURL fetch_url_;
  // Which API to use
  const FetchAPI fetch_api_;

  // The fetcher for downloading the snippets.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // The callback to notify when new snippets get fetched.
  SnippetsAvailableCallback snippets_available_callback_;

  // Flag for picking the right (stable/non-stable) API key for Chrome Reader.
  bool is_stable_channel_;

  // The variant of the fetching to use, loaded from variation parameters.
  Personalization personalization_;
  // Should we apply host restriction? It is loaded from variation parameters.
  bool use_host_restriction_;

  // Allow for an injectable tick clock for testing.
  std::unique_ptr<base::TickClock> tick_clock_;

  base::WeakPtrFactory<NTPSnippetsFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsFetcher);
};
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_
