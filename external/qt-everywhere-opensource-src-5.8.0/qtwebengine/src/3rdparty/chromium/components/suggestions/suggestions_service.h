// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/sync_driver/sync_service_observer.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace gfx {
class Image;
}

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace sync_driver {
class SyncService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class OAuth2TokenService;
class SigninManagerBase;

namespace suggestions {

class BlacklistStore;
class ImageManager;
class SuggestionsStore;

// An interface to fetch server suggestions asynchronously.
class SuggestionsService : public KeyedService,
                           public net::URLFetcherDelegate,
                           public sync_driver::SyncServiceObserver {
 public:
  using ResponseCallback = base::Callback<void(const SuggestionsProfile&)>;
  using BitmapCallback = base::Callback<void(const GURL&, const gfx::Image&)>;

  using ResponseCallbackList =
      base::CallbackList<void(const SuggestionsProfile&)>;

  SuggestionsService(const SigninManagerBase* signin_manager,
                     OAuth2TokenService* token_service,
                     sync_driver::SyncService* sync_service,
                     net::URLRequestContextGetter* url_request_context,
                     std::unique_ptr<SuggestionsStore> suggestions_store,
                     std::unique_ptr<ImageManager> thumbnail_manager,
                     std::unique_ptr<BlacklistStore> blacklist_store);
  ~SuggestionsService() override;

  // Initiates a network request for suggestions if sync state allows and there
  // is no pending request. Returns true iff sync state allowed for a request,
  // whether a new request was actually sent or not.
  bool FetchSuggestionsData();

  // Returns the current set of suggestions from the cache.
  SuggestionsProfile GetSuggestionsDataFromCache() const;

  // Adds a callback that is called when the suggestions are updated.
  std::unique_ptr<ResponseCallbackList::Subscription> AddCallback(
      const ResponseCallback& callback) WARN_UNUSED_RESULT;

  // Retrieves stored thumbnail for website |url| asynchronously. Calls
  // |callback| with Bitmap pointer if found, and NULL otherwise.
  void GetPageThumbnail(const GURL& url, const BitmapCallback& callback);

  // A version of |GetPageThumbnail| that explicitly supplies the download URL
  // for the thumbnail. Replaces any pre-existing thumbnail URL with the
  // supplied one.
  void GetPageThumbnailWithURL(const GURL& url,
                               const GURL& thumbnail_url,
                               const BitmapCallback& callback);

  // Adds a URL to the blacklist cache, returning true on success or false on
  // failure. The URL will eventually be uploaded to the server.
  bool BlacklistURL(const GURL& candidate_url);

  // Removes a URL from the local blacklist, returning true on success or false
  // on failure.
  bool UndoBlacklistURL(const GURL& url);

  // Removes all URLs from the blacklist.
  void ClearBlacklist();

  // Determines which URL a blacklist request was for, irrespective of the
  // request's status. Returns false if |request| is not a blacklist request.
  static bool GetBlacklistedUrl(const net::URLFetcher& request, GURL* url);

  // Register SuggestionsService related prefs in the Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  friend class SuggestionsServiceTest;
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, FetchSuggestionsData);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest,
                           FetchSuggestionsDataSyncDisabled);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest,
                           FetchSuggestionsDataNoAccessToken);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest,
                           IssueRequestIfNoneOngoingError);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest,
                           IssueRequestIfNoneOngoingResponseNotOK);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, BlacklistURL);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, BlacklistURLRequestFails);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, ClearBlacklist);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, UndoBlacklistURL);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, GetBlacklistedUrl);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, UpdateBlacklistDelay);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, CheckDefaultTimeStamps);

  // Helpers to build the various suggestions URLs. These are static members
  // rather than local functions in the .cc file to make them accessible to
  // tests.
  static GURL BuildSuggestionsURL();
  static std::string BuildSuggestionsBlacklistURLPrefix();
  static GURL BuildSuggestionsBlacklistURL(const GURL& candidate_url);
  static GURL BuildSuggestionsBlacklistClearURL();

  // sync_driver::SyncServiceObserver implementation.
  void OnStateChanged() override;

  // Sets default timestamp for suggestions which do not have expiry timestamp.
  void SetDefaultExpiryTimestamp(SuggestionsProfile* suggestions,
                                 int64_t timestamp_usec);

  // Issues a network request if there isn't already one happening.
  void IssueRequestIfNoneOngoing(const GURL& url);

  // Issues a network request for suggestions (fetch, blacklist, or clear
  // blacklist, depending on |url|). |access_token| is used only if OAuth2
  // authentication is enabled.
  void IssueSuggestionsRequest(const GURL& url,
                               const std::string& access_token);

  // Creates a request to the suggestions service, properly setting headers.
  // If OAuth2 authentication is enabled, |access_token| should be a valid
  // OAuth2 access token, and will be written into an auth header.
  std::unique_ptr<net::URLFetcher> CreateSuggestionsRequest(
      const GURL& url,
      const std::string& access_token);

  // net::URLFetcherDelegate implementation.
  // Called when fetch request completes. Parses the received suggestions data,
  // and dispatches them to callbacks stored in queue.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // KeyedService implementation.
  void Shutdown() override;

  // Schedules a blacklisting request if the local blacklist isn't empty.
  void ScheduleBlacklistUpload();

  // If the local blacklist isn't empty, picks a URL from it and issues a
  // blacklist request for it.
  void UploadOneFromBlacklist();

  // Updates |scheduling_delay_| based on the success of the last request.
  void UpdateBlacklistDelay(bool last_request_successful);

  // Adds extra data to suggestions profile.
  void PopulateExtraData(SuggestionsProfile* suggestions);

  // Test seams.
  base::TimeDelta blacklist_delay() const { return scheduling_delay_; }
  void set_blacklist_delay(base::TimeDelta delay) {
    scheduling_delay_ = delay; }

  base::ThreadChecker thread_checker_;

  sync_driver::SyncService* sync_service_;
  ScopedObserver<sync_driver::SyncService, sync_driver::SyncServiceObserver>
      sync_service_observer_;

  net::URLRequestContextGetter* url_request_context_;

  // The cache for the suggestions.
  std::unique_ptr<SuggestionsStore> suggestions_store_;

  // Used to obtain server thumbnails, if available.
  std::unique_ptr<ImageManager> thumbnail_manager_;

  // The local cache for temporary blacklist, until uploaded to the server.
  std::unique_ptr<BlacklistStore> blacklist_store_;

  // Delay used when scheduling a blacklisting task.
  base::TimeDelta scheduling_delay_;

  // Helper for fetching OAuth2 access tokens.
  class AccessTokenFetcher;
  std::unique_ptr<AccessTokenFetcher> token_fetcher_;

  // Contains the current suggestions fetch request. Will only have a value
  // while a request is pending, and will be reset by |OnURLFetchComplete| or
  // if cancelled.
  std::unique_ptr<net::URLFetcher> pending_request_;

  // The start time of the previous suggestions request. This is used to measure
  // the latency of requests. Initially zero.
  base::TimeTicks last_request_started_time_;

  ResponseCallbackList callback_list_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<SuggestionsService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsService);
};

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
