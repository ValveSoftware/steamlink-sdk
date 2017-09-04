// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_IMPL_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_IMPL_H_

#include <stddef.h>

#include <list>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "base/synchronization/lock.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_backend.h"
#include "components/history/core/common/thumbnail_score.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class FilePath;
class RefCountedBytes;
class RefCountedMemory;
class SingleThreadTaskRunner;
}

namespace history {

class HistoryService;
class TopSitesCache;
class TopSitesImplTest;

// This class allows requests for most visited urls and thumbnails on any
// thread. All other methods must be invoked on the UI thread. All mutations
// to internal state happen on the UI thread and are scheduled to update the
// db using TopSitesBackend.
class TopSitesImpl : public TopSites, public HistoryServiceObserver {
 public:
  // Called to check whether an URL can be added to the history. Must be
  // callable multiple time and during the whole lifetime of TopSitesImpl.
  using CanAddURLToHistoryFn = base::Callback<bool(const GURL&)>;

  TopSitesImpl(PrefService* pref_service,
               HistoryService* history_service,
               const PrepopulatedPageList& prepopulated_pages,
               const CanAddURLToHistoryFn& can_add_url_to_history);

  // Initializes TopSitesImpl.
  void Init(const base::FilePath& db_name,
            const scoped_refptr<base::SingleThreadTaskRunner>& db_task_runner);

  // TopSites implementation.
  bool SetPageThumbnail(const GURL& url,
                        const gfx::Image& thumbnail,
                        const ThumbnailScore& score) override;
  bool SetPageThumbnailToJPEGBytes(const GURL& url,
                                   const base::RefCountedMemory* memory,
                                   const ThumbnailScore& score) override;
  void GetMostVisitedURLs(const GetMostVisitedURLsCallback& callback,
                          bool include_forced_urls) override;
  bool GetPageThumbnail(const GURL& url,
                        bool prefix_match,
                        scoped_refptr<base::RefCountedMemory>* bytes) override;
  bool GetPageThumbnailScore(const GURL& url, ThumbnailScore* score) override;
  bool GetTemporaryPageThumbnailScore(const GURL& url,
                                      ThumbnailScore* score) override;
  void SyncWithHistory() override;
  bool HasBlacklistedItems() const override;
  void AddBlacklistedURL(const GURL& url) override;
  void RemoveBlacklistedURL(const GURL& url) override;
  bool IsBlacklisted(const GURL& url) override;
  void ClearBlacklistedURLs() override;
  base::CancelableTaskTracker::TaskId StartQueryForMostVisited() override;
  bool IsKnownURL(const GURL& url) override;
  const std::string& GetCanonicalURLString(const GURL& url) const override;
  bool IsNonForcedFull() override;
  bool IsForcedFull() override;
  PrepopulatedPageList GetPrepopulatedPages() override;
  bool loaded() const override;
  bool AddForcedURL(const GURL& url, const base::Time& time) override;
  void OnNavigationCommitted(const GURL& url) override;

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

  // Register preferences used by TopSitesImpl.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  ~TopSitesImpl() override;

 private:
  // TODO(yiyaoliu): Remove the enums and related code when crbug/223430 is
  // fixed.
  // An enum representing different situations under which function
  // SetTopSites can be initiated.
  // This is needed because a histogram is used to record speed related metrics
  // when SetTopSites are initiated from OnGotMostVisitedThumbnails, which
  // usually happens early and might affect Chrome startup speed.
  enum CallLocation {
    // SetTopSites is called from function OnGotMostVisitedThumbnails.
    CALL_LOCATION_FROM_ON_GOT_MOST_VISITED_THUMBNAILS,
    // SetTopSites is called from AddForcedURLs.
    CALL_LOCATION_FROM_FORCED_URLS,
    // All other situations.
    CALL_LOCATION_FROM_OTHER_PLACES,
  };

  friend class TopSitesImplTest;
  FRIEND_TEST_ALL_PREFIXES(TopSitesImplTest, DiffMostVisited);
  FRIEND_TEST_ALL_PREFIXES(TopSitesImplTest, DiffMostVisitedWithForced);

  typedef base::Callback<void(const MostVisitedURLList&,
                              const MostVisitedURLList&)> PendingCallback;

  typedef std::pair<GURL, Images> TempImage;
  typedef std::list<TempImage> TempImages;
  typedef std::vector<PendingCallback> PendingCallbacks;

  // Generates the diff of things that happened between "old" and "new."
  //
  // This treats forced URLs separately than non-forced URLs.
  //
  // The URLs that are in "new" but not "old" will be have their index into
  // "new" put in |added_urls|. The non-forced URLs that are in "old" but not
  // "new" will have their index into "old" put into |deleted_urls|.
  //
  // URLs appearing in both old and new lists but having different indices will
  // have their index into "new" be put into |moved_urls|.
  static void DiffMostVisited(const MostVisitedURLList& old_list,
                              const MostVisitedURLList& new_list,
                              TopSitesDelta* delta);

  // Sets the thumbnail without writing to the database. Useful when
  // reading last known top sites from the DB.
  // Returns true if the thumbnail was set, false if the existing one is better.
  bool SetPageThumbnailNoDB(const GURL& url,
                            const base::RefCountedMemory* thumbnail_data,
                            const ThumbnailScore& score);

  // A version of SetPageThumbnail that takes RefCountedBytes as
  // returned by HistoryService.
  bool SetPageThumbnailEncoded(const GURL& url,
                               const base::RefCountedMemory* thumbnail,
                               const ThumbnailScore& score);

  // Encodes the bitmap to bytes for storage to the db. Returns true if the
  // bitmap was successfully encoded.
  static bool EncodeBitmap(const gfx::Image& bitmap,
                           scoped_refptr<base::RefCountedBytes>* bytes);

  // Removes the cached thumbnail for url. Does nothing if |url| if not cached
  // in |temp_images_|.
  void RemoveTemporaryThumbnailByURL(const GURL& url);

  // Add a thumbnail for an unknown url. See temp_thumbnails_map_.
  void AddTemporaryThumbnail(const GURL& url,
                             const base::RefCountedMemory* thumbnail,
                             const ThumbnailScore& score);

  // Called by our timer. Starts the query for the most visited sites.
  void TimerFired();

  // Finds the given URL in the redirect chain for the given TopSite, and
  // returns the distance from the destination in hops that the given URL is.
  // The URL is assumed to be in the list. The destination is 0.
  static int GetRedirectDistanceForURL(const MostVisitedURL& most_visited,
                                       const GURL& url);

  // Add prepopulated pages: 'welcome to Chrome' and themes gallery to |urls|.
  // Returns true if any pages were added.
  bool AddPrepopulatedPages(MostVisitedURLList* urls,
                            size_t num_forced_urls);

  // Add all the forced URLs from |cache_| into |new_list|, making sure not to
  // add any URL that's already in |new_list|'s non-forced URLs. The forced URLs
  // in |cache_| and |new_list| are assumed to appear at the front of the list
  // and be sorted in increasing |last_forced_time|. This will still be true
  // after the call. If the list of forced URLs overflows the older ones are
  // dropped. Returns the number of forced URLs after the merge.
  size_t MergeCachedForcedURLs(MostVisitedURLList* new_list);

  // Takes |urls|, produces it's copy in |out| after removing blacklisted URLs.
  // Also ensures we respect the maximum number of forced URLs and non-forced
  // URLs.
  void ApplyBlacklist(const MostVisitedURLList& urls, MostVisitedURLList* out);

  // Returns an MD5 hash of the URL. Hashing is required for blacklisted URLs.
  std::string GetURLHash(const GURL& url);

  // Returns the delay until the next update of history is needed.
  // Uses num_urls_changed
  base::TimeDelta GetUpdateDelay();

  // Updates URLs in |cache_| and the db (in the background).
  // The non-forced URLs in |new_top_sites| replace those in |cache_|.
  // The forced URLs of |new_top_sites| are merged with those in |cache_|,
  // if the list of forced URLs overflows, the oldest ones are dropped.
  // All mutations to cache_ *must* go through this. Should
  // be called from the UI thread.
  void SetTopSites(const MostVisitedURLList& new_top_sites,
                   const CallLocation location);

  // Returns the number of most visited results to request from history. This
  // changes depending upon how many urls have been blacklisted. Should be
  // called from the UI thread.
  int num_results_to_request_from_history() const;

  // Invoked when transitioning to LOADED. Notifies any queued up callbacks.
  // Should be called from the UI thread.
  void MoveStateToLoaded();

  void ResetThreadSafeCache();

  void ResetThreadSafeImageCache();

  // Stops and starts timer with a delay of |delta|.
  void RestartQueryForTopSitesTimer(base::TimeDelta delta);

  // Callback from TopSites with the top sites/thumbnails. Should be called
  // from the UI thread.
  void OnGotMostVisitedThumbnails(
      const scoped_refptr<MostVisitedThumbnails>& thumbnails);

  // Called when history service returns a list of top URLs.
  void OnTopSitesAvailableFromHistory(const MostVisitedURLList* data);

  // history::HistoryServiceObserver:
  void OnURLsDeleted(HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Ensures that non thread-safe methods are called on the correct thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<TopSitesBackend> backend_;

  // The top sites data.
  std::unique_ptr<TopSitesCache> cache_;

  // Copy of the top sites data that may be accessed on any thread (assuming
  // you hold |lock_|). The data in |thread_safe_cache_| has blacklisted and
  // pinned urls applied (|cache_| does not).
  std::unique_ptr<TopSitesCache> thread_safe_cache_;

  // Lock used to access |thread_safe_cache_|.
  mutable base::Lock lock_;

  // Task tracker for history and backend requests.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Timer that asks history for the top sites. This is used to make sure our
  // data stays in sync with history.
  base::OneShotTimer timer_;

  // The time we started |timer_| at. Only valid if |timer_| is running.
  base::TimeTicks timer_start_time_;

  // The number of URLs changed on the last update.
  size_t last_num_urls_changed_;

  // The pending requests for the top sites list. Can only be non-empty at
  // startup. After we read the top sites from the DB, we'll always have a
  // cached list and be able to run callbacks immediately.
  PendingCallbacks pending_callbacks_;

  // Stores thumbnails for unknown pages. When SetPageThumbnail is
  // called, if we don't know about that URL yet and we don't have
  // enough Top Sites (new profile), we store it until the next
  // SetNonForcedTopSites call.
  TempImages temp_images_;

  // URL List of prepopulated page.
  PrepopulatedPageList prepopulated_pages_;

  // PrefService holding the NTP URL blacklist dictionary. Must outlive
  // TopSitesImpl.
  PrefService* pref_service_;

  // HistoryService that TopSitesImpl can query. May be null, but if defined it
  // must outlive TopSitesImpl.
  HistoryService* history_service_;

  // Can URL be added to the history?
  CanAddURLToHistoryFn can_add_url_to_history_;

  // Are we loaded?
  bool loaded_;

  // Have the SetTopSites execution time related histograms been recorded?
  // The histogram should only be recorded once for each Chrome execution.
  static bool histogram_recorded_;

  ScopedObserver<HistoryService, HistoryServiceObserver>
      history_service_observer_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesImpl);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_IMPL_H_
