// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_FETCHER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_FETCHER_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/remote/ntp_snippet.h"
#include "components/ntp_snippets/remote/request_throttler.h"
#include "components/translate/core/browser/language_model.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

class PrefService;
class SigninManagerBase;

namespace base {
class ListValue;
class Value;
}  // namespace base

namespace ntp_snippets {

class UserClassifier;

// TODO(tschumann): BuildArticleCategoryInfo() and BuildRemoteCategoryInfo()
// don't really belong into this library. However, as the snippets fetcher is
// providing this data for server-defined remote sections it's a good starting
// point. Candiates to add to such a library would be persisting categories
// (have all category managment in one place) or turning parsed JSON into
// FetchedCategory objects (all domain-specific logic in one place).

// Provides the CategoryInfo data for article suggestions. If |title| is
// nullopt, then the default, hard-coded title will be used.
CategoryInfo BuildArticleCategoryInfo(
    const base::Optional<base::string16>& title);

// Provides the CategoryInfo data for other remote suggestions.
CategoryInfo BuildRemoteCategoryInfo(const base::string16& title,
                                     bool allow_fetching_more_results);

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

  struct FetchedCategory {
    Category category;
    CategoryInfo info;
    NTPSnippet::PtrVector snippets;

    FetchedCategory(Category c, CategoryInfo&& info);
    FetchedCategory(FetchedCategory&&);             // = default, in .cc
    ~FetchedCategory();                             // = default, in .cc
    FetchedCategory& operator=(FetchedCategory&&);  // = default, in .cc
  };
  using FetchedCategoriesVector = std::vector<FetchedCategory>;
  using OptionalFetchedCategories = base::Optional<FetchedCategoriesVector>;

  // |snippets| contains parsed snippets if a fetch succeeded. If problems
  // occur, |snippets| contains no value (no actual vector in base::Optional).
  // Error details can be retrieved using last_status().
  using SnippetsAvailableCallback =
      base::OnceCallback<void(OptionalFetchedCategories fetched_categories)>;

  // Enumeration listing all possible outcomes for fetch attempts. Used for UMA
  // histograms, so do not change existing values. Insert new values at the end,
  // and update the histogram definition.
  enum class FetchResult {
    SUCCESS,
    DEPRECATED_EMPTY_HOSTS,
    URL_REQUEST_STATUS_ERROR,
    HTTP_ERROR,
    JSON_PARSE_ERROR,
    INVALID_SNIPPET_CONTENT_ERROR,
    OAUTH_TOKEN_ERROR,
    INTERACTIVE_QUOTA_ERROR,
    NON_INTERACTIVE_QUOTA_ERROR,
    RESULT_MAX
  };

  // Enumeration listing all possible variants of dealing with personalization.
  enum class Personalization {
    kPersonal,
    kNonPersonal,
    kBoth
  };

  // Contains all the parameters for one fetch.
  struct Params {
    Params();
    Params(const Params&);
    ~Params();

    // If non-empty, restricts the result to the given set of hosts.
    std::set<std::string> hosts;

    // BCP 47 language code specifying the user's UI language.
    std::string language_code;

    // A set of suggestion IDs that should not be returned again.
    std::set<std::string> excluded_ids;

    // Maximum number of snippets to fetch.
    int count_to_fetch = 0;

    // Whether this is an interactive request, i.e. triggered by an explicit
    // user action. Typically, non-interactive requests are subject to a daily
    // quota.
    bool interactive_request = false;

    // If set, only return results for this category.
    base::Optional<Category> exclusive_category;
  };

  NTPSnippetsFetcher(
      SigninManagerBase* signin_manager,
      OAuth2TokenService* token_service,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      PrefService* pref_service,
      CategoryFactory* category_factory,
      translate::LanguageModel* language_model,
      const ParseJSONCallback& parse_json_callback,
      const std::string& api_key,
      const UserClassifier* user_classifier);
  ~NTPSnippetsFetcher() override;

  // Initiates a fetch from the server. When done (successfully or not), calls
  // the subscriber of SetCallback().
  //
  // If an ongoing fetch exists, it will be silently abandoned and a new one
  // started, without triggering an additional callback (i.e. the callback will
  // only be called once).
  void FetchSnippets(const Params& params, SnippetsAvailableCallback callback);

  // Debug string representing the status/result of the last fetch attempt.
  const std::string& last_status() const { return last_status_; }

  // Returns the last JSON fetched from the server.
  const std::string& last_json() const {
    return last_fetch_json_;
  }

  // Returns the personalization setting of the fetcher.
  Personalization personalization() const { return personalization_; }

  // Returns the URL endpoint used by the fetcher.
  const GURL& fetch_url() const { return fetch_url_; }

  // Does the fetcher use authentication to get personalized results?
  bool UsesAuthentication() const;

  // Overrides internal clock for testing purposes.
  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock) {
    tick_clock_ = std::move(tick_clock);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsFetcherTest, BuildRequestAuthenticated);
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsFetcherTest, BuildRequestUnauthenticated);
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsFetcherTest, BuildRequestExcludedIds);
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsFetcherTest, BuildRequestNoUserClass);
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsFetcherTest,
                           BuildRequestWithTwoLanguages);
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsFetcherTest,
                           BuildRequestWithUILanguageOnly);
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsFetcherTest,
                           BuildRequestWithOtherLanguageOnly);

  enum FetchAPI {
    CHROME_READER_API,
    CHROME_CONTENT_SUGGESTIONS_API,
  };

  struct RequestBuilder {
    Params params;
    FetchAPI fetch_api = CHROME_READER_API;
    std::string obfuscated_gaia_id;
    bool only_return_personalized_results = false;
    std::string user_class;
    translate::LanguageModel::LanguageInfo ui_language;
    translate::LanguageModel::LanguageInfo other_top_language;

    RequestBuilder();
    RequestBuilder(RequestBuilder&&);
    ~RequestBuilder();

    std::string BuildRequest();
  };

  RequestBuilder MakeRequestBuilder() const;

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

  bool JsonToSnippets(const base::Value& parsed,
                      FetchedCategoriesVector* categories);
  void OnJsonParsed(std::unique_ptr<base::Value> parsed);
  void OnJsonError(const std::string& error);
  void FetchFinished(OptionalFetchedCategories fetched_categories,
                     FetchResult result,
                     const std::string& extra_message);
  void FilterCategories(FetchedCategoriesVector* categories);

  bool DemandQuotaForRequest(bool interactive_request);

  // Authentication for signed-in users.
  SigninManagerBase* signin_manager_;
  OAuth2TokenService* token_service_;
  std::unique_ptr<OAuth2TokenService::Request> oauth_request_;
  bool waiting_for_refresh_token_ = false;

  // When a token request gets canceled, we want to retry once.
  bool oauth_token_retried_ = false;

  // Holds the URL request context.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // Weak references, not owned.
  CategoryFactory* const category_factory_;
  translate::LanguageModel* const language_model_;

  const ParseJSONCallback parse_json_callback_;

  // API endpoint for fetching snippets.
  const GURL fetch_url_;
  // Which API to use
  const FetchAPI fetch_api_;

  // API key to use for non-authenticated requests.
  const std::string api_key_;

  // The variant of the fetching to use, loaded from variation parameters.
  Personalization personalization_;

  // Allow for an injectable tick clock for testing.
  std::unique_ptr<base::TickClock> tick_clock_;

  // Classifier that tells us how active the user is. Not owned.
  const UserClassifier* user_classifier_;

  // Request throttlers for limiting requests for different classes of users.
  RequestThrottler request_throttler_rare_ntp_user_;
  RequestThrottler request_throttler_active_ntp_user_;
  RequestThrottler request_throttler_active_suggestions_consumer_;

  // The callback to notify when new snippets get fetched.
  SnippetsAvailableCallback snippets_available_callback_;

  // The parameters for the current request.
  Params params_;

  // The fetcher for downloading the snippets. Only non-null if a fetch is
  // currently ongoing.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // When the current request was started, for logging purposes.
  base::TimeTicks fetch_start_time_;

  // Info on the last finished fetch.
  std::string last_status_;
  std::string last_fetch_json_;

  base::WeakPtrFactory<NTPSnippetsFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsFetcher);
};
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_FETCHER_H_
